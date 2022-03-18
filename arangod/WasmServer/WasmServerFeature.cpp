#include "WasmServerFeature.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Result.h"
#include "Basics/application-exit.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "WasmServer/Wasm3cpp.h"
#include "WasmServer/WasmCommon.h"
#include "WasmServer/WasmModuleCollection.h"

namespace arangodb {
namespace application_features {
class CommunicationFeaturePhase;
class DatabaseFeaturePhase;
}  // namespace application_features
}  // namespace arangodb

using namespace arangodb;
using namespace arangodb::wasm;

WasmServerFeature::WasmServerFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  // TODO: check when to start WasmServerFeature
  startsAfter<CommunicationFeaturePhase>();
  startsBefore<DatabaseFeaturePhase>();
}

void WasmServerFeature::prepare() {
  setEnabled(true);
  // if (ServerState::instance()->isCoordinator() or
  //     ServerState::instance()->isDBServer()) {
  //   setEnabled(true);
  // } else {
  //   setEnabled(false);
  // }
}

void WasmServerFeature::start() {
  auto vocbase = server().getFeature<SystemDatabaseFeature>().use();
  if (!vocbase) {
    LOG_TOPIC("4bcfc", FATAL, Logger::WASM) << "could not get vocbase";
    FATAL_ERROR_EXIT();
    return;
  }
  _wasmModuleCollection = std::make_unique<WasmModuleCollection>(*vocbase);
  arangodb::registerWasmModuleCollectionUpgradeTask(server());
}

void WasmServerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions>) {}
void WasmServerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {}

auto WasmServerFeature::loadModuleIntoRuntime(ModuleName const& name)
    -> Result {
  auto module = _wasmModuleCollection->get(name);
  if (module.fail()) {
    return Result{TRI_ERROR_WASM_EXECUTION_ERROR, module.errorMessage()};
  }

  auto wasm3Module = _environment.parse_module(module.get().code.bytes.data(),
                                               module.get().code.bytes.size());
  _runtime.load(wasm3Module);
  _guardedModules.doUnderLock([&name](GuardedModules guardedModules) {
    guardedModules._loadedModules.emplace(name.string);
  });
  return Result{};
}

auto WasmServerFeature::executeFunction(
    ModuleName const& moduleName, FunctionName const& functionName,
    wasm::FunctionParameters const& parameters) -> ResultT<uint64_t> {
  auto moduleAlreadyLoaded = _guardedModules.doUnderLock(
      [&moduleName](GuardedModules guardedModules) -> bool {
        return guardedModules._loadedModules.contains(moduleName.string);
      });
  if (!moduleAlreadyLoaded) {
    if (auto result = loadModuleIntoRuntime(moduleName); result.fail()) {
      return ResultT<uint64_t>::error(result.errorNumber(),
                                      result.errorMessage());
    }
  }

  auto function = _runtime.find_function(functionName.string.c_str());
  if (!function.has_value()) {
    return ResultT<uint64_t>::error(TRI_ERROR_WASM_EXECUTION_ERROR,
                                    "WasmServerFeature: Function '" +
                                        functionName.string + "' in module '" +
                                        moduleName.string + "' not found");
  }
  return function.value().call<uint64_t>(parameters.a, parameters.b);
}

auto WasmServerFeature::addModule(Module const& module) -> Result {
  return _wasmModuleCollection->add(module);
}

auto WasmServerFeature::removeModule(ModuleName const& name) -> Result {
  return _wasmModuleCollection->remove(name);
}

auto WasmServerFeature::allModules() const -> ResultT<std::set<std::string>> {
  return _wasmModuleCollection->allNames();
}

auto WasmServerFeature::module(ModuleName const& name) const
    -> ResultT<Module> {
  return _wasmModuleCollection->get(name);
}
