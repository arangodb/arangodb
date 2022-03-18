#include "WasmModuleCollection.h"

#include <memory>
#include <utility>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "RestServer/UpgradeFeature.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"
#include "velocypack/Value.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/voc-types.h"
#include "WasmServer/WasmCommon.h"

namespace arangodb {
class LogicalCollection;
}
struct TRI_vocbase_t;

using namespace arangodb;
using namespace arangodb::wasm;
using namespace velocypack;

std::string const errorContext = "WasmModuleCollection";

WasmModuleCollection::WasmModuleCollection(TRI_vocbase_t& vocbase)
    : _vocbase{vocbase} {}

auto WasmModuleCollection::allNames() const -> ResultT<std::set<std::string>> {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), _collection,
      AccessMode::Type::READ);
  if (auto res = trx.begin(); res.fail()) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  errorContext + ": Could not start transaction: " +
                      std::move(res).errorMessage()};
  }

  arangodb::OperationResult result =
      trx.all(_collection, 0, 100, OperationOptions());

  if (auto res = trx.finish(result.result); res.fail()) {
    return Result{TRI_ERROR_BAD_PARAMETER, errorContext +
                                               ": Could not get documents: " +
                                               std::move(res).errorMessage()};
  }

  std::set<std::string> names;
  for (auto const& item : ArrayIterator(Slice(result.buffer->data()))) {
    names.emplace(item.get("name").copyString());
  }
  return ResultT<std::set<std::string>>(names);
}

auto WasmModuleCollection::get(ModuleName const& name) const
    -> ResultT<Module> {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), _collection,
      AccessMode::Type::READ);
  if (auto res = trx.begin(); res.fail()) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  errorContext + " Could not start transaction: " +
                      std::move(res).errorMessage()};
  }

  VPackBuilder search;
  {
    VPackObjectBuilder ob(&search);
    search.add(StaticStrings::KeyString, VPackValue(name.string));
  }
  arangodb::OperationResult result =
      trx.document(_collection, search.slice(), OperationOptions());

  if (auto res = trx.finish(result.result); res.fail()) {
    return Result{TRI_ERROR_BAD_PARAMETER, errorContext +
                                               " Could not get document: " +
                                               std::move(res).errorMessage()};
  }

  return velocypackToModule(Slice(result.buffer->data()));
}

auto WasmModuleCollection::add(wasm::Module const& module) -> Result {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), _collection,
      AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  if (auto res = trx.begin(); res.fail()) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  errorContext + " Could not start transaction: " +
                      std::move(res).errorMessage()};
  }

  VPackBuilder moduleAsVelocypack;
  moduleToVelocypack(module, moduleAsVelocypack, true);

  arangodb::OperationOptions opOptions;
  opOptions.waitForSync = false;
  opOptions.silent = true;
  opOptions.overwriteMode = OperationOptions::OverwriteMode::Replace;
  arangodb::OperationResult result =
      trx.insert(_collection, moduleAsVelocypack.slice(), opOptions);

  if (auto res = trx.finish(result.result); res.fail()) {
    return Result{TRI_ERROR_BAD_PARAMETER, errorContext +
                                               " Could not write module: " +
                                               std::move(res).errorMessage()};
  }
  return {};
}

auto WasmModuleCollection::remove(wasm::ModuleName const& name) -> Result {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), _collection,
      AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  if (auto res = trx.begin(); res.fail()) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  errorContext + " Could not start transaction: " +
                      std::move(res).errorMessage()};
  }

  VPackBuilder search;
  {
    VPackObjectBuilder ob(&search);
    search.add(StaticStrings::KeyString, VPackValue(name.string));
  }
  arangodb::OperationOptions opOptions;
  arangodb::OperationResult result =
      trx.remove(_collection, search.slice(), opOptions);

  if (auto res = trx.finish(result.result); res.fail()) {
    return Result{TRI_ERROR_BAD_PARAMETER, errorContext +
                                               " Could not remove module: " +
                                               std::move(res).errorMessage()};
  }
  return {};
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
