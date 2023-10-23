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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/MaintenanceActionExecutor.h"

#include "Cluster/ActionDescription.h"
#include "Cluster/CreateCollection.h"
#include "Cluster/EnsureIndex.h"
#include "Cluster/Maintenance.h"
#include "Cluster/UpdateCollection.h"
#include "Logger/LogMacros.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::replication2::replicated_state::document {
MaintenanceActionExecutor::MaintenanceActionExecutor(
    GlobalLogIdentifier gid, ServerID server,
    MaintenanceFeature& maintenanceFeature, TRI_vocbase_t& vocbase)
    : _gid(std::move(gid)),
      _maintenanceFeature(maintenanceFeature),
      _server(std::move(server)),
      _vocbase(vocbase) {}

auto MaintenanceActionExecutor::executeCreateCollection(
    ShardID const& shard, TRI_col_type_e collectionType,
    velocypack::SharedSlice const& properties) noexcept -> Result {
  std::shared_ptr<LogicalCollection> col;
  auto res = basics::catchToResult([&]() -> Result {
    OperationOptions options(ExecContext::current());
    return methods::Collections::createShard(
        _vocbase, options, shard, collectionType, properties.slice(), col);
  });

  LOG_TOPIC("ef1bc", DEBUG, Logger::MAINTENANCE)
      << "Local collection " << _vocbase.name() << "/" << shard << " "
      << (col ? "successful" : "failed") << " upon creation (gid: " << _gid
      << ") " << res;
  return res;
}

auto MaintenanceActionExecutor::executeDropCollection(
    std::shared_ptr<LogicalCollection> col) noexcept -> Result {
  auto res = basics::catchToResult(
      [&]() { return methods::Collections::drop(*col, false); });

  LOG_TOPIC("accd8", DEBUG, Logger::MAINTENANCE)
      << "Dropping local collection " << _vocbase.name() << "/" << col->name()
      << " (gid: " << _gid << "): " << res;

  return res;
}

auto MaintenanceActionExecutor::executeModifyCollection(
    std::shared_ptr<LogicalCollection> col, CollectionID colId,
    velocypack::SharedSlice properties) noexcept -> Result {
  auto res =
      basics::catchToResult([&col, properties = std::move(properties)]() {
        OperationOptions options(ExecContext::current());
        return methods::Collections::updateProperties(*col, properties.slice(),
                                                      options);
      });

  if (res.fail()) {
    if (auto storeErrorRes = basics::catchToResult([&]() {
          return _maintenanceFeature.storeShardError(_vocbase.name(), colId,
                                                     col->name(), _server, res);
        });
        storeErrorRes.fail()) {
      LOG_TOPIC("d3f2a", DEBUG, Logger::MAINTENANCE)
          << "Replicated log: " << _gid
          << " failed storeShardError call on shard " << col->name() << ": "
          << storeErrorRes;
    }
  }

  LOG_TOPIC("bffdd", DEBUG, Logger::MAINTENANCE)
      << "Modifying local collection " << _vocbase.name() << "/" << col->name()
      << " (gid: " << _gid << "): " << res;

  return res;
}

auto MaintenanceActionExecutor::executeCreateIndex(
    std::shared_ptr<LogicalCollection> col, velocypack::SharedSlice properties,
    std::shared_ptr<methods::Indexes::ProgressTracker> progress) noexcept
    -> Result {
  VPackBuilder output;
  auto res = basics::catchToResult([&]() {
    return methods::Indexes::ensureIndex(*col, properties.slice(), true, output,
                                         std::move(progress));
  });

  if (res.ok()) {
    std::ignore = basics::catchVoidToResult([&]() {
      arangodb::maintenance::EnsureIndex::indexCreationLogging(output.slice());
    });
  }

  LOG_TOPIC("eb458", DEBUG, Logger::MAINTENANCE)
      << "Creating index for " << _vocbase.name() << "/" << col->name()
      << " (gid: " << _gid << "): " << res;

  return res;
}

auto MaintenanceActionExecutor::executeDropIndex(
    std::shared_ptr<LogicalCollection> col,
    velocypack::SharedSlice index) noexcept -> Result {
  auto res = basics::catchToResult(
      [&]() { return methods::Indexes::drop(*col, index.slice()); });

  LOG_TOPIC("e155f", DEBUG, Logger::MAINTENANCE)
      << "Dropping local index " << index.toJson() << " of " << _vocbase.name()
      << "/" << col->name() << ": " << res;
  return res;
}

auto MaintenanceActionExecutor::addDirty() noexcept -> Result {
  auto res = basics::catchVoidToResult(
      [&]() { _maintenanceFeature.addDirty(_gid.database); });
  if (res.fail()) {
    LOG_TOPIC("d3f2a", DEBUG, Logger::MAINTENANCE)
        << "Replicated log: " << _gid << " failed addDirty call: " << res;
  }
  return res;
}
}  // namespace arangodb::replication2::replicated_state::document
