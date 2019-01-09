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

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Meta/conversion.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Transaction/Context.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestBaseHandler::RestBaseHandler(GeneralRequest* request, GeneralResponse* response)
    : RestHandler(request, response) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the body as VelocyPack
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestBaseHandler::parseVelocyPackBody(bool& success) {
  try {
    success = true;
    return _request->toVelocyPackBuilderPtr();
  } catch (VPackException const& e) {
    std::string errmsg("VPackError error: ");
    errmsg.append(e.what());
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON, errmsg);
  }
  success = false;
  return std::make_shared<VPackBuilder>();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the body as VelocyPack
////////////////////////////////////////////////////////////////////////////////

arangodb::velocypack::Slice RestBaseHandler::parseVPackBody(bool& success) {
  try {
    success = true;
    VPackOptions optionsWithUniquenessCheck = VPackOptions::Defaults;
    optionsWithUniquenessCheck.checkAttributeUniqueness = true;
    return _request->payload(&optionsWithUniquenessCheck);
  } catch (VPackException const& e) {
    std::string errmsg("VPackError error: ");
    errmsg.append(e.what());
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON, errmsg);
  }
  success = false;
  return VPackSlice::noneSlice();
}

void RestBaseHandler::handleError(Exception const& ex) {
  generateError(GeneralResponse::responseCode(ex.code()), ex.code(), ex.what());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from VelocyPack
////////////////////////////////////////////////////////////////////////////////

template <typename Payload>
void RestBaseHandler::generateResult(rest::ResponseCode code, Payload&& payload) {
  resetResponse(code);
  VPackOptions options(VPackOptions::Defaults);
  options.escapeUnicode = true;
  writeResult(std::forward<Payload>(payload), options);
}

template <typename Payload>
void RestBaseHandler::generateResult(rest::ResponseCode code, Payload&& payload,
                                     VPackOptions const* options) {
  resetResponse(code);
  VPackOptions tmpoptions(*options);
  tmpoptions.escapeUnicode = true;
  writeResult(std::forward<Payload>(payload), tmpoptions);
}
////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from VelocyPack
////////////////////////////////////////////////////////////////////////////////

template <typename Payload>
void RestBaseHandler::generateResult(rest::ResponseCode code, Payload&& payload,
                                     std::shared_ptr<transaction::Context> context) {
  resetResponse(code);
  writeResult(std::forward<Payload>(payload), *(context->getVPackOptionsForDump()));
}

/// convenience function akin to generateError,
/// renders payload in 'result' field
/// adds proper `error`, `code` fields
void RestBaseHandler::generateOk(rest::ResponseCode code, VPackSlice const& payload) {
  resetResponse(code);

  try {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder tmp(buffer);
    tmp.add(VPackValue(VPackValueType::Object, true));
    tmp.add(StaticStrings::Error, VPackValue(false));
    tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
    tmp.add("result", payload);
    tmp.close();

    VPackOptions options(VPackOptions::Defaults);
    options.escapeUnicode = true;
    writeResult(std::move(buffer), options);
  } catch (...) {
    // Building the error response failed
  }
}

/// Add `error` and `code` fields into your response
void RestBaseHandler::generateOk(rest::ResponseCode code, VPackBuilder const& payload) {
  resetResponse(code);

  try {
    VPackBuilder tmp;
    tmp.add(VPackValue(VPackValueType::Object, true));
    tmp.add(StaticStrings::Error, VPackValue(false));
    tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
    tmp.close();

    tmp = VPackCollection::merge(tmp.slice(), payload.slice(), false);

    VPackOptions options(VPackOptions::Defaults);
    options.escapeUnicode = true;
    writeResult(tmp.slice(), options);
  } catch (...) {
    // Building the error response failed
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a cancel message
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateCanceled() {
  return generateError(rest::ResponseCode::GONE, TRI_ERROR_REQUEST_CANCELED);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief writes volocypack or json to response
//////////////////////////////////////////////////////////////////////////////

template <typename Payload>
void RestBaseHandler::writeResult(Payload&& payload, VPackOptions const& options) {
  try {
    TRI_ASSERT(options.escapeUnicode);
    if (_request != nullptr) {
      _response->setContentType(_request->contentTypeResponse());
    }
    _response->setPayload(std::forward<Payload>(payload), true, options);
  } catch (basics::Exception const& ex) {
    generateError(GeneralResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (std::exception const& ex) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

// TODO -- rather move code to header (slower linking) or remove templates
template void RestBaseHandler::generateResult<VPackBuffer<uint8_t>>(rest::ResponseCode,
                                                                    VPackBuffer<uint8_t>&&);
template void RestBaseHandler::generateResult<VPackSlice>(rest::ResponseCode, VPackSlice&&);
template void RestBaseHandler::generateResult<VPackSlice&>(rest::ResponseCode, VPackSlice&);

template void RestBaseHandler::generateResult<VPackBuffer<uint8_t>>(
    rest::ResponseCode, VPackBuffer<uint8_t>&&, VPackOptions const*);
template void RestBaseHandler::generateResult<VPackSlice>(rest::ResponseCode, VPackSlice&&,
                                                          VPackOptions const*);
template void RestBaseHandler::generateResult<VPackSlice&>(rest::ResponseCode, VPackSlice&,
                                                           VPackOptions const*);

template void RestBaseHandler::generateResult<VPackBuffer<uint8_t>>(
    rest::ResponseCode, VPackBuffer<uint8_t>&&, std::shared_ptr<transaction::Context>);
template void RestBaseHandler::generateResult<VPackSlice>(
    rest::ResponseCode, VPackSlice&&, std::shared_ptr<transaction::Context>);
template void RestBaseHandler::generateResult<VPackSlice&>(
    rest::ResponseCode, VPackSlice&, std::shared_ptr<transaction::Context>);

template void RestBaseHandler::writeResult<VPackBuffer<uint8_t>>(VPackBuffer<uint8_t>&& payload,
                                                                 VPackOptions const&);
template void RestBaseHandler::writeResult<VPackSlice>(VPackSlice&& payload,
                                                       VPackOptions const&);
template void RestBaseHandler::writeResult<VPackSlice&>(VPackSlice& payload,
                                                        VPackOptions const&);
template void RestBaseHandler::writeResult<VPackSlice const&>(VPackSlice const& payload,
                                                              VPackOptions const&);
