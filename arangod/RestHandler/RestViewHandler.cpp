////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Rest/GeneralResponse.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/OperationOrigin.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utilities/NameValidator.h"
#include "VocBase/LogicalView.h"
#include "Logger/LogMacros.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @return the specified vocbase is granted 'level' access
////////////////////////////////////////////////////////////////////////////////
bool canUse(arangodb::auth::Level level, TRI_vocbase_t const& vocbase) {
  return arangodb::ExecContext::current().canUseDatabase(vocbase.name(), level);
}

}  // namespace

using namespace arangodb::basics;

namespace arangodb {

RestViewHandler::RestViewHandler(ArangodServer& server, GeneralRequest* request,
                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

void RestViewHandler::getView(std::string const& nameOrId, bool detailed) {
  auto view = CollectionNameResolver(_vocbase).getView(nameOrId);

  if (!view) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

    return;
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!view->canUse(
          auth::Level::RO)) {  // check auth after ensuring that the view exists
    // auth after ensuring that the view exists
    generateError(
        Result(TRI_ERROR_FORBIDDEN, "insufficient rights to get view"));

    return;
  }

  // skip views for which the full view definition cannot be generated, as per
  // https://github.com/arangodb/backlog/issues/459
  try {
    arangodb::velocypack::Builder viewBuilder;

    viewBuilder.openObject();

    auto res = view->properties(viewBuilder,
                                LogicalDataSource::Serialization::Properties);

    if (!res.ok()) {
      generateError(res);

      return;  // skip view
    }
  } catch (...) {
    generateError(arangodb::Result(TRI_ERROR_INTERNAL));

    return;  // skip view
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();

  auto const context = detailed ? LogicalDataSource::Serialization::Properties
                                : LogicalDataSource::Serialization::List;
  auto res = view->properties(builder, context);

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

void RestViewHandler::createView() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting POST /_api/view");
    events::CreateView(_vocbase.name(), "", TRI_ERROR_BAD_PARAMETER);
    return;
  }

  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    events::CreateView(_vocbase.name(), "", TRI_ERROR_BAD_PARAMETER);
    return;
  }

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "request body is not an object");
    events::CreateView(_vocbase.name(), "", TRI_ERROR_BAD_PARAMETER);
    return;
  }

  if (body.isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON);
    events::CreateView(_vocbase.name(), "", TRI_ERROR_HTTP_CORRUPTED_JSON);
    return;
  }

  auto nameSlice = body.get(StaticStrings::DataSourceName);
  auto typeSlice = body.get(StaticStrings::DataSourceType);

  if (!nameSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting name parameter to be of the form of \"name: "
                  "<string>\"");
    events::CreateView(_vocbase.name(), "", TRI_ERROR_BAD_PARAMETER);
    return;
  }

  if (!typeSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting type parameter to be of the form of \"type: "
                  "<string>\"");
    events::CreateView(_vocbase.name(), nameSlice.copyString(),
                       TRI_ERROR_BAD_PARAMETER);
    return;
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!canUse(auth::Level::RW, _vocbase)) {
    generateError(
        Result(TRI_ERROR_FORBIDDEN, "insufficient rights to create view"));
    events::CreateView(_vocbase.name(), nameSlice.copyString(),
                       TRI_ERROR_FORBIDDEN);
    return;
  }

  try {
    auto& analyzers =
        server().getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    // First refresh our analyzers cache to see all latest changes in analyzers
    auto res = analyzers.loadAvailableAnalyzers(
        _vocbase.name(), transaction::OperationOriginREST{"creating view"});

    if (res.fail()) {
      generateError(res);
      events::CreateView(_vocbase.name(), nameSlice.copyString(),
                         res.errorNumber());
      return;
    }

    LogicalView::ptr view;
    res = LogicalView::create(view, _vocbase, body, true);

    if (!res.ok()) {
      generateError(res);
      events::CreateView(_vocbase.name(), nameSlice.copyString(),
                         res.errorNumber());
      return;
    }

    if (!view) {
      generateError(
          arangodb::Result(TRI_ERROR_INTERNAL, "problem creating view"));
      events::CreateView(_vocbase.name(), nameSlice.copyString(),
                         TRI_ERROR_INTERNAL);
      return;
    }

    velocypack::Builder builder;

    builder.openObject();
    res =
        view->properties(builder, LogicalDataSource::Serialization::Properties);

    if (!res.ok()) {
      generateError(res);
      return;
    }

    builder.close();
    generateResult(rest::ResponseCode::CREATED, builder.slice());
  } catch (basics::Exception const& ex) {
    events::CreateView(_vocbase.name(), nameSlice.copyString(), ex.code());
    generateError(arangodb::Result(ex.code(), ex.message()));
  } catch (...) {
    events::CreateView(_vocbase.name(), nameSlice.copyString(), TRI_errno());
    generateError(arangodb::Result(TRI_errno(), "problem creating view"));
  }
}

