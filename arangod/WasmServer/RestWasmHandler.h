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

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "WasmServer/Methods.h"
#include "velocypack/Slice.h"

namespace arangodb {
using namespace wasm;

class RestWasmHandler : public arangodb::RestVocbaseBaseHandler {
 public:
  explicit RestWasmHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

  ~RestWasmHandler() override;

 private:
  auto executeByMethod(wasm::WasmVmMethods const& methods) -> RestStatus;
  auto handleGetRequest(wasm::WasmVmMethods const& methods) -> RestStatus;
  auto handlePostRequest(wasm::WasmVmMethods const& methods) -> RestStatus;
  auto handleDeleteRequest(wasm::WasmVmMethods const& methods) -> RestStatus;

 public:
  RestStatus execute() override;
  char const* name() const override { return "Wasm Rest Handler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }
};
}  // namespace arangodb
