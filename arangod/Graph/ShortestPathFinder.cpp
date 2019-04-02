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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ShortestPathFinder.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/ShortestPathOptions.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::graph;

ShortestPathFinder::ShortestPathFinder(ShortestPathOptions& options)
    : _options(options),
      _httpRequests(0) {}

void ShortestPathFinder::destroyEngines() {
  if (ServerState::instance()->isCoordinator()) {
    // We have to clean up the engines in Coordinator Case.
    auto cc = ClusterComm::instance();

    if (cc != nullptr) {
      auto ch = reinterpret_cast<ClusterTraverserCache*>(_options.cache());
      // nullptr only happens on controlled server shutdown
      std::string const url(
          "/_db/" +
          arangodb::basics::StringUtils::urlEncode(_options.trx()->vocbase().name()) +
          "/_internal/traverser/");

      for (auto const& it : *ch->engines()) {
        incHttpRequests(1);
        arangodb::CoordTransactionID coordTransactionID = TRI_NewTickServer();
        std::unordered_map<std::string, std::string> headers;
        auto res = cc->syncRequest(coordTransactionID, "server:" + it.first,
                                   RequestType::DELETE_REQ,
                                   url + arangodb::basics::StringUtils::itoa(it.second),
                                   "", headers, 30.0);

        if (res->status != CL_COMM_SENT) {
          // Note If there was an error on server side we do not have
          // CL_COMM_SENT
          std::string message("Could not destroy all traversal engines");

          if (!res->errorMessage.empty()) {
            message += std::string(": ") + res->errorMessage;
          }

          LOG_TOPIC("d31a4", ERR, arangodb::Logger::FIXME) << message;
        }
      }
    }
  }
}

ShortestPathOptions& ShortestPathFinder::options() const { return _options; }
