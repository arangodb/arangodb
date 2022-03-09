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

namespace arangodb::wasm {

struct Code {
  std::vector<uint8_t> bytes;
  auto operator==(const Code& function) const -> bool = default;
};

struct WasmFunction {
  std::string name;
  Code code;
  bool isDeterministic;
  auto operator==(const WasmFunction& function) const -> bool = default;
};

auto velocypack2WasmFunction(arangodb::velocypack::Slice slice)
    -> ResultT<WasmFunction>;
auto checkVelocypack2WasmFunctionIsPossible(velocypack::Slice slice)
    -> arangodb::Result;
auto velocypack2Name(arangodb::velocypack::Slice slice) -> ResultT<std::string>;
auto velocypack2Code(arangodb::velocypack::Slice slice) -> ResultT<Code>;
auto velocypack2IsDeterministic(
    std::optional<arangodb::velocypack::Slice> slice) -> ResultT<bool>;

void wasmFunction2Velocypack(WasmFunction const& wasmFunction,
                             VPackBuilder& builder);
void code2Velocypack(Code const& code, VPackBuilder& builder);

}  // namespace arangodb::wasm
