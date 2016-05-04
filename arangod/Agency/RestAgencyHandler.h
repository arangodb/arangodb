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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_AGENCY_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_AGENCY_HANDLER_H 1

#include "RestHandler/RestBaseHandler.h"
#include "Agency/Agent.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief REST handler for outside calls to agency (write & read)
////////////////////////////////////////////////////////////////////////////////

class RestAgencyHandler : public RestBaseHandler {
 public:
  RestAgencyHandler(HttpRequest*, consensus::Agent*);

  bool isDirect() const override;

  status_t execute() override;

 private:
  status_t reportErrorEmptyRequest();
  status_t reportTooManySuffices();
  status_t reportUnknownMethod();
  status_t handleStores();
  status_t handleRead();
  status_t handleWrite();
  status_t handleConfig();
  status_t reportMethodNotAllowed();
  status_t handleState();

  void redirectRequest(arangodb::consensus::id_t leaderId);
  consensus::Agent* _agent;
};
}

#endif
