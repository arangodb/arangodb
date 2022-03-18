#include "WasmModuleCollection.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryResult.h"
#include "Aql/QueryString.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "RestServer/UpgradeFeature.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"
#include "velocypack/Value.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"
#include "velocypack/Buffer.h"
#include "WasmServer/WasmCommon.h"

namespace arangodb {
class LocalDocumentId;
}

using namespace arangodb;
using namespace arangodb::wasm;
using namespace velocypack;

std::string const errorContext = "WasmModuleCollection";

WasmModuleCollection::WasmModuleCollection(TRI_vocbase_t& vocbase)
    : _vocbase{vocbase} {}

auto WasmModuleCollection::allNames() const
    -> ResultT<std::vector<ModuleName>> {
  std::string const aql("FOR l IN " + _collection + " RETURN l._key");
  auto query = arangodb::aql::Query::create(
      transaction::StandaloneContext::Create(_vocbase),
      arangodb::aql::QueryString(aql), nullptr);

  aql::QueryResult queryResult = query->executeSync();

  if (queryResult.result.fail()) {
    return Result{TRI_ERROR_INTERNAL,
                  errorContext + " Could not get all keys: " +
                      queryResult.result.errorMessage().data()};
  }

  std::vector<ModuleName> names;
  for (auto const& item :
       velocypack::ArrayIterator(queryResult.data->slice())) {
    names.emplace_back(ModuleName{item.copyString()});
  }

  return ResultT<std::vector<ModuleName>>(names);
}

auto WasmModuleCollection::getFromDBServer(ModuleName const& name) const
    -> ResultT<Module> {
  auto& clusterInfo =
      _vocbase.server().getFeature<ClusterFeature>().clusterInfo();
  auto logicalCollectionPtr =
      clusterInfo.getCollection(_vocbase.name(), _collection);
  auto shardList =
      clusterInfo.getShardList(std::to_string(logicalCollectionPtr->id().id()));
  if (shardList->size() != 1) {
    return Result{
        TRI_ERROR_INTERNAL,
        errorContext +
            " Could not find shard for satellite collection on this server."};
  }
  auto collectionName = shardList->at(0);

  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), _collection,
      AccessMode::Type::READ);
  if (auto res = trx.begin(); res.fail()) {
    return Result{TRI_ERROR_INTERNAL, errorContext +
                                          " Could not start transaction: " +
                                          res.errorMessage().data()};
  }
  VPackBuilder builder;
  auto result = trx.documentFastPathLocal(
      collectionName, name.string,
      [&builder](LocalDocumentId const& id, velocypack::Slice slice) {
        builder.add(slice);
        return true;
      });
  if (auto res = trx.finish(result); res.fail()) {
    return Result{
        TRI_ERROR_BAD_PARAMETER,
        errorContext + " Could not get document: " + res.errorMessage().data()};
  }
  return velocypackToModule(builder.slice());
}

auto WasmModuleCollection::get(ModuleName const& name) const
    -> ResultT<Module> {
  if (ServerState::instance()->isDBServer()) {
    return getFromDBServer(name);
  }

  VPackBuilder search;
  {
    VPackObjectBuilder ob(&search);
    search.add(StaticStrings::KeyString, VPackValue(name.string));
  }
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), _collection,
      AccessMode::Type::READ);
  if (auto res = trx.begin(); res.fail()) {
    return Result{TRI_ERROR_INTERNAL, errorContext +
                                          " Could not start transaction: " +
                                          res.errorMessage().data()};
  }
  return trx.documentAsync(_collection, search.slice(), OperationOptions())
      .thenValue([&trx](OperationResult opRes) {
        return trx.finishAsync(opRes.result)
            .thenValue(
                [opRes(std::move(opRes))](Result result) -> ResultT<Module> {
                  if (result.fail()) {
                    return Result{TRI_ERROR_BAD_PARAMETER,
                                  errorContext + " Could not get document: " +
                                      result.errorMessage().data()};
                  }
                  return velocypackToModule(Slice(opRes.buffer->data()));
                });
      })
      .get();
}

