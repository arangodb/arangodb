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
#include "Futures/Future.h"
#include "Futures/Unit.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief REST handler for outside calls to agency (write & read)
////////////////////////////////////////////////////////////////////////////////

class RestAgencyHandler : public RestVocbaseBaseHandler {
 public:
  RestAgencyHandler(ArangodServer&, GeneralRequest*, GeneralResponse*,
                    consensus::Agent*);

 public:
  char const* name() const override final { return "RestAgencyHandler"; }

  RequestLane lane() const override final {
    return RequestLane::AGENCY_CLUSTER;
  }

  auto executeAsync() -> futures::Future<futures::Unit> override;
  /**
   * @brief Async call to Agent poll with index to wait for within timeout
   */
  auto pollIndex(consensus::index_t const& index, double const& timeout)
      -> async<void>;

 private:
  void reportErrorEmptyRequest();
  void reportTooManySuffices();
  void reportUnknownMethod();
  void reportMessage(arangodb::rest::ResponseCode, std::string const&);
  void handleStores();
  void handleStore();
  void handleRead();
  void handleWrite();
  void handleTransact();
  void handleConfig();
  void reportMethodNotAllowed();
  void handleState();
  void handleTransient();
  void handleInquire();
  auto handlePoll() -> async<void>;

  void redirectRequest(std::string const& leaderId);
  consensus::Agent* _agent;
};
}  // namespace arangodb
