////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RestRedirectHandler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestStatus RestRedirectHandler::execute() {

  std::string const& url = request()->fullUrl();
  std::string prefix = request()->prefix();
  if (prefix.empty()) {
    prefix = request()->requestPath();
  }

  std::string newUrl = _newPrefix + url.substr(prefix.size());
  response()->setHeader(StaticStrings::Location, newUrl);
  response()->setResponseCode(ResponseCode::PERMANENT_REDIRECT);
  return RestStatus::DONE;
}

void RestRedirectHandler::handleError(basics::Exception const&) {}
