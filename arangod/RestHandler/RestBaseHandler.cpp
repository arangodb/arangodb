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
#include "Utils/TransactionContext.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestBaseHandler::RestBaseHandler(HttpRequest* request) : HttpHandler(request) {}

void RestBaseHandler::handleError(Exception const& ex) {
  generateError(GeneralResponse::responseCode(ex.code()), ex.code(), ex.what());
}



////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from VelocyPack
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult(GeneralResponse::ResponseCode code,
                                     VPackSlice const& slice) {

  createResponse(code);
  writeResult(slice, VPackOptions::Defaults);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from VelocyPack
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult(GeneralResponse::ResponseCode code,
                                     VPackSlice const& slice,
                                     std::shared_ptr<TransactionContext> context) {
  createResponse(code);
  writeResult(slice,*(context->getVPackOptions()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError(GeneralResponse::ResponseCode code,
                                    int errorCode) {
  char const* message = TRI_errno_string(errorCode);

  if (message != nullptr) {
    generateError(code, errorCode, std::string(message));
  } else {
    generateError(code, errorCode, std::string("unknown error"));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError(GeneralResponse::ResponseCode code,
                                    int errorCode, std::string const& message) {
  createResponse(code);

  VPackBuilder builder;
  try {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("error", VPackValue(true));
    if (message.empty()) {
      // prevent empty error messages
      builder.add("errorMessage", VPackValue(TRI_errno_string(errorCode)));
    } else {
      builder.add("errorMessage", VPackValue(message));
    }
    builder.add("code", VPackValue(static_cast<int>(code)));
    builder.add("errorNum", VPackValue(errorCode));
    builder.close();

    writeResult(builder.slice(), VPackOptions::Defaults);
  } catch (...) {
    // Building the error response failed
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an OUT_OF_MEMORY error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateOOMError() {
  generateError(GeneralResponse::ResponseCode::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a cancel message
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateCanceled() {
  return generateError(GeneralResponse::ResponseCode::GONE, TRI_ERROR_REQUEST_CANCELED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dumps the response as JSON into the response string buffer
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::dumpResponse(VPackSlice const& slice,
                                   VPackOptions const* options) {
  VPackStringBufferAdapter buffer(_response->body().stringBuffer());

  VPackDumper dumper(&buffer, options);
  try {
    dumper.dump(slice);
  } catch (std::exception const& ex) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief checks if velocypack is expected as answer
//////////////////////////////////////////////////////////////////////////////

bool RestBaseHandler::returnVelocypack() const {
  auto accept = std::string(_request->header("accept"));
  if (std::string::npos == accept.find("application/x-velocypack")) {
    return false;
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief writes volocypack or json to response
//////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::writeResult(arangodb::velocypack::Slice const& slice, VPackOptions const& options){
  if(returnVelocypack()) {
    _response->setContentType("application/x-velocypack");
    _response->body().appendText(slice.startAs<const char>(),slice.byteSize());
  } else {
    _response->setContentType("application/json; charset=utf-8");
    dumpResponse(slice, &options);
  }
}
