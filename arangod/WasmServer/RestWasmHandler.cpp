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

#include "RestWasmHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include "velocypack/Parser.h"

#include "GeneralServer/RestHandler.h"
#include "Logger/LogMacros.h"
#include "Rest/CommonDefines.h"

#include "WasmServer/Methods.h"
#include "RestServer/arangod.h"

#include "WasmServer/WasmCommon.h"
#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "velocypack/Slice.h"

using namespace arangodb;
using namespace arangodb::wasm;

RestWasmHandler::RestWasmHandler(ArangodServer& server, GeneralRequest* request,
                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestWasmHandler::~RestWasmHandler() = default;

auto RestWasmHandler::execute() -> RestStatus {
  // depends on ServerState::RoleEnum
  auto methods = WasmVmMethods::createInstance(_vocbase);
  return executeByMethod(*methods);
}

auto RestWasmHandler::executeByMethod(WasmVmMethods const& methods)
    -> RestStatus {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      return handleGetRequest(methods);
    case rest::RequestType::POST:
      return handlePostRequest(methods);
    case rest::RequestType::DELETE_REQ:
      return handleDeleteRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}

auto RestWasmHandler::handleGetRequest(WasmVmMethods const& methods)
    -> RestStatus {
  auto allFunctions = methods.getAllWasmUdfs().get();

  VPackBuilder builder;
  {
    VPackArrayBuilder ob(&builder);
    for (auto const& function : allFunctions) {
      arangodb::wasm::wasmFunctionToVelocypack(function.second, builder);
    }
  }
  generateOk(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}

auto RestWasmHandler::handleDeleteRequest(WasmVmMethods const& methods)
    -> RestStatus {
  auto success = bool{};
  auto slice = parseVPackBody(success);
  if (!success) {
    return RestStatus::DONE;
  }
  if (!slice.isString()) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
        "RestWasmHandler Expects name of removable function as string");
  }

  methods.deleteWasmUdf(slice.copyString());

  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add("removed", VPackValue(true));
  }
  generateOk(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}

auto addWasmFunction(VPackSlice slice, wasm::WasmVmMethods const& methods,
                     VPackBuilder& response) -> Result {
  ResultT<WasmFunction> function =
      arangodb::wasm::velocypackToWasmFunction(slice);
  if (function.fail()) {
    return Result{function.errorNumber(), function.errorMessage()};
  }

  auto create = methods.createWasmUdf(function.get());

  {
    VPackObjectBuilder ob1(&response);
    response.add(VPackValue("installed"));
    arangodb::wasm::wasmFunctionToVelocypack(function.get(), response);
  }
  return Result{};
}

auto executeWasmFunction(std::string const& functionname, VPackSlice slice,
                         wasm::WasmVmMethods const& methods,
                         VPackBuilder& response) -> Result {
  auto parameters = arangodb::wasm::velocypackToParameters(slice);
  if (!parameters.ok()) {
    return Result{parameters.errorNumber(), parameters.errorMessage()};
  }
  auto [a, b] = parameters.get();

  auto result = methods.executeWasmUdf(functionname, a, b).get();
  if (!result.has_value()) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  "Function '" + functionname + "' cannot be found"};
  }

  {
    VPackObjectBuilder ob(&response);
    response.add("result", VPackValue(result.value()));
  }
  return Result{};
}

auto RestWasmHandler::handlePostRequest(WasmVmMethods const& methods)
    -> RestStatus {
  auto success = bool{};
  auto slice = parseVPackBody(success);
  if (!success) {
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  VPackBuilder response;
  if (suffixes.size() == 0) {
    auto result = addWasmFunction(slice, methods, response);
    if (result.fail()) {
      generateError(ResponseCode::BAD, result.errorNumber(),
                    result.errorMessage());
    } else {
      generateOk(rest::ResponseCode::CREATED, response.slice());
    }
  } else if (suffixes.size() == 1) {
    auto result = executeWasmFunction(suffixes[0], slice, methods, response);
    if (result.fail()) {
      generateError(ResponseCode::BAD, result.errorNumber(),
                    result.errorMessage());
    } else {
      generateOk(rest::ResponseCode::OK, response.slice());
    }
  } else {
    generateError(
        ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
        "POST without suffix to add a function, POST with functionname as "
        "suffix to exectue the function. More than one suffix is not allowed");
  }
  return RestStatus::DONE;
}
