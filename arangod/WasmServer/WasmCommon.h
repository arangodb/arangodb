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
#include "Basics/Result.h"
#include "velocypack/Slice.h"
#include <set>
#include <tuple>

namespace arangodb::wasm {

struct Code {
  std::vector<uint8_t> bytes;
  auto operator==(Code const& function) const -> bool = default;
};

struct WasmFunction {
  std::string name;
  Code code;
  bool isDeterministic;
  auto operator==(WasmFunction const& function) const -> bool = default;
};

struct Parameters {
  uint64_t a;
  uint64_t b;
  auto operator==(Parameters const& parameters) const -> bool = default;
};

auto velocypackToWasmFunction(arangodb::velocypack::Slice slice)
    -> ResultT<WasmFunction>;
void wasmFunctionToVelocypack(WasmFunction const& wasmFunction,
                              VPackBuilder& builder);
auto velocypackToParameters(arangodb::velocypack::Slice slice)
    -> ResultT<Parameters>;

}  // namespace arangodb::wasm
