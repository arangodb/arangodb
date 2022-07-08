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

#include "RocksDBTempStorage.h"

#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBSortedRowsStorageContext.h"

#include <absl/strings/str_cat.h>

#include <rocksdb/comparator.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>

using namespace arangodb;

namespace {

// TODO: rename it
class TwoPartComparator : public rocksdb::Comparator {
 public:
  TwoPartComparator() = default;
  int Compare(rocksdb::Slice const& lhs,
              rocksdb::Slice const& rhs) const override {
    int diff = memcmp(lhs.data(), rhs.data(), sizeof(uint64_t));
    if (diff != 0) {
      return diff;
    }
    auto p1 = lhs.data() + sizeof(uint64_t);
    auto p2 = rhs.data() + sizeof(uint64_t);

    int diffInId = 0;
    bool hasPrefixId1 =
        (static_cast<size_t>(p1 - lhs.data()) < lhs.size()) ? true : false;
    bool hasPrefixId2 =
        (static_cast<size_t>(p2 - rhs.data()) < rhs.size()) ? true : false;
    if (hasPrefixId1 && hasPrefixId2) {
      diffInId = memcmp(p1, p2, sizeof(uint64_t));
      p1 += sizeof(uint64_t);
      p2 += sizeof(uint64_t);
    } else if (hasPrefixId1) {
      diffInId = 1;
    } else if (hasPrefixId2) {
      diffInId = -1;
    }
    while (static_cast<size_t>(p1 - lhs.data()) < lhs.size() &&
           static_cast<size_t>(p2 - rhs.data()) < rhs.size()) {
      arangodb::velocypack::Slice slice1(reinterpret_cast<uint8_t const*>(p1));
      p1 += slice1.byteSize();
      arangodb::velocypack::Slice slice2(reinterpret_cast<uint8_t const*>(p2));
      p2 += slice2.byteSize();
      char order1 = *p1;
      char order2 = *p2;
      TRI_ASSERT(order1 == order2);

      diff = basics::VelocyPackHelper::compare(slice1, slice2, true);

      if (diff != 0) {
        return (order1 == '1') ? diff : -diff;
      }
      ++p1;
      ++p2;
    }

    return diffInId;
  }

  // Ignore the following methods for now:
  const char* Name() const override { return "TwoPartComparator"; }
  void FindShortestSeparator(std::string*,
                             rocksdb::Slice const&) const override {}
  void FindShortSuccessor(std::string*) const override {}
};

}  // namespace

RocksDBTempStorage::RocksDBTempStorage(std::string const& basePath)
    : _basePath(basePath),
      _nextId(0),
      _db(nullptr),
      _comparator(std::make_unique<::TwoPartComparator>()) {}

RocksDBTempStorage::~RocksDBTempStorage() = default;

Result RocksDBTempStorage::init() {
  _tempFilesPath = basics::FileUtils::buildFilename(_basePath, "temp");

  {
    std::string systemErrorStr;
    long errorNo;

    auto res = TRI_CreateRecursiveDirectory(_tempFilesPath.c_str(), errorNo,
                                            systemErrorStr);

    if (res != TRI_ERROR_NO_ERROR) {
      return {
          TRI_ERROR_FAILED,
          absl::StrCat("cannot create directory for intermediate results ('",
                       _tempFilesPath, "'): ", systemErrorStr)};
    }
  }

  rocksdb::DBOptions options;
  options.create_missing_column_families = true;
  options.create_if_missing = true;
  options.env = rocksdb::Env::Default();

#ifdef USE_ENTERPRISE
// TODO: fix Env override for encryption!
// rocksDBEngine.configureEnterpriseRocksDBOptions(_options, true);
#endif

  std::vector<rocksdb::ColumnFamilyDescriptor> columnFamilies;

  rocksdb::ColumnFamilyOptions cfOptions;
  cfOptions.comparator = _comparator.get();
  cfOptions.prefix_extractor.reset(
      rocksdb::NewFixedPrefixTransform(sizeof(uint64_t)));

  columnFamilies.emplace_back(
      rocksdb::ColumnFamilyDescriptor("SortCF", cfOptions));
  columnFamilies.emplace_back(rocksdb::ColumnFamilyDescriptor(
      rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()));

  std::string rocksDBPath =
      basics::FileUtils::buildFilename(_basePath, "rocksdb");

  rocksdb::Status s = rocksdb::DB::Open(options, rocksDBPath, columnFamilies,
                                        &_cfHandles, &_db);

  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

void RocksDBTempStorage::close() {
  TRI_ASSERT(_db != nullptr);
  _db->Close();
}

std::unique_ptr<RocksDBSortedRowsStorageContext>
RocksDBTempStorage::getSortedRowsStorageContext() {
  return std::make_unique<RocksDBSortedRowsStorageContext>(
      _db, _cfHandles[0], _tempFilesPath, nextId());
}

uint64_t RocksDBTempStorage::nextId() noexcept {
  return _nextId.fetch_add(1, std::memory_order_relaxed) + 1;
}
