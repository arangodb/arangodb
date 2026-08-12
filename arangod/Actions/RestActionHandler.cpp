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
////////////////////////////////////////////////////////////////////////////////

#include "RestActionHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Actions/actions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Statistics/RequestStatistics.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {
using application_features::ApplicationServer;

RestActionHandler::RestActionHandler(ApplicationServer& server,
                                     GeneralRequest* request,
                                     GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _action(TRI_LookupActionVocBase(request)),
      _data(nullptr) {}

// Mounted at / (prefix, catchall)
RestStatus RestActionHandler::execute() {
  // need an action
  if (_action == nullptr) {
    generateNotImplemented(_request->fullUrl());
    return RestStatus::DONE;
  }

  ExecContextSuperuserScope escope(_mustEscalateToSuperuser);

  // extract the sub-request type
  rest::RequestType type = _request->requestType();

  // execute one of the HTTP methods
  switch (type) {
    case rest::RequestType::GET:
    case rest::RequestType::POST:
    case rest::RequestType::PUT:
    case rest::RequestType::DELETE_REQ:
    case rest::RequestType::HEAD:
    case rest::RequestType::OPTIONS:
    case rest::RequestType::PATCH: {
      executeAction();
      break;
    }

    default:
      generateNotImplemented("METHOD");
      break;
  }

  return RestStatus::DONE;
}

void RestActionHandler::cancel() {
  RestVocbaseBaseHandler::cancel();
  _action->cancel(&_dataLock, &_data);
}

/// @brief executes an action
void RestActionHandler::executeAction() {
  // handle redirections for web interface here
  rest::RequestType type = _request->requestType();
  if (type == rest::RequestType::GET) {
    std::vector<std::string> const& suffixes = _request->decodedSuffixes();
    if (suffixes.empty() || (suffixes.size() == 2 && suffixes[0] == "_admin" &&
                             suffixes[1] == "html")) {
      // request to just /
      auto& gs = server().getFeature<GeneralServerFeature>();
      _response->setResponseCode(gs.permanentRootRedirect()
                                     ? rest::ResponseCode::MOVED_PERMANENTLY
                                     : rest::ResponseCode::FOUND);
      _response->setHeaderNC(StaticStrings::Location,
                             "/_db/" + StringUtils::urlEncode(_vocbase.name()) +
                                 gs.redirectRootTo());
      return;
    }
  }

  TRI_action_result_t result = _action->execute(
      &_vocbase, _request.get(), _response.get(), &_dataLock, &_data);

  if (!result.isValid) {
    if (result.canceled) {
      generateCanceled();
    } else {
      generateNotImplemented(_action->_url);
    }
  }
}

// The aardvark web UI serves a handful of static assets and harmless
// metadata that must be reachable before the user has logged in (so the
// UI itself can load and offer a login form). Unlike the rest of
// /_admin/aardvark/, none of these need any database access, so they must
// never be granted superuser rights - doing so previously allowed
// unauthenticated AQL execution and other superuser-level operations via
// any path merely prefixed with /_admin/aardvark/.

bool isPublicAardvarkPath(std::string_view path) {
  using namespace std::string_view_literals;
  constexpr std::array exact = {
      "/_admin/aardvark/index.html"sv,
      "/_admin/aardvark/config.js"sv,
      "/_admin/aardvark/whoAmI"sv,
  };
  constexpr std::array prefixes = {
      "/_admin/aardvark/static/"sv,
      "/_admin/aardvark/img/"sv,
  };
  return std::ranges::any_of(exact, [&](auto p) { return path == p; }) ||
         std::ranges::any_of(prefixes,
                             [&](auto p) { return path.starts_with(p); });
}

async<RestHandler::AuthenticationGrant>
RestActionHandler::checkUserAuthentication() const {
  if (isPublicAardvarkPath(request()->requestPath())) {
    // Note that we do **not** escalate to superuser for these!
    co_return AuthenticationGrant::GRANTED_EARLY;
  }

  co_return co_await RestHandler::checkUserAuthentication();
}

bool RestActionHandler::hasAllowedUnauthenticatedPath() const {
  auto const* const auth = AuthenticationFeature::instance();
  ADB_PROD_ASSERT(auth->isActive());
  auto const& path = request()->requestPath();
  return auth->authenticationSystemOnly() &&  // TODO remove in 4.0
         !path.empty() && !path.starts_with("/_");
}

async<Result> RestActionHandler::checkApiVersionAccess() const {
  if (hasAllowedUnauthenticatedPath()) {
    co_return Result{};
  }

  co_return co_await RestHandler::checkApiVersionAccess();
}

async<Result> RestActionHandler::checkDatabaseAccess() const {
  auto r = co_await RestHandler::checkDatabaseAccess();
  if (r.ok()) {
    co_return Result{};
  }

  if (hasAllowedUnauthenticatedPath()) {
    // check if path is / which is required for the web UI to get started
    _mustEscalateToSuperuser = true;
    co_return Result{};
  }
  co_return r;
}

}  // namespace arangodb
