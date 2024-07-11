////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBSortedRowsStorageContext.h"

#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "RestServer/TemporaryStorageFeature.h"
#include "RocksDBEngine/Methods/RocksDBSstFileMethods.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBMethodsMemoryTracker.h"

#include <velocypack/Slice.h>

#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/slice.h>

namespace arangodb {

RocksDBSortedRowsStorageContext::RocksDBSortedRowsStorageContext(
    rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf, std::string const& path,
    uint64_t keyPrefix, StorageUsageTracker& usageTracker,
    RocksDBMethodsMemoryTracker& memoryTracker)
    : _db(db),
      _cf(cf),
      _path(path),
      _keyPrefix(keyPrefix),
      _usageTracker(usageTracker),
      _bytesWrittenToDir(0),
      _needsCleanup(false) {
  // build lower bound for iterator
  rocksutils::uintToPersistentBigEndian<std::uint64_t>(_lowerBoundPrefix,
                                                       _keyPrefix);
  _lowerBoundSlice = _lowerBoundPrefix;

  // build upper bound for iterator
  rocksutils::uintToPersistentBigEndian<std::uint64_t>(_upperBoundPrefix,
                                                       _keyPrefix + 1);
  _upperBoundSlice = _upperBoundPrefix;

  // create SstFileMethods instance that we will use for file ingestion
  rocksdb::Options options = _db->GetOptions();
  options.comparator = _cf->GetComparator();

  _methods = std::make_unique<RocksDBSstFileMethods>(
      _db, _cf, options, _path, usageTracker, memoryTracker);
}

RocksDBSortedRowsStorageContext::~RocksDBSortedRowsStorageContext() {
  try {
    cleanup();
  } catch (...) {
  }
}

Result RocksDBSortedRowsStorageContext::storeRow(RocksDBKey const& key,
                                                 velocypack::Slice data) {
  _needsCleanup = true;
  rocksdb::Status s = _methods->Put(
      _cf, key, {data.startAs<char const>(), data.byteSize()}, true);
  if (s.ok()) {
    return {};
  }
  return rocksutils::convertStatus(s);
}

void RocksDBSortedRowsStorageContext::ingestAll() {
  std::vector<std::string> fileNames;
  TRI_IF_FAILURE("failOnIngestAll1") {
    rocksdb::Status s = rocksdb::Status::Corruption("broken");
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  Result res = _methods->stealFileNames(fileNames);

  if (res.ok() && !fileNames.empty()) {
    rocksdb::IngestExternalFileOptions ingestOptions;
    ingestOptions.move_files = true;
    ingestOptions.failed_move_fall_back_to_copy = true;
    ingestOptions.snapshot_consistency = false;
    ingestOptions.write_global_seqno = false;
    ingestOptions.verify_checksums_before_ingest = false;

    rocksdb::Status s =
        _db->IngestExternalFile(_cf, fileNames, std::move(ingestOptions));

    TRI_IF_FAILURE("failOnIngestAll2") {
      s = rocksdb::Status::Corruption("broken");
      // not throw exception because the sst file methods don't have access
      // to the file names anymore, so clean up has to be called from here
      // with the file names
    }

    if (!s.ok()) {
      // an error occured. now let the SstFileMethods do the cleanup
      res.reset(rocksutils::convertStatus(s));
      _methods->cleanUpFiles(fileNames);
    } else {
      // everything went well. now we are responsible for any later cleanup
      _bytesWrittenToDir = _methods->stealBytesWrittenToDir();
    }
  }

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

std::unique_ptr<rocksdb::Iterator>
RocksDBSortedRowsStorageContext::getIterator() {
  std::string keyPrefix;
  rocksutils::uintToPersistentBigEndian<std::uint64_t>(keyPrefix, _keyPrefix);

  rocksdb::ReadOptions readOptions;
  readOptions.iterate_upper_bound = &_upperBoundSlice;
  readOptions.prefix_same_as_start = true;
  // this is ephemeral data, we write it once and then may iterate at most
  // once over it
  readOptions.verify_checksums = false;
  // as we are reading the data only once and then will get rid of it, there
  // is no need to populate the block cache with it
  readOptions.fill_cache = false;
  // note: RangeDeletes can safely be ignored when reading data, because
  // all keys are prefixed with a unique context id, and every operation
  // will only read its own keys (i.e. the keys with its own context id
  // prefix). and all RangeDeleted must be from a different context id.
  readOptions.ignore_range_deletions = true;
  // try to use readhead
  readOptions.adaptive_readahead = true;

  std::unique_ptr<rocksdb::Iterator> iterator(
      _db->NewIterator(readOptions, _cf));

  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "unable to create iterator for intermediate results");
  }

  iterator->Seek(keyPrefix);

  return iterator;
}

void RocksDBSortedRowsStorageContext::cleanup() {
  if (!_needsCleanup) {
    // nothing to be done
    return;
  }

  // first delete all files that only contain data for the range in
  // question
  rocksdb::Status s = rocksdb::DeleteFilesInRange(_db, _cf, &_lowerBoundSlice,
                                                  &_upperBoundSlice, false);

  if (s.ok()) {
    rocksdb::WriteOptions writeOptions;
    // this is just temporary data, which will be removed at restart.
    // no need to sync data to disk or to have a WAL.
    writeOptions.sync = false;
    writeOptions.disableWAL = true;

    // get rid of the remaining keys via a RangeDelete.
    // note: RangeDeletes can safely be ignored when reading data, because
    // all keys are prefixed with a unique context id, and every operation
    // will only read its own keys (i.e. the keys with its own context id
    // prefix)
    s = _db->DeleteRange(writeOptions, _cf, _lowerBoundSlice, _upperBoundSlice);
  }

  if (!s.ok()) {
    LOG_TOPIC("d1e84", WARN, Logger::ENGINES)
        << "failure during range deletion of intermediate results: "
        << rocksutils::convertStatus(s).errorMessage();
  }
  _needsCleanup = false;
  _usageTracker.decreaseUsage(_bytesWrittenToDir);
  _bytesWrittenToDir = 0;
}

bool RocksDBSortedRowsStorageContext::hasReachedMaxCapacity() {
  std::uint64_t maxCapacity = _usageTracker.maxCapacity();
  if (maxCapacity == 0) {
    return false;
  } else {
    return _usageTracker.currentUsage() >= maxCapacity;
  }
}

}  // namespace arangodb
