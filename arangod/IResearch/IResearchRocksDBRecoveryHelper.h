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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH_ROCKSDB_RECOVERY_HELPER_H
#define ARANGOD_IRESEARCH_ROCKSDB_RECOVERY_HELPER_H 1

#include <cstdint>
#include <set>

#include <rocksdb/slice.h>
#include <rocksdb/types.h>

#include "Basics/Identifier.h"
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class DatabaseFeature;
class RocksDBEngine;

namespace iresearch {

class IResearchRocksDBRecoveryHelper final : public RocksDBRecoveryHelper {
 public:
  struct IndexId {
    TRI_voc_tick_t db;
    DataSourceId cid;
    arangodb::IndexId iid;

    IndexId(TRI_voc_tick_t db, DataSourceId cid, arangodb::IndexId iid) noexcept
        : db(db), cid(cid), iid(iid) {}

    bool operator<(IndexId const& rhs) const noexcept {
      return (db < rhs.db) ||
               (db == rhs.db &&
                  (cid < rhs.cid ||
                    (cid == rhs.cid &&  iid < rhs.iid)));
    }
  };

  explicit IResearchRocksDBRecoveryHelper(application_features::ApplicationServer&);

  virtual ~IResearchRocksDBRecoveryHelper() override = default;

  virtual void prepare() override;

  virtual void PutCF(uint32_t column_family_id,
                     const rocksdb::Slice& key,
                     const rocksdb::Slice& value,
                     rocksdb::SequenceNumber tick) override;

  virtual void DeleteCF(uint32_t column_family_id,
                        const rocksdb::Slice& key,
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

 private:
  void handleDeleteCF(uint32_t column_family_id,
                      const rocksdb::Slice& key,
                      rocksdb::SequenceNumber tick);

  application_features::ApplicationServer& _server;
  std::set<IndexId> _recoveredIndexes;  // set of already recovered indexes
  DatabaseFeature* _dbFeature{};
  RocksDBEngine* _engine{};
  uint32_t _documentCF{};
};

}  // end namespace iresearch
}  // end namespace arangodb

#endif
