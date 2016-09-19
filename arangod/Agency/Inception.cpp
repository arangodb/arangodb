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

    // Build gossip message
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

    std::string path = privApiPrefix + "gossip";

    // gossip peers
    for (auto const& p : config.gossipPeers()) {
      if (p != config.endpoint()) {
        std::string clientid = config.id() + std::to_string(i++);
        auto hf =
            std::make_unique<std::unordered_map<std::string, std::string>>();
        arangodb::ClusterComm::instance()->asyncRequest(
          clientid, 1, p, rest::RequestType::POST, path,
          std::make_shared<std::string>(out->toJson()), hf,
          std::make_shared<GossipCallback>(_agent), 1.0, true, 0.5);
      }
    }
    
    // pool entries
    for (auto const& pair : config.pool()) {
      if (pair.second != config.endpoint()) {
        std::string clientid = config.id() + std::to_string(i++);
        auto hf =
            std::make_unique<std::unordered_map<std::string, std::string>>();
        arangodb::ClusterComm::instance()->asyncRequest(
          clientid, 1, pair.second, rest::RequestType::POST, path,
          std::make_shared<std::string>(out->toJson()), hf,
          std::make_shared<GossipCallback>(_agent), 1.0, true, 0.5);
      }
    }

    // don't panic
    _cv.wait(100000);

    // Timed out? :(
    if ((std::chrono::system_clock::now() - s) > timeout) {
      if (config.poolComplete()) {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Stopping active gossipping!";
      } else {
        LOG_TOPIC(ERR, Logger::AGENCY)
            << "Failed to find complete pool of agents. Giving up!";
      }
      break;
    }

    // We're done
    if (config.poolComplete()) {
      _agent->startConstituent();
      break;
    }
    
  }
  
}


// @brief Active agency from persisted database
bool Inception::activeAgencyFromPersistence() {

  auto myConfig = _agent->config();
  std::string const path = pubApiPrefix + "config";

  // Can only be done responcibly, if we are complete
  if (myConfig.poolComplete()) {

    // Contact hosts on pool in hopes of finding a leader Id
    for (auto const& pair : myConfig.pool()) {

      if (pair.first != myConfig.id()) {

        auto comres = arangodb::ClusterComm::instance()->syncRequest(
          myConfig.id(), 1, pair.second, rest::RequestType::GET, path,
          std::string(), std::unordered_map<std::string, std::string>(), 1.0);
        
        if (comres->status == CL_COMM_SENT) {

          auto body = comres->result->getBodyVelocyPack();
          auto theirConfig = body->slice();
          
          std::string leaderId;

          // LeaderId in configuration?
          try {
            leaderId = theirConfig.get("leaderId").copyString();
          } catch (std::exception const& e) {
            LOG_TOPIC(DEBUG, Logger::AGENCY)
              << "Failed to get leaderId from" << pair.second << ": "
              << e.what();
          }

          if (leaderId != "") { // Got leaderId. Let's get do it. 

            try {
              LOG_TOPIC(DEBUG, Logger::AGENCY)
                << "Found active agency with leader " << leaderId
                << " at endpoint "
                << theirConfig.get("configuration").get(
                  "pool").get(leaderId).copyString();
            } catch (std::exception const& e) {
              LOG_TOPIC(DEBUG, Logger::AGENCY)
                << "Failed to get leaderId from" << pair.second << ": "
                << e.what();
            }

            auto agency = std::make_shared<Builder>();
            agency->openObject();
            agency->add("term", theirConfig.get("term"));
            agency->add("id", VPackValue(leaderId));
            agency->add("active", theirConfig.get("configuration").get("active"));
            agency->add("pool", theirConfig.get("configuration").get("pool"));
            agency->close();
            _agent->notify(agency);
            
            return true;
            
          } else { // No leaderId. Move on.

            LOG_TOPIC(DEBUG, Logger::AGENCY)
              << "Failed to get leaderId from" << pair.second;

          }
          
        }
        
      }
      
    }
  }

  return false;
  
}


bool Inception::restartingActiveAgent() {

  auto myConfig = _agent->config();
  std::string const path = pubApiPrefix + "config";

  auto s = std::chrono::system_clock::now();
  std::chrono::seconds timeout(60);
  
  // Can only be done responcibly, if we are complete
  if (myConfig.poolComplete()) {
    
    auto pool = myConfig.pool();
    auto active = myConfig.active();
    
    CONDITION_LOCKER(guard, _cv);

    while (!this->isStopping() && !_agent->isStopping()) {

      active.erase(
        std::remove(active.begin(), active.end(), myConfig.id()), active.end());
      active.erase(
        std::remove(active.begin(), active.end(), ""), active.end());

      if (active.empty()) {
        return true;
      }

      for (auto& i : active) {
        
        if (i != myConfig.id() && i != "") {
          
          auto clientId = myConfig.id();
          auto comres   = arangodb::ClusterComm::instance()->syncRequest(
            clientId, 1, pool.at(i), rest::RequestType::GET, path, std::string(),
            std::unordered_map<std::string, std::string>(), 2.0);
          
          if (comres->status == CL_COMM_SENT) {

            try {

              auto theirActive = comres->result->getBodyVelocyPack()->
                slice().get("configuration").get("active").toJson();
              auto myActive = myConfig.activeToBuilder()->toJson();
              
              if (theirActive != myActive) {
                LOG_TOPIC(FATAL, Logger::AGENCY)
                  << "Assumed active RAFT peer and I disagree on active membership."
                  << "Administrative intervention needed.";
                FATAL_ERROR_EXIT();
                return false;
              } else {
                i = "";
              }

            } catch (std::exception const& e) {
              LOG_TOPIC(FATAL, Logger::AGENCY)
                << "Assumed active RAFT peer has no active agency list: " << e.what()
                << "Administrative intervention needed.";
                FATAL_ERROR_EXIT();
                return false;
            }
            
          } else {
            /*
              LOG_TOPIC(FATAL, Logger::AGENCY)
              << "Assumed active RAFT peer and I disagree on active membership."
              << "Administrative intervention needed.";
              FATAL_ERROR_EXIT();
              return false;*/
          }
        }
        
      }
      
      // Timed out? :(
      if ((std::chrono::system_clock::now() - s) > timeout) {
        if (myConfig.poolComplete()) {
          LOG_TOPIC(DEBUG, Logger::AGENCY) << "Stopping active gossipping!";
        } else {
          LOG_TOPIC(ERR, Logger::AGENCY)
            << "Failed to find complete pool of agents. Giving up!";
        }
        break;
      }
      
      _cv.wait(500000);

    }
  }

  return false;
    

}
  

// @brief Active agency from persisted database
bool Inception::activeAgencyFromCommandLine() {
  return false;
}

// @brief Thread main
void Inception::run() {

  // 1. If active agency, do as you're told
  if (activeAgencyFromPersistence()) {
    _agent->ready(true);  
    return;
  }

  // 2. If we think that we used to be active agent
  if (restartingActiveAgent()) {
  _agent->ready(true);  
    return;
  }

  // 3. Else gossip
  config_t config = _agent->config();
  if (!config.poolComplete()) {
    gossip();
  }

  _agent->ready(true);

}

// @brief Graceful shutdown
void Inception::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}
