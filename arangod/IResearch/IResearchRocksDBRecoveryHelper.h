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

struct TRI_vocbase_t;

namespace arangodb {

class DatabaseFeature;
class LogicalCollection;
class RocksDBEngine;

namespace iresearch {

class IResearchLink;

class IResearchRocksDBRecoveryHelper : public RocksDBRecoveryHelper {
 public:
  IResearchRocksDBRecoveryHelper();

  virtual ~IResearchRocksDBRecoveryHelper() override;

  virtual void PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
                     const rocksdb::Slice& value) override;

  virtual void DeleteCF(uint32_t column_family_id,
                        const rocksdb::Slice& key) override;

  virtual void SingleDeleteCF(uint32_t column_family_id,
                              const rocksdb::Slice& key) override;

  virtual void LogData(const rocksdb::Slice& blob) override;

 private:
  std::pair<TRI_vocbase_t*, LogicalCollection*> lookupDatabaseAndCollection(
      uint64_t objectId) const;
  std::vector<IResearchLink*> lookupLinks(LogicalCollection* coll) const;

 private:
  DatabaseFeature* const _dbFeature;
  RocksDBEngine* const _engine;
  uint32_t const _documentCF;
};

}  // end namespace iresearch
}  // end namespace arangodb

#endif
