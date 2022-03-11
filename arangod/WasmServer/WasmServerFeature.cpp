#include <unordered_map>
#include "WasmServerFeature.h"
#include <s2/base/integral_types.h>
#include <optional>
#include "Cluster/ServerState.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "WasmServer/Wasm3cpp.h"
#include "WasmServer/WasmCommon.h"

using namespace arangodb;

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

void WasmServerFeature::addModule(wasm::Module const& module) {
  _guardedModules.doUnderLock([&module](GuardedModules& guardedModules) {
    guardedModules._modules.insert_or_assign(module.name, module);
  });
}

auto WasmServerFeature::loadModule(std::string const& name)
    -> std::optional<wasm3::module> {
  auto module = _guardedModules.doUnderLock(
      [&name](GuardedModules& guardedModules) -> std::optional<wasm::Module> {
        auto maybef = guardedModules._modules.find(name);
        if (maybef == std::end(guardedModules._modules)) {
          return std::nullopt;
        } else {
          return maybef->second;
        }
      });
  if (module.has_value()) {
    return environment.parse_module(module.value().code.bytes.data(),
                                    module.value().code.bytes.size());
  } else {
    return std::nullopt;
  }
}

auto WasmServerFeature::executeFunction(
    std::string const& moduleName, std::string const& functionName,
    wasm::FunctionParameters const& parameters) -> std::optional<uint64_t> {
  auto module = loadModule(moduleName);
  auto runtime = environment.new_runtime(1024);
  if (!module.has_value()) {
    return std::nullopt;
  }

  runtime.load(module.value());
  auto function = runtime.find_function(functionName.c_str());
  if (function.fail()) {
    return std::nullopt;
  }

  return function.get().call<uint64_t>(parameters.a, parameters.b);
}

void WasmServerFeature::deleteModule(std::string const& name) {
  _guardedModules.doUnderLock([&name](GuardedModules& guardedModules) {
    guardedModules._modules.erase(name);
  });
}

auto WasmServerFeature::allModules() const
    -> std::unordered_map<std::string, wasm::Module> {
  return _guardedModules.doUnderLock([&](GuardedModules const& guardedModules) {
    return guardedModules._modules;
  });
}
