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
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Rest/GeneralResponse.h"
#include "VocBase/LogicalView.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;

namespace arangodb {

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

  if (type == rest::RequestType::PUT) {
    modifyView(false);
    return RestStatus::DONE;
  }

  if (type == rest::RequestType::PATCH) {
    modifyView(true);
    return RestStatus::DONE;
  }

  if (type == rest::RequestType::DELETE_REQ) {
    deleteView();
    return RestStatus::DONE;
  }

  if (type == rest::RequestType::GET) {
    getViews();
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
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON);
    return;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting POST /_api/view");
    return;
  }

  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    return;
  }

  auto badParamError = [&]() -> void {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting body to be of the form {name: <string>, type: "
                  "<string>, properties: <object>}");
  };

  if (!body.isObject()) {
    badParamError();
    return;
  }

  auto nameSlice = body.get(StaticStrings::DataSourceName);
  auto typeSlice = body.get(StaticStrings::DataSourceType);
  VPackSlice const propertiesSlice = body.get("properties");

  if (!nameSlice.isString() || !typeSlice.isString() ||
      !propertiesSlice.isObject()) {
    badParamError();
    return;
  }

  try {
    auto view = _vocbase.createView(body);

    if (view != nullptr) {
      VPackBuilder props;
      props.openObject();
      view->toVelocyPack(props, true, false);
      props.close();
      generateResult(rest::ResponseCode::CREATED, props.slice());
    } else {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "problem creating view");
    }
  } catch (basics::Exception const& ex) {
    generateError(GeneralResponse::responseCode(ex.code()), ex.code(), ex.message());
  } catch (...) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "problem creating view");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_identifier
////////////////////////////////////////////////////////////////////////////////

void RestViewHandler::modifyView(bool partialUpdate) {
  if (_request->payload().isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON);
    return;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  if ((suffixes.size() != 2) || (suffixes[1] != "properties" && suffixes[1] != "rename")) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting [PUT, PATCH] /_api/view/<view-name>/properties or PUT /_api/view/<view-name>/rename");
    return;
  }

  std::string const& name = suffixes[0];
  auto view = _vocbase.lookupView(name);

  if (view == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return;
  }

  try {
    bool parseSuccess = false;
    VPackSlice const body = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {
      return;
    }

    // handle rename functionality
    if (suffixes[1] == "rename") {
      VPackSlice newName = body.get("name");
      if (!newName.isString()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "expecting \"name\" parameter to be a string");
        return;
      }

      int res = _vocbase.renameView(view, newName.copyString());

      if (res == TRI_ERROR_NO_ERROR) {
        getSingleView(newName.copyString());
      } else {
        generateError(GeneralResponse::responseCode(res), res);
      }
      return;
    }

    auto const result = view->updateProperties(
      body, partialUpdate, true
    );  // TODO: not force sync?

    if (result.ok()) {
      VPackBuilder updated;
      updated.openObject();
      view->toVelocyPack(updated, false, false);
      updated.close();
      generateResult(rest::ResponseCode::OK, updated.slice());
      return;
    } else {
      generateError(GeneralResponse::responseCode(result.errorNumber()), result.errorNumber(), result.errorMessage());
      return;
    }
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
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting DELETE /_api/view/<view-name>");

    return;
  }

  std::string const& name = suffixes[0];
  auto allowDropSystem = _request->parsedValue("isSystem", false);
  auto view = _vocbase.lookupView(name);

  if (!view) {
    generateError(
      rest::ResponseCode::NOT_FOUND,
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
    );

    return;
  }

  auto res = _vocbase.dropView(view->id(), allowDropSystem).errorNumber();

  if (res == TRI_ERROR_NO_ERROR) {
    generateOk(rest::ResponseCode::OK, VPackSlice::trueSlice());
  } else if (res == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  } else {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "problem dropping view");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_delete
////////////////////////////////////////////////////////////////////////////////

void RestViewHandler::getViews() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() > 2 ||
      ((suffixes.size() == 2) && (suffixes[1] != "properties"))) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting GET /_api/view[/<view-name>[/properties]]");
    return;
  }

  if (suffixes.size() == 0) {
    getListOfViews();
    return;
  } else {
    std::string const& name = suffixes[0];
    if (suffixes.size() == 1) {
      getSingleView(name);
    } else {
      getViewProperties(name);
    }
    return;
  }
}

void RestViewHandler::getListOfViews() {
  auto views = _vocbase.views();

  std::sort(views.begin(), views.end(),
            [](std::shared_ptr<LogicalView> const& lhs,
               std::shared_ptr<LogicalView> const& rhs) -> bool {
              return StringUtils::tolower(lhs->name()) <
                     StringUtils::tolower(rhs->name());
            });

  VPackBuilder props;
  props.openArray();
  for (std::shared_ptr<LogicalView> view : views) {
    if (view.get() != nullptr) {
      props.openObject();
      view->toVelocyPack(props, true, false);
      props.close();
    }
  }
  props.close();
  generateResult(rest::ResponseCode::OK, props.slice());
}

void RestViewHandler::getSingleView(std::string const& name) {
  auto view = _vocbase.lookupView(name);

  if (view.get() != nullptr) {
    VPackBuilder props;
    props.openObject();
    view->toVelocyPack(props, true, false);
    props.close();
    generateResult(rest::ResponseCode::OK, props.slice());
  } else {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
}

void RestViewHandler::getViewProperties(std::string const& name) {
  auto view = _vocbase.lookupView(name);

  if (view.get() != nullptr) {
    VPackBuilder props;
    props.openObject();
    view->toVelocyPack(props, true, false);
    props.close();
    generateResult(rest::ResponseCode::OK, props.slice().get("properties"));
  } else {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
}

} // arangodb
