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
/// @author Daniel H. Larkin
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_COMMON_H
#define ARANGO_ROCKSDB_ROCKSDB_COMMON_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"

#include <rocksdb/status.h>

namespace arangodb {
class TransactionState;
class RocksDBTransactionState;
namespace transaction { class Methods; }
namespace rocksutils {

enum StatusHint { none, document, collection, view, index, database };

arangodb::Result convertStatus(rocksdb::Status const&,
                               StatusHint hint = StatusHint::none);

uint64_t uint64FromPersistent(char const* p);
void uint64ToPersistent(char* p, uint64_t value);
void uint64ToPersistent(std::string& out, uint64_t value);  
RocksDBTransactionState* toRocksTransactionState(transaction::Methods* trx);

}  // namespace rocksutils
}  // namespace arangodb

#endif
