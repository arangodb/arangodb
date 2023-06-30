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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "StatePersistor.h"

#include <rocksdb/db.h>
#include <Inspection/VPack.h>

#include "Basics/RocksDBUtils.h"
#include "Replication2/Storage/RocksDB/AsyncLogWriteContext.h"
#include "Replication2/Storage/RocksDB/ReplicatedStateInfo.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBValue.h"

using namespace arangodb::replication2::replicated_state;

namespace arangodb::replication2::storage::rocksdb {

StatePersistor::StatePersistor(LogId logId, AsyncLogWriteContext& ctx,
                               ::rocksdb::DB* const db,
                               ::rocksdb::ColumnFamilyHandle* const metaCf)
    : logId(logId), ctx(ctx), db(db), metaCf(metaCf) {}

Result StatePersistor::updateMetadata(storage::PersistedStateInfo info) {
  TRI_ASSERT(info.stateId == logId);  // redundant information

  auto key = RocksDBKey{};
  key.constructReplicatedState(ctx.vocbaseId, logId);

  ReplicatedStateInfo rInfo;
  rInfo.dataSourceId = logId.id();
  rInfo.stateId = logId;
  rInfo.objectId = ctx.objectId;
  rInfo.state = std::move(info);

  VPackBuilder valueBuilder;
  velocypack::serialize(valueBuilder, rInfo);
  auto value = RocksDBValue::ReplicatedState(valueBuilder.slice());

  ::rocksdb::WriteOptions opts;
  auto s = db->GetRootDB()->Put(opts, metaCf, key.string(), value.string());

  return rocksutils::convertStatus(s);
}

ResultT<PersistedStateInfo> StatePersistor::readMetadata() {
  auto key = RocksDBKey{};
  key.constructReplicatedState(ctx.vocbaseId, logId);

  std::string value;
  auto s = db->GetRootDB()->Get(::rocksdb::ReadOptions{}, metaCf, key.string(),
                                &value);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  auto slice = VPackSlice(reinterpret_cast<uint8_t const*>(value.data()));
  auto info = velocypack::deserialize<ReplicatedStateInfo>(slice);

  TRI_ASSERT(info.stateId == logId);
  return std::move(info.state);
}

auto StatePersistor::drop() -> Result {
  auto key = RocksDBKey{};
  key.constructReplicatedState(ctx.vocbaseId, logId);
  ::rocksdb::WriteOptions opts;
  auto status = db->GetRootDB()->Delete(opts, metaCf, key.string());
  return rocksutils::convertStatus(status);
}

}  // namespace arangodb::replication2::storage::rocksdb
