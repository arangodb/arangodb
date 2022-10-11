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

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class DatabaseFeature;
class RocksDBEngine;

namespace iresearch {

// recovery helper that replays/buffers all operations for links/indexes
// found during recovery
class IResearchRocksDBRecoveryHelper final : public RocksDBRecoveryHelper {
 public:
  explicit IResearchRocksDBRecoveryHelper(
      ArangodServer&, std::span<std::string const> skipRecoveryItems);

  virtual void prepare() override;

  virtual void PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
                     const rocksdb::Slice& value,
                     rocksdb::SequenceNumber tick) override;

  virtual void DeleteCF(uint32_t column_family_id, const rocksdb::Slice& key,
                        rocksdb::SequenceNumber tick) override {
    handleDeleteCF(column_family_id, key, tick);
  }

  virtual void SingleDeleteCF(uint32_t column_family_id,
                              const rocksdb::Slice& key,
                              rocksdb::SequenceNumber tick) override {
    handleDeleteCF(column_family_id, key, tick);
  }

  virtual void LogData(const rocksdb::Slice& blob,
                       rocksdb::SequenceNumber tick) override;

  virtual bool wasSkipped(IndexId id) const noexcept override {
    return _skippedIndexes.contains(id);
  }

 private:
  void handleDeleteCF(uint32_t column_family_id, const rocksdb::Slice& key,
                      rocksdb::SequenceNumber tick);

  using LinkContainer =
      containers::SmallVector<std::pair<std::shared_ptr<arangodb::Index>, bool>,
                              8>;

  bool lookupLinks(LinkContainer& result,
                   arangodb::LogicalCollection& coll) const;

 protected:
  ArangodServer& _server;
  DatabaseFeature* _dbFeature{};
  RocksDBEngine* _engine{};
  uint32_t _documentCF{};

  // skip recovery of all links/indexes
  bool _skipAllItems;

  // skip recovery of dedicated links/indexes
  // maps from collection name => index ids / index names
  containers::FlatHashMap<std::string, containers::FlatHashSet<std::string>>
      _skipRecoveryItems;

  containers::FlatHashSet<IndexId> _skippedIndexes;
};

}  // end namespace iresearch
}  // end namespace arangodb
