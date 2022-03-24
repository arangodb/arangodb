////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Guarded.h"
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "RestServer/arangod.h"
#include "WasmServer/Wasm3cpp.h"
#include "WasmServer/WasmCommon.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}  // namespace options

class WasmServerFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "WasmServerFeature";
  }

  explicit WasmServerFeature(Server& server);
  ~WasmServerFeature() override = default;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;

  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override;
  void addModule(wasm::Module const& module);
  auto loadModuleIntoRuntime(wasm::ModuleName const& name) -> Result;
  auto executeFunction(wasm::ModuleName const& moduleName,
                       wasm::FunctionName const& functionName,
                       wasm::FunctionParameters const& parameters)
      -> ResultT<uint64_t>;
  void deleteModule(wasm::ModuleName const& name);
  auto allModules() const -> std::unordered_map<std::string, wasm::Module>;
  auto module(wasm::ModuleName const& name) const
      -> std::optional<wasm::Module>;

 private:
  struct GuardedModules {
    std::unordered_map<std::string, wasm::Module> _modules;
  };
  Guarded<GuardedModules> _guardedModules;

  wasm3::environment _environment;
  wasm3::runtime _runtime = _environment.new_runtime(1024);

  std::set<std::string> _loadedModules;
};
}  // namespace arangodb
