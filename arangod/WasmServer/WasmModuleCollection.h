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

#include <set>
#include <string>

#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "RestServer/arangod.h"
#include "WasmServer/WasmCommon.h"

struct TRI_vocbase_t;

namespace arangodb {

class WasmModuleCollection {
 public:
  WasmModuleCollection(TRI_vocbase_t& vocbase);
  auto allNames() const -> ResultT<std::set<std::string>>;
  auto get(wasm::ModuleName const& name) const -> ResultT<wasm::Module>;
  auto add(wasm::Module const& module) -> Result;
  auto remove(wasm::ModuleName const& name) -> Result;

 private:
  TRI_vocbase_t& _vocbase;
  inline static std::string const _collection = "wasmModules";
};

void registerWasmModuleCollectionUpgradeTask(ArangodServer& server);

}  // namespace arangodb
