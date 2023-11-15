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
    MaintenanceFeature& maintenanceFeature, TRI_vocbase_t& vocbase,
    LoggerContext const& loggerContext)
    : _gid(std::move(gid)),
      _maintenanceFeature(maintenanceFeature),
      _server(std::move(server)),
      _vocbase(vocbase),
      _loggerContext(loggerContext.withTopic(Logger::MAINTENANCE)) {}

auto MaintenanceActionExecutor::executeCreateCollection(
    ShardID const& shard, TRI_col_type_e collectionType,
    velocypack::SharedSlice const& properties) noexcept -> Result {
  std::shared_ptr<LogicalCollection> col;
  auto res = basics::catchToResult([&]() -> Result {
    OperationOptions options(ExecContext::current());
    return methods::Collections::createShard(
        _vocbase, options, shard, collectionType, properties.slice(), col);
  });

  LOG_CTX("ef1bc", DEBUG, _loggerContext)
      << "Local collection " << _vocbase.name() << "/" << shard << " "
      << (col ? "successful" : "failed") << " upon creation: " << res;
  return res;
}

auto MaintenanceActionExecutor::executeDropCollection(
    std::shared_ptr<LogicalCollection> col) noexcept -> Result {
  auto res = basics::catchToResult(
      [&]() { return methods::Collections::drop(*col, false); });

  LOG_CTX("accd8", DEBUG, _loggerContext)
      << "Dropping local collection " << _vocbase.name() << "/" << col->name()
      << ": " << res;

  return res;
}

auto MaintenanceActionExecutor::executeModifyCollection(
    std::shared_ptr<LogicalCollection> col, CollectionID colId,
    velocypack::SharedSlice properties) noexcept -> Result {
  auto res =
      basics::catchToResult([&col, properties = std::move(properties)]() {
        OperationOptions options(ExecContext::current());
        return methods::Collections::updateProperties(*col, properties.slice(),
                                                      options)
            .get();
      });

  if (res.fail()) {
    if (auto storeErrorRes = basics::catchToResult([&]() {
          return _maintenanceFeature.storeShardError(_vocbase.name(), colId,
                                                     col->name(), _server, res);
        });
        storeErrorRes.fail()) {
      LOG_CTX("d0295", DEBUG, _loggerContext)
          << "Failed storeShardError call on shard " << col->name() << ": "
          << storeErrorRes;
    }
  }

  LOG_CTX("bffdd", DEBUG, _loggerContext)
      << "Modifying local collection " << _vocbase.name() << "/" << col->name()
      << ": " << res;

  return res;
}

auto MaintenanceActionExecutor::executeCreateIndex(
    std::shared_ptr<LogicalCollection> col, velocypack::SharedSlice properties,
    std::shared_ptr<methods::Indexes::ProgressTracker> progress) noexcept
    -> Result {
  VPackBuilder output;
  auto res = basics::catchToResult([&]() {
    return methods::Indexes::ensureIndex(*col, properties.slice(), true, output,
                                         std::move(progress))
        .get();
  });

  if (res.ok()) {
    std::ignore = basics::catchVoidToResult([&]() {
      arangodb::maintenance::EnsureIndex::indexCreationLogging(output.slice());
    });
  }

  LOG_CTX("eb458", DEBUG, _loggerContext)
      << "Creating index for " << _vocbase.name() << "/" << col->name() << ": "
      << res;

  return res;
}

auto MaintenanceActionExecutor::executeDropIndex(
    std::shared_ptr<LogicalCollection> col,
    velocypack::SharedSlice index) noexcept -> Result {
  auto res = basics::catchToResult(
      [&]() { return methods::Indexes::drop(*col, index.slice()).get(); });

  LOG_CTX("e155f", DEBUG, _loggerContext)
      << "Dropping local index " << index.toJson() << " of " << _vocbase.name()
      << "/" << col->name() << ": " << res;
  return res;
}

auto MaintenanceActionExecutor::addDirty() noexcept -> Result {
  auto res = basics::catchVoidToResult(
      [&]() { _maintenanceFeature.addDirty(_gid.database); });
  if (res.fail()) {
    LOG_CTX("d3f2a", DEBUG, _loggerContext) << "Failed addDirty call: " << res;
  }
  return res;
}
}  // namespace arangodb::replication2::replicated_state::document
