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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestTimeHandler.h"

#include "Basics/StaticStrings.h"
#include "Basics/system-functions.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestTimeHandler::RestTimeHandler(application_features::ApplicationServer& server,
                                 GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestTimeHandler::execute() {
  VPackBuilder result;
  result.openObject(true);
  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code, VPackValue(static_cast<int>(rest::ResponseCode::OK)));
  result.add("time", VPackValue(TRI_microtime()));
  result.close();
  
  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
