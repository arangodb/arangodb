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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminDatabaseHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Rest/Version.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestAdminDatabaseHandler::RestAdminDatabaseHandler(application_features::ApplicationServer& server,
                                                   GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestAdminDatabaseHandler::execute() {
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("version", VPackValue(std::to_string(Version::getNumericServerVersion())));
  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code,
             VPackValue(static_cast<int>(rest::ResponseCode::OK)));  // hard-coded
  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
