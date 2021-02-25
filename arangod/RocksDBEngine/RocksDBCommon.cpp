////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Daniel H. Larkin
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBCommon.h"
#include "Basics/Exceptions.h"
#include "Basics/RocksDBUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"

#include <rocksdb/comparator.h>
#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/transaction_db.h>
#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>

#include <initializer_list>

namespace arangodb {
namespace rocksutils {

void checkIteratorStatus(rocksdb::Iterator const* iterator) {
  auto s = iterator->status();
  if (!s.ok()) {
    THROW_ARANGO_EXCEPTION(arangodb::rocksutils::convertStatus(s));
  }
}

/// @brief count all keys in the given column family
std::size_t countKeys(rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);

  rocksdb::ReadOptions opts;
  opts.fill_cache = false;
  opts.total_order_seek = true;
  opts.verify_checksums = false;

  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(opts, cf));
  std::size_t count = 0;

  rocksdb::Slice lower(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
  it->Seek(lower);
  while (it->Valid()) {
    ++count;
    it->Next();
  }
  return count;
}

/// @brief iterate over all keys in range and count them
std::size_t countKeyRange(rocksdb::DB* db, RocksDBKeyBounds const& bounds,
                          bool prefix_same_as_start) {
  rocksdb::Slice lower(bounds.start());
  rocksdb::Slice upper(bounds.end());

  rocksdb::ReadOptions readOptions;
  readOptions.prefix_same_as_start = prefix_same_as_start;
  readOptions.iterate_upper_bound = &upper;
  readOptions.total_order_seek = !prefix_same_as_start;
  readOptions.verify_checksums = false;
  readOptions.fill_cache = false;

  rocksdb::ColumnFamilyHandle* cf = bounds.columnFamily();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(readOptions, cf));
  std::size_t count = 0;

  it->Seek(lower);
  while (it->Valid() && cmp->Compare(it->key(), upper) < 0) {
    ++count;
    it->Next();
  }
  return count;
}

/// @brief whether or not the specified range has keys
bool hasKeys(rocksdb::DB* db, RocksDBKeyBounds const& bounds, bool prefix_same_as_start) {
  rocksdb::Slice lower(bounds.start());
  rocksdb::Slice upper(bounds.end());

  rocksdb::ReadOptions readOptions;
  readOptions.prefix_same_as_start = prefix_same_as_start;
  readOptions.iterate_upper_bound = &upper;
  readOptions.total_order_seek = !prefix_same_as_start;
  readOptions.verify_checksums = false;
  readOptions.fill_cache = false;

  rocksdb::ColumnFamilyHandle* cf = bounds.columnFamily();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(readOptions, cf));

  it->Seek(lower);
  return (it->Valid() && cmp->Compare(it->key(), upper) < 0);
}

