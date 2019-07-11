////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "VocBase/voc-types.h"

#include <velocypack/velocypack-aliases.h>
#include <set>

struct TRI_vocbase_t;

namespace arangodb {

class Result;
class DatabaseFeature;
class LogicalCollection;
class RocksDBEngine;

namespace iresearch {

class IResearchLink;

class IResearchRocksDBRecoveryHelper final : public RocksDBRecoveryHelper {
 public:
  struct IndexId {
    TRI_voc_tick_t db;
    TRI_voc_cid_t cid;
    TRI_idx_iid_t iid;

    IndexId(TRI_voc_tick_t db, TRI_voc_cid_t cid, TRI_idx_iid_t iid) noexcept
        : db(db), cid(cid), iid(iid) {}

    bool operator<(IndexId const& rhs) const noexcept {
      return db < rhs.db && cid < rhs.cid && iid < rhs.iid;
    }
  };

  IResearchRocksDBRecoveryHelper() = default;

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

  virtual void LogData(const rocksdb::Slice& blob) override;

 private:
  void handleDeleteCF(uint32_t column_family_id,
                      const rocksdb::Slice& key,
                      rocksdb::SequenceNumber tick);

  std::set<IndexId> _recoveredIndexes;  // set of already recovered indexes
  DatabaseFeature* _dbFeature{};
  RocksDBEngine* _engine{};
  uint32_t _documentCF{};
};

}  // end namespace iresearch
}  // end namespace arangodb

#endif
