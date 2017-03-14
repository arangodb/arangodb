////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RestViewHandler.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackDumper.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"

#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestViewHandler::RestViewHandler(GeneralRequest* request,
                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestStatus RestViewHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    createView();
    return RestStatus::DONE;
  }

  if (type == rest::RequestType::PATCH) {
    modifyView();
    return RestStatus::DONE;
  }

  if (type == rest::RequestType::DELETE_REQ) {
    deleteView();
    return RestStatus::DONE;
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor
////////////////////////////////////////////////////////////////////////////////

void RestViewHandler::createView() {
  if (_request->payload().isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, 600);
    return;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/view");
    return;
  }

  try {
    bool parseSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(parseSuccess);

    if (!parseSuccess) {
      return;
    }
    VPackSlice body = parsedBody.get()->slice();

    // TODO: validate body
    if (!body.isObject()) {
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          "expecting body to be of the form {name: <string>, type: <string>}");
      return;
    }
    VPackSlice const nameSlice = body.get("name");
    VPackSlice const typeSlice = body.get("type");
    if (!nameSlice.isString() || !typeSlice.isString()) {
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          "expecting body to be of the form {name: <string>, type: <string>}");
      return;
    }

    TRI_voc_cid_t id = 0;
    LogicalView* view = _vocbase->createView(body, id);
    if (view != nullptr) {
      generateOk(rest::ResponseCode::CREATED);
    } else {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "problem creating view");
    }
  } catch (...) {
    // TODO: cleanup?
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_identifier
////////////////////////////////////////////////////////////////////////////////

void RestViewHandler::modifyView() {
  if (_request->payload().isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, 600);
    return;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/view/<view-name>");
    return;
  }

  std::string const& name = suffixes[0];
  LogicalView* view = _vocbase->lookupView(name);
  if (view == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "could not find view by that name");
    return;
  }

  try {
    bool parseSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(parseSuccess);

    if (!parseSuccess) {
      return;
    }
    VPackSlice body = parsedBody.get()->slice();

    // TODO: figure out what to do with body
    generateNotImplemented("PATCH /_api/view/<view-name>");
  } catch (...) {
    // TODO: cleanup?
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_delete
////////////////////////////////////////////////////////////////////////////////

void RestViewHandler::deleteView() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/view/<view-name>");
    return;
  }

  std::string const& name = suffixes[0];
  LogicalView* view = _vocbase->lookupView(name);
  if (view == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "could not find view by that name");
    return;
  }

  int res = _vocbase->dropView(view);
  if (res == TRI_ERROR_NO_ERROR) {
    generateOk();
  } else {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "problem dropping view");
  }
}