/// @brief helper method to remove large ranges of data
/// Should mainly be used to implement the drop() call
Result removeLargeRange(rocksdb::DB* db, RocksDBKeyBounds const& bounds,
                        bool prefixSameAsStart, bool useRangeDelete) {
  LOG_TOPIC("95aeb", DEBUG, Logger::ENGINES) << "removing large range: " << bounds;
  
  rocksdb::ColumnFamilyHandle* cf = bounds.columnFamily();
  rocksdb::DB* bDB = db->GetRootDB();
  TRI_ASSERT(bDB != nullptr);

  try {
    // delete files in range lower..upper
    rocksdb::Slice lower(bounds.start());
    rocksdb::Slice upper(bounds.end());
    {
      rocksdb::Status s = rocksdb::DeleteFilesInRange(bDB, cf, &lower, &upper);
      if (!s.ok()) {
        // if file deletion failed, we will still iterate over the remaining
        // keys, so we don't need to abort and raise an error here
        arangodb::Result r = rocksutils::convertStatus(s);
        LOG_TOPIC("60468", WARN, arangodb::Logger::ENGINES)
            << "RocksDB file deletion failed: " << r.errorMessage();
      }
    }

    // go on and delete the remaining keys (delete files in range does not
    // necessarily find them all, just complete files)
    if (useRangeDelete) {
      rocksdb::WriteOptions wo;
      rocksdb::Status s = bDB->DeleteRange(wo, cf, lower, upper);
      if (!s.ok()) {
        LOG_TOPIC("e7e1b", WARN, arangodb::Logger::ENGINES)
            << "RocksDB key deletion failed: " << s.ToString();
        return rocksutils::convertStatus(s);
      }
      return {};
    }

    // go on and delete the remaining keys (delete files in range does not
    // necessarily find them all, just complete files)
    rocksdb::ReadOptions readOptions;
    readOptions.iterate_upper_bound = &upper;
    readOptions.prefix_same_as_start = prefixSameAsStart;  // for edge index
    readOptions.total_order_seek = !prefixSameAsStart;
    readOptions.verify_checksums = false;
    readOptions.fill_cache = false;
    std::unique_ptr<rocksdb::Iterator> it(bDB->NewIterator(readOptions, cf));

    rocksdb::WriteOptions wo;

    rocksdb::Comparator const* cmp = cf->GetComparator();
    rocksdb::WriteBatch batch;

    size_t total = 0;
    size_t counter = 0;
    for (it->Seek(lower); it->Valid(); it->Next()) {
      TRI_ASSERT(cmp->Compare(it->key(), lower) > 0);
      TRI_ASSERT(cmp->Compare(it->key(), upper) < 0);
      ++total;
      ++counter;
      batch.Delete(cf, it->key());
      if (counter >= 1000) {
        LOG_TOPIC("8a358", DEBUG, Logger::ENGINES) << "intermediate delete write";
        // Persist deletes all 1000 documents
        rocksdb::Status status = bDB->Write(wo, &batch);
        if (!status.ok()) {
          LOG_TOPIC("18fe8", WARN, arangodb::Logger::ENGINES)
              << "RocksDB key deletion failed: " << status.ToString();
          return rocksutils::convertStatus(status);
        }
        batch.Clear();
        counter = 0;
      }
    }

    LOG_TOPIC("abf53", DEBUG, Logger::ENGINES)
        << "removing large range, deleted in total: " << total;

    if (counter > 0) {
      LOG_TOPIC("21187", DEBUG, Logger::ENGINES) << "intermediate delete write";
      // We still have sth to write
      // now apply deletion batch
      rocksdb::Status status = bDB->Write(rocksdb::WriteOptions(), &batch);

      if (!status.ok()) {
        LOG_TOPIC("ba426", WARN, arangodb::Logger::ENGINES)
            << "RocksDB key deletion failed: " << status.ToString();
        return rocksutils::convertStatus(status);
      }
    }

    return {};
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC("c26f2", ERR, arangodb::Logger::ENGINES)
        << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    LOG_TOPIC("dfd4b", ERR, arangodb::Logger::ENGINES)
        << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    LOG_TOPIC("16927", ERR, arangodb::Logger::ENGINES)
        << "caught unknown exception during RocksDB key prefix deletion";
    return Result(TRI_ERROR_INTERNAL,
                  "unknown exception during RocksDB key prefix deletion");
  }
}

Result compactAll(rocksdb::DB* db, bool changeLevel, bool compactBottomMostLevel) {
  rocksdb::CompactRangeOptions options;
  options.change_level = changeLevel;
  options.bottommost_level_compaction = compactBottomMostLevel ?
      rocksdb::BottommostLevelCompaction::kForceOptimized : 
      rocksdb::BottommostLevelCompaction::kIfHaveCompactionFilter;

  std::initializer_list<rocksdb::ColumnFamilyHandle*> const cfs = {
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Definitions),
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents),
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::PrimaryIndex),
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::EdgeIndex),
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::VPackIndex),
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::GeoIndex),
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::FulltextIndex),
  };

  LOG_TOPIC("d8a5d", INFO, arangodb::Logger::ENGINES)
      << "starting compaction of entire RocksDB database key range";

  for (auto* cf : cfs) {
    // compact the entire data range
    rocksdb::Status s = db->CompactRange(options, cf, nullptr, nullptr);
    if (!s.ok()) {
      Result res = rocksutils::convertStatus(s);
      LOG_TOPIC("e46a3", WARN, arangodb::Logger::ENGINES)
        << "compaction of entire RocksDB database key range failed: " << res.errorMessage();
      return res;
    }
  }
  LOG_TOPIC("65594", INFO, arangodb::Logger::ENGINES)
      << "compaction of entire RocksDB database key range finished";

  return {};
}

}  // namespace rocksutils
}  // namespace arangodb
