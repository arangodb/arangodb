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

auto showAllModules(WasmVmMethods const& methods, VPackBuilder& response)
    -> void {
  auto modules = methods.allModules().get();

  VPackArrayBuilder ob(&response);
  for (auto const& function : modules) {
    response.add(VPackValue(function.first));
  }
}

auto showModule(ModuleName const& name, WasmVmMethods const& methods,
                VPackBuilder& response) -> Result {
  auto module = methods.module(name).get();

  if (!module.has_value()) {
    return Result{
        TRI_ERROR_BAD_PARAMETER,
        "RestWasmHandler: Module " + name.string + " does not exist."};
  }

  VPackObjectBuilder ob(&response);
  response.add(VPackValue("result"));
  arangodb::wasm::moduleToVelocypack(module.value(), response);
  return Result{};
}

auto RestWasmHandler::handleGetRequest(WasmVmMethods const& methods)
    -> RestStatus {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  VPackBuilder builder;
  if (suffixes.size() == 0) {
    showAllModules(methods, builder);
    generateOk(rest::ResponseCode::OK, builder.slice());
  } else if (suffixes.size() == 1) {
    auto result = showModule(ModuleName{suffixes[0]}, methods, builder);
    if (result.fail()) {
      generateError(ResponseCode::BAD, result.errorNumber(),
                    result.errorMessage());
    } else {
      generateOk(rest::ResponseCode::OK, builder.slice());
    }
  } else {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "RestWasmHandler: Expects name of module as suffix.");
    return RestStatus::DONE;
  }
  return RestStatus::DONE;
}

auto RestWasmHandler::handleDeleteRequest(WasmVmMethods const& methods)
    -> RestStatus {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() != 1) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
        "RestWasmHandler: Expects name of removable module as suffix.");
    return RestStatus::DONE;
  }
  auto const& name = suffixes[0];

  methods.deleteModule(ModuleName{name});

  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add("removed", VPackValue(name));
  }
  generateOk(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}

auto addWasmModule(VPackSlice slice, wasm::WasmVmMethods const& methods,
                   VPackBuilder& response) -> Result {
  ResultT<Module> module = arangodb::wasm::velocypackToModule(slice);
  if (module.fail()) {
    return Result{module.errorNumber(), module.errorMessage()};
  }

  auto create = methods.addModule(module.get());

  {
    VPackObjectBuilder ob1(&response);
    response.add("installed", VPackValue(module.get().name.string));
  }
  return Result{};
}

auto executeWasmFunction(ModuleName const& moduleName,
                         FunctionName const& functionName, VPackSlice slice,
                         wasm::WasmVmMethods const& methods,
                         VPackBuilder& response) -> Result {
  auto functionParameters =
      arangodb::wasm::velocypackToFunctionParameters(slice);
  if (!functionParameters.ok()) {
    return Result{functionParameters.errorNumber(),
                  functionParameters.errorMessage()};
  }

  auto result =
      methods
          .executeFunction(moduleName, functionName, functionParameters.get())
          .get();
  if (!result.has_value()) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  "RestWasmHandler: Function '" + functionName.string +
                      "' in module '" + moduleName.string +
                      "' cannot be found"};
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
    auto result = addWasmModule(slice, methods, response);
    if (result.fail()) {
      generateError(ResponseCode::BAD, result.errorNumber(),
                    result.errorMessage());
    } else {
      generateOk(rest::ResponseCode::CREATED, response.slice());
    }
  } else if (suffixes.size() == 2) {
    auto result =
        executeWasmFunction(ModuleName{suffixes[0]}, FunctionName{suffixes[1]},
                            slice, methods, response);
    if (result.fail()) {
      generateError(ResponseCode::BAD, result.errorNumber(),
                    result.errorMessage());
    } else {
      generateOk(rest::ResponseCode::OK, response.slice());
    }
  } else {
    generateError(
        ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
        "RestWasmHandler: Use POST without suffix to add a function, POST with "
        "modulename and functionname as "
        "suffixes to exectue the function.");
  }
  return RestStatus::DONE;
}
