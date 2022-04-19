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
#include <memory>
#include <string>
#include <vector>

#include "WasmServer/WasmCommon.h"

struct TRI_vocbase_t;

namespace arangodb {
class Result;
template<typename T>
class ResultT;

namespace futures {
template<typename T>
class Future;
}
}  // namespace arangodb

namespace arangodb::wasm {

struct WasmVmMethods {
  virtual ~WasmVmMethods() = default;
  virtual auto addModule(Module const& module) const
      -> futures::Future<Result> = 0;
  virtual auto removeModule(ModuleName const& name) const
      -> futures::Future<Result> = 0;
  virtual auto allModules() const
      -> futures::Future<ResultT<std::vector<ModuleName>>> = 0;
  virtual auto module(ModuleName const& name) const
      -> futures::Future<ResultT<Module>> = 0;
  virtual auto executeFunction(ModuleName const& moduleName,
                               FunctionName const& functionName,
                               FunctionInput const& parameters) const
      -> futures::Future<ResultT<FunctionOutput>> = 0;
  static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<WasmVmMethods>;
};

}  // namespace arangodb::wasm
