////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestBaseHandler.h"
#include "Basics/StringUtils.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestBaseHandler::RestBaseHandler(HttpRequest* request) : HttpHandler(request) {}

void RestBaseHandler::handleError(Exception const& ex) {
  generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from JSON
////////////////////////////////////////////////////////////////////////////////
void RestBaseHandler::generateResult(TRI_json_t const* json) {
  generateResult(HttpResponse::OK, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from JSON
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult(HttpResponse::HttpResponseCode code,
                                     TRI_json_t const* json) {
  createResponse(code);
  _response->setContentType("application/json; charset=utf-8");

  int res = TRI_StringifyJson(_response->body().stringBuffer(), json);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from VelocyPack
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult(VPackSlice const& slice) {
  generateResult(HttpResponse::OK, slice);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from VelocyPack
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult(HttpResponse::HttpResponseCode code,
                                     VPackSlice const& slice) {
  createResponse(code);
  _response->setContentType("application/json; charset=utf-8");

  VPackStringBufferAdapter buffer(_response->body().stringBuffer());

  VPackDumper dumper(&buffer);
  try {
    dumper.dump(slice);
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a cancel message
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateCanceled() {
  VPackBuilder builder;
  try {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("error", VPackValue(true));
    builder.add("code", VPackValue((int32_t)HttpResponse::GONE));
    builder.add("errorNum", VPackValue((int32_t)TRI_ERROR_REQUEST_CANCELED));
    builder.add("errorMessage", VPackValue("request canceled"));
    builder.close();

    VPackSlice slice(builder.start());
    generateResult(HttpResponse::GONE, slice);
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError(HttpResponse::HttpResponseCode code,
                                    int errorCode) {
  char const* message = TRI_errno_string(errorCode);

  if (message) {
    generateError(code, errorCode, std::string(message));
  } else {
    generateError(code, errorCode, std::string("unknown error"));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError(HttpResponse::HttpResponseCode code,
                                    int errorCode, std::string const& message) {
  createResponse(code);
  _response->setContentType("application/json; charset=utf-8");

  VPackBuilder builder;
  try {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("error", VPackValue(true));
    if (message.empty()) {
      // prevent empty error messages
      builder.add("errorMessage", VPackValue(TRI_errno_string(errorCode)));
    } else {
      builder.add("errorMessage",
                  VPackValue(StringUtils::escapeUnicode(message)));
    }
    builder.add("code", VPackValue(code));
    builder.add("errorNum", VPackValue(errorCode));
    builder.close();
    VPackSlice slice(builder.start());
    VPackStringBufferAdapter buffer(_response->body().stringBuffer());
    VPackDumper dumper(&buffer);
    dumper.dump(slice);
  } catch (...) {
    // Building the error response failed
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an OUT_OF_MEMORY error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateOOMError() {
  generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
}
