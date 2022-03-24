#include "WasmServerFeature.h"

#include <cstdint>
#include <iterator>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/error-registry.h"
#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "Cluster/ServerState.h"
#include "WasmServer/Wasm3cpp.h"
#include "WasmServer/WasmCommon.h"

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
  startsAfter<DatabaseFeaturePhase>();
}

void WasmServerFeature::prepare() {
  if (ServerState::instance()->isCoordinator() or
      ServerState::instance()->isDBServer()) {
    setEnabled(true);
  } else {
    setEnabled(false);
  }
}

void WasmServerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions>) {}
void WasmServerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {}

void WasmServerFeature::addModule(Module const& module) {
  _guardedModules.doUnderLock([&module](GuardedModules& guardedModules) {
    guardedModules._modules.insert_or_assign(module.name.string, module);
  });
}

auto WasmServerFeature::loadModuleIntoRuntime(ModuleName const& name)
    -> Result {
  auto module = _guardedModules.doUnderLock(
      [&name](GuardedModules& guardedModules) -> std::optional<Module> {
        auto maybef = guardedModules._modules.find(name.string);
        if (maybef == std::end(guardedModules._modules)) {
          return std::nullopt;
        } else {
          return maybef->second;
        }
      });

  if (!module.has_value()) {
    return Result{TRI_ERROR_WASM_EXECUTION_ERROR,
                  "WasmServerFeature: Module '" + name.string + "' not found"};
  }

  auto wasm3Module = _environment.parse_module(
      module.value().code.bytes.data(), module.value().code.bytes.size());
  _runtime.load(wasm3Module);
  _loadedModules.emplace(name.string);
  return Result{};
}

auto WasmServerFeature::executeFunction(
    ModuleName const& moduleName, FunctionName const& functionName,
    wasm::FunctionParameters const& parameters) -> ResultT<uint64_t> {
  if (!_loadedModules.contains(moduleName.string)) {
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

void WasmServerFeature::deleteModule(ModuleName const& name) {
  _guardedModules.doUnderLock([&name](GuardedModules& guardedModules) {
    guardedModules._modules.erase(name.string);
  });
}

auto WasmServerFeature::allModules() const
    -> std::unordered_map<std::string, Module> {
  return _guardedModules.doUnderLock([](GuardedModules const& guardedModules) {
    return guardedModules._modules;
  });
}

auto WasmServerFeature::module(ModuleName const& name) const
    -> std::optional<Module> {
  return _guardedModules.doUnderLock(
      [&name](GuardedModules const& guardedModules) -> std::optional<Module> {
        auto module = guardedModules._modules.find(name.string);
        if (module == std::end(guardedModules._modules)) {
          return std::nullopt;
        } else {
          return module->second;
        }
      });
}
