////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBSstFileMethods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/RocksDBUtils.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabasePathFeature.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
namespace FU = basics::FileUtils;

RocksDBSstFileMethods::RocksDBSstFileMethods(
    bool isForeground, ArangodServer const& server, rocksdb::DB* rootDB,
    RocksDBTransactionCollection* trxColl, RocksDBIndex& ridx,
    rocksdb::Options const& dbOptions, std::string const& idxPath)
    : RocksDBMethods(),
      _isForeground(isForeground),
      _server(server),
      _rootDB(rootDB),
      _trxColl(trxColl),
      _ridx(ridx),
      _sstFileWriter(rocksdb::EnvOptions(dbOptions), dbOptions,
                     ridx.columnFamily()->GetComparator(), ridx.columnFamily()),
      _cf(ridx.columnFamily()),
      _idxPath(idxPath) {}

RocksDBSstFileMethods::~RocksDBSstFileMethods() { cleanUpFiles(); }

void RocksDBSstFileMethods::insertEstimators() {
  auto ops = _trxColl->stealTrackedIndexOperations();
  if (!ops.empty()) {
    TRI_ASSERT(_ridx.hasSelectivityEstimate() && ops.size() == 1);
    auto it = ops.begin();
    TRI_ASSERT(_ridx.id() == it->first);

    auto* estimator = _ridx.estimator();
    if (estimator != nullptr) {
      if (_isForeground) {
        estimator->insert(it->second.inserts);
      } else {
        uint64_t seq = _rootDB->GetLatestSequenceNumber();
        // since cuckoo estimator uses a map with seq as key we need to
        estimator->bufferUpdates(seq, std::move(it->second.inserts),
                                 std::move(it->second.removals));
      }
    }
  }
}

rocksdb::Status RocksDBSstFileMethods::writeToFile() {
  if (_sortedKeyValPairs.empty()) {
    return rocksdb::Status::OK();
  }

  auto comparator = _cf->GetComparator();
  std::sort(_sortedKeyValPairs.begin(), _sortedKeyValPairs.end(),
            [&comparator](auto& v1, auto& v2) {
              return comparator->Compare({v1.first}, {v2.first}) < 0;
            });
  TRI_pid_t pid = Thread::currentProcessId();
  std::string tmpFileName =
      std::to_string(pid) + '-' +
      std::to_string(RandomGenerator::interval(UINT32_MAX));
  std::string fileName = FU::buildFilename(_idxPath, tmpFileName);
  fileName += ".sst";
  rocksdb::Status res = _sstFileWriter.Open(fileName);
  if (res.ok()) {
    _bytesToWriteCount = 0;
    _sstFileNames.emplace_back(fileName);
    for (auto const& [key, val] : _sortedKeyValPairs) {
      _sstFileWriter.Put(rocksdb::Slice(key), rocksdb::Slice(val));
    }
    _sortedKeyValPairs.clear();
    rocksdb::Status res = _sstFileWriter.Finish();
    if (!res.ok()) {
      cleanUpFiles();
    } else {
      insertEstimators();
    }
  }
  return res;
}

rocksdb::Status RocksDBSstFileMethods::stealFileNames(
    std::vector<std::string>& fileNames) {
  rocksdb::Status res = writeToFile();
  if (res.ok()) {
    fileNames = std::move(_sstFileNames);
  }
  return res;
}

void RocksDBSstFileMethods::cleanUpFiles() {
  for (auto const& fileName : _sstFileNames) {
    TRI_UnlinkFile(fileName.data());
  }
  _sstFileNames.clear();
}

rocksdb::Status RocksDBSstFileMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                           rocksdb::Slice const& key,
                                           rocksdb::PinnableSlice* val,
                                           ReadOwnWrites) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "SstFileMethods does not provide Get");
}

rocksdb::Status RocksDBSstFileMethods::GetForUpdate(
    rocksdb::ColumnFamilyHandle* cf, rocksdb::Slice const& key,
    rocksdb::PinnableSlice* val) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "SstFileMethods does not provide GetForUpdate");
}

rocksdb::Status RocksDBSstFileMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                           RocksDBKey const& key,
                                           rocksdb::Slice const& val,
                                           bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  if (_cf == nullptr) {
    _cf = cf;
  }
  _sortedKeyValPairs.emplace_back(std::make_pair(
      key.string().ToString(), std::string(val.data(), val.size())));
  _bytesToWriteCount += key.size() + val.size();
  if (_bytesToWriteCount >= this->kMaxDataSize) {
    return writeToFile();
  }
  return rocksdb::Status::OK();
}

rocksdb::Status RocksDBSstFileMethods::PutUntracked(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key,
    rocksdb::Slice const& val) {
  return this->Put(cf, key, val, false);
}

rocksdb::Status RocksDBSstFileMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                              RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(false);
  return rocksdb::Status::NotSupported();
}

rocksdb::Status RocksDBSstFileMethods::SingleDelete(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key) {
  return this->Delete(cf, key);
}

void RocksDBSstFileMethods::PutLogData(rocksdb::Slice const& blob) {
  TRI_ASSERT(false);
}
