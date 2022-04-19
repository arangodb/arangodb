#include "WasmServerFeature.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/error-registry.h"
#include "Basics/Result.h"
#include "Basics/application-exit.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "WasmServer/WasmCommon.h"
#include "WasmServer/WasmModuleCollection.h"
#include "WasmServer/WasmVm/wasm_with_slices.hpp"

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

void WasmServerFeature::prepare() { setEnabled(true); }

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
  auto code = module.get().code.bytes;
  if (auto res = _vm.load_module(module.get().code.bytes); res.fail()) {
    return Result{TRI_ERROR_WASM_EXECUTION_ERROR, res.errorMessage()};
  }
  _guardedModules.doUnderLock([&name](GuardedModules& guardedModules) {
    guardedModules._loadedModules.emplace(name.string);
  });
  return Result{};
}

auto WasmServerFeature::executeFunction(ModuleName const& moduleName,
                                        FunctionName const& functionName,
                                        wasm::FunctionInput const& parameters)
    -> ResultT<FunctionOutput> {
  auto moduleAlreadyLoaded = _guardedModules.doUnderLock(
      [&moduleName](GuardedModules const& guardedModules) -> bool {
        return guardedModules._loadedModules.contains(moduleName.string);
      });
  if (!moduleAlreadyLoaded) {
    if (auto result = loadModuleIntoRuntime(moduleName); result.fail()) {
      return ResultT<FunctionOutput>::error(result.errorNumber(),
                                            result.errorMessage());
    }
  }

  auto output = arangodb::wasm_interface::call_function(
      _vm, functionName.string.data(), parameters);
  if (output.fail()) {
    return ResultT<FunctionOutput>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("WasmServerFeature: Module {}: {}", moduleName.string,
                    output.errorMessage()));
  }
  return FunctionOutput{output.get()};
}

auto WasmServerFeature::addModule(Module const& module) -> Result {
  return _wasmModuleCollection->add(module);
}

auto WasmServerFeature::removeModule(ModuleName const& name) -> Result {
  return _wasmModuleCollection->remove(name);
}

auto WasmServerFeature::allModules() const -> ResultT<std::vector<ModuleName>> {
  return _wasmModuleCollection->allNames();
}

auto WasmServerFeature::module(ModuleName const& name) const
    -> ResultT<Module> {
  return _wasmModuleCollection->get(name);
}
