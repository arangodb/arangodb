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

#include "RestPleaseUpgradeHandler.h"

#include "Rest/HttpResponse.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestPleaseUpgradeHandler::RestPleaseUpgradeHandler(GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestHandler(request, response) {}

bool RestPleaseUpgradeHandler::isDirect() const { return true; }

RestHandler::status RestPleaseUpgradeHandler::execute() {
  // TODO needs to generalized
  auto response = dynamic_cast<HttpResponse*>(_response.get());

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  resetResponse(rest::ResponseCode::OK);
  response->setContentType(rest::ContentType::TEXT);

  auto& buffer = response->body();
  buffer.appendText("Database: ");
  buffer.appendText(_request->databaseName());
  buffer.appendText("\r\n\r\n");
  buffer.appendText("It appears that your database must be upgraded. ");
  buffer.appendText("Normally this can be done using\r\n\r\n");
  buffer.appendText("  /etc/init.d/arangodb3 stop\r\n");
  buffer.appendText("  /etc/init.d/arangodb3 upgrade\r\n");
  buffer.appendText("  /etc/init.d/arangodb3 start\r\n\r\n");
  buffer.appendText("Please check the log file for details.\r\n");

  return status::DONE;
}

void RestPleaseUpgradeHandler::handleError(const Exception&) {}