void RestViewHandler::modifyView(bool partialUpdate) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 2 ||
      (suffixes[1] != "properties" && suffixes[1] != "rename")) {
    return generateError(
        rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
        "expecting [PUT, PATCH] /_api/view/<view-name>/properties or "
        "PUT /_api/view/<view-name>/rename");
  }

  auto name = basics::StringUtils::urlDecode(suffixes[0]);

  CollectionNameResolver resolver{_vocbase};
  auto view = resolver.getView(name);
  if (!view) {
    return generateError(rest::ResponseCode::NOT_FOUND,
                         TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  bool parseSuccess = false;
  auto body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return;
  }

  auto& analyzers = server().getFeature<iresearch::IResearchAnalyzerFeature>();
  // First refresh our analyzers cache to see all latest changes in analyzers
  auto r = analyzers.loadAvailableAnalyzers(
      _vocbase.name(), transaction::OperationOriginREST{"modifiying view"});
  if (!r.ok()) {
    return generateError(r);
  }

  bool const isRename = suffixes[1] == "rename";
  if (isRename) {
    body = body.get("name");
    if (!body.isString()) {
      return generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                           "expecting \"name\" parameter to be a string");
    }
  }

  // check auth after ensuring that the view exists
  if (!view->canUse(auth::Level::RW)) {
    return generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                         "insufficient rights to modify view");
  }
  velocypack::Builder builder;
  // skip views for which the full view definition cannot be generated, as
  // per https://github.com/arangodb/backlog/issues/459
  builder.openObject();
  r = view->properties(builder, LogicalDataSource::Serialization::Properties);
  if (!r.ok()) {
    return generateError(r);
  }

  if (isRename) {
    // handle rename functionality
    if (view->name() != body.stringView()) {
      // only carry out an actual name change
      r = view->rename(body.copyString());
    }
  } else {
    // handle update functionality
    r = view->properties(body, true, partialUpdate);
  }
  if (!r.ok()) {
    return generateError(r);
  }

  // ensure have the latest definition, by id because name is cached
  // search-alias view and name are modified in place so we can avoid re-reading
  if (!isRename && view->type() != ViewType::kSearchAlias) {
    auto newView = resolver.getView(view->id());
    if (!newView) {
      LOG_TOPIC("f58dc", WARN, Logger::CLUSTER)
          << "Failed to refresh view definition from the Agency";
      return generateResult(rest::ResponseCode::OK, builder.close().slice());
    }
    view = std::move(newView);
  }

  // return updated definition
  builder.clear();
  builder.openObject();
  r = view->properties(builder, LogicalDataSource::Serialization::Properties);
  if (!r.ok()) {
    return generateError(r);
  }
  generateResult(rest::ResponseCode::OK, builder.close().slice());
}

void RestViewHandler::deleteView() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting DELETE /_api/view/<view-name>");

    events::DropView(_vocbase.name(), "", TRI_ERROR_BAD_PARAMETER);
    return;
  }

  auto name = arangodb::basics::StringUtils::urlDecode(suffixes[0]);

  auto allowDropSystem =
      _request->parsedValue(StaticStrings::DataSourceSystem, false);
  auto view = CollectionNameResolver(_vocbase).getView(name);

  if (!view) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

    events::DropView(_vocbase.name(), name,
                     TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return;
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!view->canUse(
          auth::Level::RW)) {  // check auth after ensuring that the view exists
    generateError(
        Result(TRI_ERROR_FORBIDDEN, "insufficient rights to drop view"));

    events::DropView(_vocbase.name(), name, TRI_ERROR_FORBIDDEN);
    return;
  }

  // prevent dropping of system views
  if (!allowDropSystem && view->system()) {
    generateError(
        Result(TRI_ERROR_FORBIDDEN, "insufficient rights to drop system view"));

    events::DropView(_vocbase.name(), name, TRI_ERROR_FORBIDDEN);
    return;
  }

  auto res = view->drop();

  if (!res.ok()) {
    generateError(res);

    return;
  }

  generateOk(rest::ResponseCode::OK, VPackSlice::trueSlice());
}

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
    auto name = arangodb::basics::StringUtils::urlDecode(suffixes[0]);

    getView(name, suffixes.size() > 1);

    return;
  }

  // /_api/view
  arangodb::velocypack::Builder builder;
  bool excludeSystem = _request->parsedValue("excludeSystem", false);

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!canUse(auth::Level::RO, _vocbase)) {
    generateError(
        Result(TRI_ERROR_FORBIDDEN, "insufficient rights to get views"));

    return;
  }

  std::vector<LogicalView::ptr> views;

  LogicalView::enumerate(_vocbase,
                         [&views](LogicalView::ptr const& view) -> bool {
                           views.emplace_back(view);

                           return true;
                         });

  std::sort(views.begin(), views.end(),
            [](std::shared_ptr<LogicalView> const& lhs,
               std::shared_ptr<LogicalView> const& rhs) -> bool {
              return StringUtils::tolower(lhs->name()) <
                     StringUtils::tolower(rhs->name());
            });
  builder.openArray();

  for (auto view : views) {
    if (view && (!excludeSystem || !view->system())) {
      if (!view->canUse(auth::Level::RO)) {  // check auth after ensuring that
                                             // the view exists
        continue;  // skip views that are not authorized to be read
      }

      // skip views for which the full view definition cannot be generated, as
      // per https://github.com/arangodb/backlog/issues/459
      try {
        arangodb::velocypack::Builder viewBuilder;

        viewBuilder.openObject();

        if (!view->properties(viewBuilder,
                              LogicalDataSource::Serialization::Properties)
                 .ok()) {
          continue;  // skip view
        }
      } catch (...) {
        continue;  // skip view
      }

      arangodb::velocypack::Builder viewBuilder;

      viewBuilder.openObject();

      try {
        auto res = view->properties(viewBuilder,
                                    LogicalDataSource::Serialization::List);

        if (!res.ok()) {
          generateError(res);

          return;
        }
      } catch (arangodb::basics::Exception const& e) {
        if (TRI_ERROR_FORBIDDEN != e.code()) {
          throw;  // skip views that are not authorized to be read
        }
      }

      viewBuilder.close();
      builder.add(viewBuilder.slice());
    }
  }

  builder.close();
  generateOk(rest::ResponseCode::OK, builder.slice());
}

}  // namespace arangodb
