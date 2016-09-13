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

#include "Inception.h"

#include "Agency/Agent.h"
#include "Agency/GossipCallback.h"
#include "Basics/ConditionLocker.h"
#include "Cluster/ClusterComm.h"

#include <chrono>
#include <thread>

using namespace arangodb::consensus;

Inception::Inception() : Thread("Inception"), _agent(nullptr) {}

Inception::Inception(Agent* agent) : Thread("Inception"), _agent(agent) {}

// Shutdown if not already
Inception::~Inception() { shutdown(); }

/// Gossip to others
/// - Get snapshot of gossip peers and agent pool
/// - Create outgoing gossip.
/// - Send to all peers

void Inception::gossip() {
  auto s = std::chrono::system_clock::now();
  std::chrono::seconds timeout(120);
  size_t i = 0;

  CONDITION_LOCKER(guard, _cv);
  
  while (!this->isStopping() && !_agent->isStopping()) {
    config_t config = _agent->config();  // get a copy of conf

    query_t out = std::make_shared<Builder>();
    out->openObject();
    out->add("endpoint", VPackValue(config.endpoint()));
    out->add("id", VPackValue(config.id()));
    out->add("pool", VPackValue(VPackValueType::Object));
    for (auto const& i : config.pool()) {
      out->add(i.first, VPackValue(i.second));
    }
    out->close();
    out->close();

    std::string path = "/_api/agency_priv/gossip";

    for (auto const& p : config.gossipPeers()) { // gossip peers
      if (p != config.endpoint()) {
        std::string clientid = config.id() + std::to_string(i++);
        auto hf =
            std::make_unique<std::unordered_map<std::string, std::string>>();
        arangodb::ClusterComm::instance()->asyncRequest(
          clientid, 1, p, rest::RequestType::POST, path,
          std::make_shared<std::string>(out->toJson()), hf,
          std::make_shared<GossipCallback>(_agent), 1.0, true);
      }
    }

    for (auto const& pair : config.pool()) { // pool entries
      if (pair.second != config.endpoint()) {
        std::string clientid = config.id() + std::to_string(i++);
        auto hf =
            std::make_unique<std::unordered_map<std::string, std::string>>();
        arangodb::ClusterComm::instance()->asyncRequest(
          clientid, 1, pair.second, rest::RequestType::POST, path,
          std::make_shared<std::string>(out->toJson()), hf,
          std::make_shared<GossipCallback>(_agent), 1.0, true);
      }
    }

    _cv.wait(100000);

    if ((std::chrono::system_clock::now() - s) > timeout) {
      if (config.poolComplete()) {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Stopping active gossipping!";
      } else {
        LOG_TOPIC(ERR, Logger::AGENCY)
            << "Failed to find complete pool of agents. Giving up!";
      }
      break;
    }

    if (config.poolComplete()) {
      _agent->startConstituent();
      break;
    }
  }
}

void Inception::activeAgency() {  // Do we have an active agency?

  auto config = _agent->config();
  std::string const path = pubApiPrefix + "config";
  
  if (config.poolComplete()) {

    for (auto const& pair : config.pool()) { // pool entries

      if (pair.first != config.id()) {

        std::string clientId = config.id();
        auto comres = arangodb::ClusterComm::instance()->syncRequest(
          clientId, 1, pair.second, rest::RequestType::GET, path, std::string(),
          std::unordered_map<std::string, std::string>(), 1.0);
        
        if (comres->status == CL_COMM_SENT) {

          auto body = comres->result->getBodyVelocyPack();
          auto config = body->slice();
          
          std::string leaderId;

          try {
            leaderId = config.get("leaderId").copyString();
          } catch (std::exception const& e) {
            LOG_TOPIC(DEBUG, Logger::AGENCY)
              << "Failed to get leaderId from" << pair.second << ": "
              << e.what();
          }

          if (leaderId != "") {
            try {
              LOG_TOPIC(DEBUG, Logger::AGENCY)
                << "Found active agency with leader " << leaderId
                << " at endpoint "
                << config.get("configuration").get(
                  "pool").get(leaderId).copyString();
            } catch (std::exception const& e) {
              LOG_TOPIC(DEBUG, Logger::AGENCY)
                << "Failed to get leaderId from" << pair.second << ": "
                << e.what();
            }

            auto agency = std::make_shared<Builder>();
            agency->openObject();
            agency->add("term", config.get("term"));
            agency->add("id", VPackValue(leaderId));
            agency->add("active", config.get("configuration").get("active"));
            agency->add("pool", config.get("configuration").get("pool"));
            agency->close();
            _agent->notify(agency);
            
            break;
            
          } else {

            LOG_TOPIC(DEBUG, Logger::AGENCY)
              << "Failed to get leaderId from" << pair.second;

          }
          
        }
        
      }
      
    }
  }
  
}

void Inception::run() {
  activeAgency();

  config_t config = _agent->config();
  if (!config.poolComplete()) {
    gossip();
  }
}

void Inception::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}
