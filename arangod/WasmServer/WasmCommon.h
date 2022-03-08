
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

#include <string>
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include "Basics/ResultT.h"
#include "velocypack/Slice.h"

namespace arangodb::wasm {

struct WasmContainer {
  uint8_t* code;
  size_t length;
};

struct WasmFunction {
  std::string name;
  WasmContainer code;
  bool isDeterministic;
  static auto fromVelocyPack(arangodb::velocypack::Slice slice)
      -> ResultT<WasmFunction>;
};

void toVelocyPack(WasmFunction const& wasmFunction, VPackBuilder& builder);
auto requiredStringSliceField(std::string_view fieldName,
                              velocypack::Slice slice)
    -> ResultT<velocypack::Slice>;
auto deterministicField(velocypack::Slice slice) -> arangodb::ResultT<bool>;
auto areOnlyValidFieldsIncluded(velocypack::Slice slice)
    -> arangodb::ResultT<bool>;

}  // namespace arangodb::wasm
