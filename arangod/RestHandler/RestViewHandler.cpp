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
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Rest/GeneralResponse.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalView.h"

#include <velocypack/velocypack-aliases.h>

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @return the specified object is granted 'level' access
////////////////////////////////////////////////////////////////////////////////
bool canUse(
    arangodb::auth::Level level,
    TRI_vocbase_t const& vocbase,
    std::string const* dataSource = nullptr // nullptr == validate only vocbase
) {
  auto* execCtx = arangodb::ExecContext::CURRENT;

  return !execCtx
         || (execCtx->canUseDatabase(vocbase.name(), level)
             && (!dataSource
                 || execCtx->canUseCollection(vocbase.name(), *dataSource, level)
                )
            );
}

}

using namespace arangodb::basics;

namespace arangodb {

RestViewHandler::RestViewHandler(GeneralRequest* request,
                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

void RestViewHandler::getView(std::string const& nameOrId, bool detailed) {
  auto view = CollectionNameResolver(_vocbase).getView(nameOrId);

  if (!view) {
    generateError(
      rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
    );

    return;
  }
  
  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!canUse(auth::Level::RO, view->vocbase())) { // as per https://github.com/arangodb/backlog/issues/459
  //if (!canUse(auth::Level::RO, view->vocbase(), &view->name())) { // check auth after ensuring that the view exists
    generateError(Result(TRI_ERROR_FORBIDDEN, "insufficient rights to get view"));

    return;
  }

  // skip views for which the full view definition cannot be generated, as per https://github.com/arangodb/backlog/issues/459
  try {
    arangodb::velocypack::Builder viewBuilder;

    viewBuilder.openObject();

    auto res = view->properties(viewBuilder, true, false);

    if (!res.ok()) {
      generateError(res);

      return; // skip view
    }
  } catch (...) {
    generateError(arangodb::Result(TRI_ERROR_INTERNAL));

    return; // skip view
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();

  auto res = view->properties(builder, detailed, false);

  builder.close();

  if (!res.ok()) {
    generateError(res);

    return;
  }

  generateOk(rest::ResponseCode::OK, builder);
}

RestStatus RestViewHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    createView();
  } else if (type == rest::RequestType::PUT) {
    modifyView(false);
  } else if (type == rest::RequestType::PATCH) {
    modifyView(true);
  } else if (type == rest::RequestType::DELETE_REQ) {
    deleteView();
  } else if (type == rest::RequestType::GET) {
    getViews();
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

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

  if (!nameSlice.isString() || !typeSlice.isString()) {
    badParamError();

    return;
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!canUse(auth::Level::RW, _vocbase)) {
    generateError(Result(TRI_ERROR_FORBIDDEN, "insufficient rights to create view"));

    return;
  }

  try {
    LogicalView::ptr view;
    auto res = LogicalView::create(view, _vocbase, body);

    if (!res.ok()) {
      generateError(res);

      return;
    }

    if (!view) {
      generateError(arangodb::Result(TRI_ERROR_INTERNAL, "problem creating view"));

      return;
    }

    velocypack::Builder builder;

    builder.openObject();
    res = view->properties(builder, true, false);

    if (!res.ok()) {
      generateError(res);

      return;
    }

    builder.close();
    generateResult(rest::ResponseCode::CREATED, builder.slice());
  } catch (basics::Exception const& ex) {
    generateError(arangodb::Result(ex.code(), ex.message()));
  } catch (...) {
    generateError(arangodb::Result(TRI_errno(), "problem creating view"));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_identifier
////////////////////////////////////////////////////////////////////////////////

void RestViewHandler::modifyView(bool partialUpdate) {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if ((suffixes.size() != 2) || (suffixes[1] != "properties" && suffixes[1] != "rename")) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting [PUT, PATCH] /_api/view/<view-name>/properties or PUT /_api/view/<view-name>/rename");

    return;
  }

  std::string const& name = suffixes[0];
  CollectionNameResolver resolver(_vocbase);
  auto view = resolver.getView(name);

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

      // .......................................................................
      // end of parameter parsing
      // .......................................................................

      if (!canUse(auth::Level::RW, view->vocbase())) { // as per https://github.com/arangodb/backlog/issues/459
      //if (!canUse(auth::Level::RW, view->vocbase(), &view->name())) { // check auth after ensuring that the view exists
        generateError(Result(TRI_ERROR_FORBIDDEN, "insufficient rights to rename view"));

        return;
      }

      // skip views for which the full view definition cannot be generated, as per https://github.com/arangodb/backlog/issues/459
      try {
        arangodb::velocypack::Builder viewBuilder;

        viewBuilder.openObject();

        auto res = view->properties(viewBuilder, true, false);

        if (!res.ok()) {
          generateError(res);

          return; // skip view
        }
      } catch (...) {
        generateError(arangodb::Result(TRI_ERROR_INTERNAL));

        return; // skip view
      }

      auto res = view->rename(newName.copyString());

      if (res.ok()) {
        getView(view->name(), false);
      } else {
        generateError(res);
      }

      return;
    }

    // .........................................................................
    // end of parameter parsing
    // .........................................................................

    if (!canUse(auth::Level::RW, view->vocbase())) { // as per https://github.com/arangodb/backlog/issues/459
    //if (!canUse(auth::Level::RW, view->vocbase(), &view->name())) { // check auth after ensuring that the view exists
      generateError(Result(TRI_ERROR_FORBIDDEN, "insufficient rights to modify view"));

      return;
    }

    // check ability to read current properties
    {
      arangodb::velocypack::Builder builderCurrent;

      builderCurrent.openObject();

      auto resCurrent = view->properties(builderCurrent, true, false);

      if (!resCurrent.ok()) {
        generateError(resCurrent);

        return;
      }
    }

    auto result = view->properties(body, partialUpdate);

    if (!result.ok()) {
      generateError(result);

      return;
    }

    view = resolver.getView(view->id()); // ensure have the latest definition

    if (!view) {
      generateError(arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));

      return;
    }

