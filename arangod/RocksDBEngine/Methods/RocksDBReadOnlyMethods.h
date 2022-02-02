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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/Methods/RocksDBReadOnlyBaseMethods.h"

namespace rocksdb {
class TransactionDB;
}  // namespace rocksdb

namespace arangodb {

// only implements GET and NewIterator
class RocksDBReadOnlyMethods final : public RocksDBReadOnlyBaseMethods {
 public:
  using RocksDBReadOnlyBaseMethods::RocksDBReadOnlyBaseMethods;

  Result beginTransaction() override;

  Result commitTransaction() override;

  Result abortTransaction() override;

  rocksdb::ReadOptions iteratorReadOptions() const override;

  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const& key,
                      rocksdb::PinnableSlice* val, ReadOwnWrites) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ColumnFamilyHandle*,
                                                 ReadOptionsCallback) override;
};

}  // namespace arangodb
