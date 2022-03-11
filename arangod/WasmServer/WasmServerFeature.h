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

#include <s2/base/integral_types.h>
#include <unordered_map>
#include "RestServer/arangod.h"
#include "WasmCommon.h"
#include "Basics/Guarded.h"
#include "Wasm3cpp.h"

namespace arangodb {
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
  auto loadModule(std::string const& name) -> std::optional<wasm3::module>;
  auto executeFunction(std::string const& moduleName,
                       std::string const& functionName,
                       wasm::FunctionParameters const& parameters)
      -> std::optional<uint64_t>;
  void deleteModule(std::string const& name);
  auto allModules() const -> std::unordered_map<std::string, wasm::Module>;

 private:
  struct GuardedModules {
    std::unordered_map<std::string, wasm::Module> _modules;
  };
  Guarded<GuardedModules> _guardedModules;

  wasm3::environment environment;
};
}  // namespace arangodb
