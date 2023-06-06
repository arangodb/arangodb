////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include <rocksdb/slice.h>
#include <rocksdb/types.h>

#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "RestServer/arangod.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/IndexId.h"
#include "IResearch/IResearchDataStore.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}  // namespace application_features

class DatabaseFeature;
class RocksDBEngine;

namespace iresearch {
class IResearchRocksDBInvertedIndex;
class IResearchRocksDBLink;

// recovery helper that replays/buffers all operations for links/indexes
// found during recovery
class IResearchRocksDBRecoveryHelper final : public RocksDBRecoveryHelper {
 public:
  explicit IResearchRocksDBRecoveryHelper(
      ArangodServer& server, std::span<std::string const> skipRecoveryItems);

  void prepare() final;
  void unprepare() noexcept final {
    *this = {};  // release all data
  }

  void PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
             const rocksdb::Slice& value, rocksdb::SequenceNumber tick) final;

  void DeleteCF(uint32_t column_family_id, const rocksdb::Slice& key,
                rocksdb::SequenceNumber tick) final;

  void SingleDeleteCF(uint32_t column_family_id, const rocksdb::Slice& key,
                      rocksdb::SequenceNumber tick) final {
    DeleteCF(column_family_id, key, tick);
  }

  void LogData(const rocksdb::Slice& blob, rocksdb::SequenceNumber tick) final;

 private:
  IResearchRocksDBRecoveryHelper() = default;

  template<bool NeedExists, typename Func>
  void applyCF(uint32_t column_family_id, rocksdb::Slice key,
               rocksdb::SequenceNumber tick, Func&& func);
  void skipAll(uint64_t objectId, rocksdb::SequenceNumber tick);

  static constexpr auto kMaxSize = std::numeric_limits<uint16_t>::max();

  struct Range {
    uint16_t begin{};
    uint16_t end{};

    bool empty() const noexcept { return begin == end; }
  };

  struct Ranges {
    Range indexes;
    Range links;

    bool empty() const noexcept { return indexes.empty() && links.empty(); }
  };

  Ranges& getRanges(uint64_t objectId);
  Ranges makeRanges(uint64_t objectId);
  std::shared_ptr<LogicalCollection> lookupCollection(uint64_t objectId);

  template<bool Force>
  void clear() noexcept;

  ArangodServer* _server{};
  RocksDBEngine* _engine{};
  DatabaseFeature* _dbFeature{};
  uint32_t _documentCF{};

  // skip recovery of all links/indexes
  bool _skipAllItems{false};
  // skip recovery of dedicated links/indexes
  // maps from collection name => index ids / index names
  containers::FlatHashMap<std::string_view,
                          containers::FlatHashSet<std::string_view>>
      _skipRecoveryItems;

  containers::FlatHashMap<uint64_t, Ranges> _ranges;
  std::vector<IResearchRocksDBInvertedIndex*> _indexes;
  std::vector<IResearchRocksDBLink*> _links;
};

}  // namespace iresearch
}  // namespace arangodb
