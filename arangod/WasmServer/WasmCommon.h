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
#include <string>
#include <utility>
#include <vector>

#include "Basics/ResultT.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {
class Slice;
}  // namespace velocypack
}  // namespace arangodb

namespace arangodb::wasm {

struct Code {
  std::vector<uint8_t> bytes;
  auto operator==(Code const& function) const -> bool = default;
};

struct ModuleName {
  std::string string;
  auto operator==(ModuleName const& module) const -> bool = default;
  ModuleName(std::string string) : string{std::move(string)} {}
};

struct Module {
  ModuleName name;
  Code code;
  bool isDeterministic;
  auto operator==(Module const& function) const -> bool = default;
};

struct FunctionName {
  std::string string;
  auto operator==(FunctionName const& module) const -> bool = default;
  FunctionName(std::string string) : string{std::move(string)} {}
};

using FunctionInput = VPackSlice;
using FunctionOutput = VPackSlice;

auto velocypackToModule(arangodb::velocypack::Slice slice) -> ResultT<Module>;
void moduleToVelocypack(Module const& module, VPackBuilder& builder,
                        bool forCollection = false);

}  // namespace arangodb::wasm
