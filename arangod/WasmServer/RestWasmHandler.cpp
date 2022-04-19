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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Basics/voc-errors.h"
#include "Futures/Future.h"
#include "GeneralServer/RestHandler.h"
#include "Rest/CommonDefines.h"
#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"
#include "RestServer/arangod.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include "velocypack/Value.h"
#include "WasmServer/Methods.h"
#include "WasmServer/WasmCommon.h"

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
    -> Result {
  auto modules = methods.allModules().get();

  if (modules.fail()) {
    return Result{modules.errorNumber(), modules.errorMessage()};
  }

  VPackArrayBuilder ob(&response);
  for (auto const& module : modules.get()) {
    response.add(VPackValue(module.string));
  }
  return {};
}

auto showModule(ModuleName const& name, WasmVmMethods const& methods,
                VPackBuilder& response) -> Result {
  auto module = methods.module(name).get();

  if (module.fail()) {
    return Result{module.errorNumber(), module.errorMessage()};
  }

  VPackObjectBuilder ob(&response);
  response.add(VPackValue("result"));
  arangodb::wasm::moduleToVelocypack(module.get(), response);
  return {};
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

  auto result = methods.removeModule(ModuleName{name});
  if (auto res = result.get(); res.fail()) {
    generateError(ResponseCode::BAD, res.errorNumber(), res.errorMessage());
    return RestStatus::DONE;
  }

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

  auto result = methods.addModule(module.get());
  if (auto res = result.get(); res.fail()) {
    return res;
  }

  VPackObjectBuilder ob1(&response);
  response.add("installed", VPackValue(module.get().name.string));
  return Result{};
}

auto executeWasmFunction(ModuleName const& moduleName,
                         FunctionName const& functionName, FunctionInput input,
                         wasm::WasmVmMethods const& methods,
                         VPackBuilder& response) -> Result {
  auto output = methods.executeFunction(moduleName, functionName, input).get();
  if (output.fail()) {
    return Result{output.errorNumber(), output.errorMessage()};
  }

  VPackObjectBuilder ob(&response);
  response.add("result", output.get());
  return Result{};
}

auto RestWasmHandler::handlePostRequest(WasmVmMethods const& methods)
    -> RestStatus {
  auto success = bool{};
  auto input = parseVPackBody(success);
  if (!success) {
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  VPackBuilder response;
  if (suffixes.size() == 0) {
    auto result = addWasmModule(input, methods, response);
    if (result.fail()) {
      generateError(ResponseCode::BAD, result.errorNumber(),
                    result.errorMessage());
    } else {
      generateOk(rest::ResponseCode::CREATED, response.slice());
    }
  } else if (suffixes.size() == 2) {
    auto result =
        executeWasmFunction(ModuleName{suffixes[0]}, FunctionName{suffixes[1]},
                            input, methods, response);
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
