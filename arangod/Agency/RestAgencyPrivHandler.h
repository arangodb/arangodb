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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_AGENCY_PRIV_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_AGENCY_PRIV_HANDLER_H 1

#include "Agency/Agent.h"
#include "Logger/LogMacros.h"
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief REST handler for private agency communication
///        (vote, appendentries, notify)
////////////////////////////////////////////////////////////////////////////////

class RestAgencyPrivHandler : public arangodb::RestBaseHandler {
 public:
  RestAgencyPrivHandler(application_features::ApplicationServer&,
                        GeneralRequest*, GeneralResponse*, consensus::Agent*);

 public:
  char const* name() const override final { return "RestAgencyPrivHandler"; }

  RequestLane lane() const override final {
    return RequestLane::AGENCY_INTERNAL;
  }

  RestStatus execute() override;

 private:

  RestStatus reportErrorEmptyRequest();
  RestStatus reportTooManySuffices();
  RestStatus reportBadQuery(std::string const& message = "bad parameter");
  RestStatus reportMethodNotAllowed();
  RestStatus reportGone();
  RestStatus reportMessage(arangodb::rest::ResponseCode, std::string const&);
  RestStatus reportError(VPackSlice);
  void redirectRequest(std::string const& leaderId);

  consensus::Agent* _agent;
};

}  // namespace arangodb

#endif
