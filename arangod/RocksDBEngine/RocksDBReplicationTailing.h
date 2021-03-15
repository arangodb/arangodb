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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_REPLICATION_TAILING_H
#define ARANGO_ROCKSDB_ROCKSDB_REPLICATION_TAILING_H 1

#include "Basics/Common.h"
#include "Replication/common-defines.h"
#include "RocksDBEngine/RocksDBReplicationCommon.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace rocksutils {

// iterates over WAL starting at 'from' and returns up to 'limit' documents
// from the corresponding database; releases dumping resources
RocksDBReplicationResult tailWal(TRI_vocbase_t* vocbase, uint64_t tickStart,
                                 uint64_t tickEnd, size_t chunkSize,
                                 bool includeSystem, DataSourceId collectionId,
                                 arangodb::velocypack::Builder& builder);

arangodb::TRI_replication_operation_e convertLogType(RocksDBLogType t);

}  // namespace rocksutils
}  // namespace arangodb

#endif
