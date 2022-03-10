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

#include <memory>
#include <variant>
#include "VocBase/vocbase.h"
#include "WasmCommon.h"

namespace arangodb {
class Result;
namespace futures {
template<typename T>
class Future;
}
}  // namespace arangodb

namespace arangodb::wasm {

struct WasmVmMethods {
  virtual ~WasmVmMethods() = default;
  virtual auto createWasmUdf(WasmFunction const& function) const
      -> futures::Future<Result> = 0;
  virtual auto deleteWasmUdf(std::string const& functionName) const
      -> futures::Future<Result> = 0;
  virtual auto getAllWasmUdfs() const
      -> futures::Future<std::unordered_map<std::string, WasmFunction>> = 0;
  virtual auto executeWasmUdf(std::string const& name, uint64_t a,
                              uint64_t b) const
      -> futures::Future<std::optional<uint64_t>> = 0;
  static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<WasmVmMethods>;
};

}  // namespace arangodb::wasm