    arangodb::velocypack::Builder updated;

    updated.openObject();

    auto res = view->properties(updated, true, false);

    updated.close();

    if (!res.ok()) {
      generateError(res);

      return;
    }

    generateResult(rest::ResponseCode::OK, updated.slice());
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
  auto view = CollectionNameResolver(_vocbase).getView(name);

  if (!view) {
    generateError(
      rest::ResponseCode::NOT_FOUND,
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
    );

    return;
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!canUse(auth::Level::RW, view->vocbase())) { // as per https://github.com/arangodb/backlog/issues/459
  //if (!canUse(auth::Level::RW, view->vocbase(), &view->name())) { // check auth after ensuring that the view exists
    generateError(Result(TRI_ERROR_FORBIDDEN, "insufficient rights to drop view"));

    return;
  }

  // prevent dropping of system views
  if (!allowDropSystem && view->system()) {
    generateError(Result(TRI_ERROR_FORBIDDEN, "insufficient rights to drop system view"));

    return;
  }

  auto res = view->drop();

  if (!res.ok()) {
    generateError(res);

    return;
  }

  generateOk(rest::ResponseCode::OK, VPackSlice::trueSlice());
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

  // /_api/view/<name>[/properties]
  if (!suffixes.empty()) {
    getView(suffixes[0], suffixes.size() > 1);

    return;
  }

  // /_api/view
  arangodb::velocypack::Builder builder;
  bool excludeSystem = _request->parsedValue("excludeSystem", false);

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!canUse(auth::Level::RO, _vocbase)) {
    generateError(Result(TRI_ERROR_FORBIDDEN, "insufficient rights to get views"));

    return;
  }

  std::vector<LogicalView::ptr> views;

  LogicalView::enumerate(
    _vocbase,
    [&views](LogicalView::ptr const& view)->bool {
      views.emplace_back(view);

      return true;
    }
  );

  std::sort(
    views.begin(),
    views.end(),
    [](std::shared_ptr<LogicalView> const& lhs, std::shared_ptr<LogicalView> const& rhs) -> bool {
      return StringUtils::tolower(lhs->name()) < StringUtils::tolower(rhs->name());
    }
  );
  builder.openArray();

  for (auto view: views) {
    if (view && (!excludeSystem || !view->system())) {
      if (!canUse(auth::Level::RO, view->vocbase())) { // as per https://github.com/arangodb/backlog/issues/459
      //if (!canUse(auth::Level::RO, view->vocbase(), &view->name())) {
        continue; // skip views that are not authorized to be read
      }

      // skip views for which the full view definition cannot be generated, as per https://github.com/arangodb/backlog/issues/459
      try {
        arangodb::velocypack::Builder viewBuilder;

        viewBuilder.openObject();

        if (!view->properties(viewBuilder, true, false).ok()) {
          continue; // skip view
        }
      } catch (...) {
        continue; // skip view
      }

      arangodb::velocypack::Builder viewBuilder;

      viewBuilder.openObject();

      try {
        auto res = view->properties(viewBuilder, false, false);

        if (!res.ok()) {
          generateError(res);

          return;
        }
      } catch (arangodb::basics::Exception const& e) {
        if (TRI_ERROR_FORBIDDEN != e.code()) {
          throw; // skip views that are not authorized to be read
        }
      }

      viewBuilder.close();
      builder.add(viewBuilder.slice());
    }
  }

  builder.close();
  generateOk(rest::ResponseCode::OK, builder.slice());
}

} // arangodb
