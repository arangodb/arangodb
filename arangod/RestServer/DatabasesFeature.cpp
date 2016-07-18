////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "DatabasesFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/server.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;

TRI_server_t* DatabasesFeature::SERVER;

DatabasesFeature::DatabasesFeature(ApplicationServer* server)
    : ApplicationFeature(server, "Databases"),
      _server(nullptr) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("DatabasePath");
}

void DatabasesFeature::prepare() {
  // create the server
  _server.reset(new TRI_server_t());
  SERVER = _server.get();
}

void DatabasesFeature::unprepare() {
  // delete the server
  TRI_StopServer(_server.get());
  SERVER = nullptr;
  _server.reset(nullptr);
}
