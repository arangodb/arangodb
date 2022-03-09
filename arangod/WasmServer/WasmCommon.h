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

class WasmFunction {
 public:
  std::string _name;
  std::vector<uint8_t> _code;
  bool _isDeterministic;
  static auto requiredStringField(std::string const&& fieldName,
                                  velocypack::Slice slice)
      -> ResultT<std::string>;
  static auto requiredCodeField(std::string const&& fieldName,
                                velocypack::Slice slice)
      -> ResultT<std::vector<uint8_t>>;
  static auto optionalBoolField(std::string const&& fieldName,
                                bool defaultValue, velocypack::Slice slice)
      -> arangodb::ResultT<bool>;
  static auto checkOnlyValidFieldnamesAreIncluded(
      std::set<std::string>&& validFieldnames, velocypack::Slice slice)
      -> arangodb::Result;

 public:
  WasmFunction(std::string name, std::vector<uint8_t> code,
               bool isDeterministic);
  auto name() const -> std::string { return _name; };
  static auto fromVelocyPack(arangodb::velocypack::Slice slice)
      -> ResultT<WasmFunction>;
  void toVelocyPack(VPackBuilder& builder) const;
  auto operator==(const WasmFunction& function) const -> bool = default;
};

}  // namespace arangodb::wasm
