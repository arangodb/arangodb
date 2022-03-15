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

#include "Methods.h"
#include "Futures/Future.h"
#include "Cluster/ServerState.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "WasmServerFeature.h"
#include "WasmCommon.h"

using namespace arangodb;
using namespace arangodb::wasm;

struct WasmVmMethodsSingleServer final
    : WasmVmMethods,
      std::enable_shared_from_this<WasmVmMethodsSingleServer> {
  explicit WasmVmMethodsSingleServer(TRI_vocbase_t& vocbase)
      : vocbase(vocbase){};

  auto createWasmUdf(WasmFunction const& function) const
      -> futures::Future<Result> override {
    vocbase.server().getFeature<WasmServerFeature>().addFunction(function);
    return Result{TRI_ERROR_NO_ERROR};
  }

  auto deleteWasmUdf(std::string const& functionName) const
      -> futures::Future<Result> override {
    vocbase.server().getFeature<WasmServerFeature>().deleteFunction(
        functionName);
    return Result{TRI_ERROR_NO_ERROR};
  }

  auto getAllWasmUdfs() const -> futures::Future<
      std::unordered_map<std::string, WasmFunction>> override {
    return vocbase.server().getFeature<WasmServerFeature>().getAllFunctions();
  }

  auto executeWasmUdf(std::string const& name, uint64_t a, uint64_t b) const
      -> futures::Future<std::optional<uint64_t>> override {
    return vocbase.server().getFeature<WasmServerFeature>().executeFunction(
        name, a, b);
  }

  TRI_vocbase_t& vocbase;
};

auto WasmVmMethods::createInstance(TRI_vocbase_t& vocbase)
    -> std::shared_ptr<WasmVmMethods> {
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_SINGLE:
      return std::make_shared<WasmVmMethodsSingleServer>(vocbase);
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "This API is only available on single server.");
  }
}