auto WasmModuleCollection::add(wasm::Module const& module) -> Result {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), _collection,
      AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  if (auto res = trx.begin(); res.fail()) {
    return Result{TRI_ERROR_INTERNAL, errorContext +
                                          " Could not start transaction: " +
                                          res.errorMessage().data()};
  }

  VPackBuilder moduleAsVelocypack;
  moduleToVelocypack(module, moduleAsVelocypack, true);

  arangodb::OperationOptions opOptions;
  opOptions.waitForSync = false;
  opOptions.silent = true;
  opOptions.overwriteMode = OperationOptions::OverwriteMode::Replace;
  return trx.insertAsync(_collection, moduleAsVelocypack.slice(), opOptions)
      .thenValue([&trx](OperationResult opRes) {
        return trx.finishAsync(opRes.result)
            .thenValue([opRes(std::move(opRes))](Result result) -> Result {
              if (result.fail()) {
                return Result{TRI_ERROR_BAD_PARAMETER,
                              errorContext + " Could not write module: " +
                                  result.errorMessage().data()};
              }
              return {};
            });
      })
      .get();
}

auto WasmModuleCollection::remove(wasm::ModuleName const& name) -> Result {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), _collection,
      AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  if (auto res = trx.begin(); res.fail()) {
    return Result{TRI_ERROR_INTERNAL, errorContext +
                                          " Could not start transaction: " +
                                          res.errorMessage().data()};
  }

  VPackBuilder search;
  {
    VPackObjectBuilder ob(&search);
    search.add(StaticStrings::KeyString, VPackValue(name.string));
  }
  return trx.removeAsync(_collection, search.slice(), OperationOptions())
      .thenValue([&trx](OperationResult opRes) {
        return trx.finishAsync(opRes.result)
            .thenValue([opRes(std::move(opRes))](Result result) -> Result {
              if (result.fail()) {
                return Result{TRI_ERROR_BAD_PARAMETER,
                              errorContext + " Could not remove module: " +
                                  result.errorMessage().data()};
              }
              return {};
            });
      })
      .get();
}

bool upgradeWasmModuleSystemCollection(
    TRI_vocbase_t& vocbase, velocypack::Slice const& /*upgradeParams*/) {
  VPackBuilder collectionProperties;
  {
    VPackObjectBuilder builder(&collectionProperties);
    collectionProperties.add(StaticStrings::ReplicationFactor,
                             VPackValue(StaticStrings::Satellite));
  }
  auto collections = std::shared_ptr<LogicalCollection>();
  auto result = methods::Collections::create(
      vocbase, OperationOptions(), "wasmModules", TRI_COL_TYPE_DOCUMENT,
      collectionProperties.slice(),
      true,  // createWaitsForSyncReplication
      true,  // enforceReplicationFactor
      true,  // isNewDatabase
      collections,
      false  // allow system collection creation
  );
  return true;
}

void arangodb::registerWasmModuleCollectionUpgradeTask(ArangodServer& server) {
  if (!server.hasFeature<UpgradeFeature>()) {
    return;
  }
  auto& upgrade = server.getFeature<UpgradeFeature>();

  {
    methods::Upgrade::Task task;
    task.name = "upgradeWasm";
    task.description =
        "ensure creation of satellite collection for wasm modules";
    task.systemFlag = methods::Upgrade::Flags::DATABASE_ALL;
    task.clusterFlags = methods::Upgrade::Flags::CLUSTER_COORDINATOR_GLOBAL |
                        methods::Upgrade::Flags::CLUSTER_NONE;
    task.databaseFlags = methods::Upgrade::Flags::DATABASE_UPGRADE |
                         methods::Upgrade::Flags::DATABASE_EXISTING |
                         methods::Upgrade::Flags::DATABASE_INIT |
                         methods::Upgrade::Flags::DATABASE_ONLY_ONCE;
    task.action = &upgradeWasmModuleSystemCollection;
    upgrade.addTask(std::move(task));
  }
}
