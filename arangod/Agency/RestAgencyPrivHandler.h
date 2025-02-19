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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

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
  RestAgencyPrivHandler(ArangodServer&, GeneralRequest*, GeneralResponse*,
                        consensus::Agent*);

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
