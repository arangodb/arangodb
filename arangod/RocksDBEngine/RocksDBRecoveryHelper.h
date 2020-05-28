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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_RECOVERY_HELPER_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_RECOVERY_HELPER_H 1

#include "Basics/Common.h"

#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/types.h>

namespace arangodb {

class RocksDBRecoveryHelper {
 public:
  virtual ~RocksDBRecoveryHelper() = default;

  virtual void prepare() {}

  virtual void PutCF(uint32_t column_family_id,
                     const rocksdb::Slice& key,
                     const rocksdb::Slice& value,
                     rocksdb::SequenceNumber tick) {
  }

  virtual void DeleteCF(uint32_t column_family_id,
                        const rocksdb::Slice& key,
                        rocksdb::SequenceNumber tick) {
  }

  virtual void SingleDeleteCF(uint32_t column_family_id,
                              const rocksdb::Slice& key,
                              rocksdb::SequenceNumber tick) {
  }

  virtual void DeleteRangeCF(uint32_t column_family_id,
                             const rocksdb::Slice& begin_key,
                             const rocksdb::Slice& end_key,
                             rocksdb::SequenceNumber tick) {
  }

  virtual void LogData(const rocksdb::Slice& blob,
                       rocksdb::SequenceNumber tick) {
  }
};

}  // end namespace arangodb

#endif
