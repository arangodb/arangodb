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
#include "GeneralServer/RestHandlerFactory.h"

#include <chrono>
#include <iomanip>
#include <numeric>
#include <thread>

using namespace arangodb::consensus;

Inception::Inception() : Thread("Inception"), _agent(nullptr) {}

Inception::Inception(Agent* agent) : Thread("Inception"), _agent(agent) {}

// Shutdown if not already
Inception::~Inception() {
  if (!isStopping()) {
    shutdown();
  }
}

/// Gossip to others
/// - Get snapshot of gossip peers and agent pool
/// - Create outgoing gossip.
/// - Send to all peers
void Inception::gossip() {

  if (this->isStopping() || _agent->isStopping()) {
    return;
  }
  
  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    // nullptr only happens during controlled shutdown
    return;
  }

  LOG_TOPIC(INFO, Logger::AGENCY) << "Entering gossip phase ...";
  using namespace std::chrono;
  
  auto startTime = system_clock::now();
  seconds timeout(3600);
  size_t j = 0;
  long waitInterval = 250000;

  CONDITION_LOCKER(guard, _cv);
  
  while (!this->isStopping() && !_agent->isStopping()) {

    auto const config = _agent->config();  // get a copy of conf
    auto const version = config.version();

    // Build gossip message
    auto out = std::make_shared<Builder>();
    out->openObject();
    out->add("endpoint", VPackValue(config.endpoint()));
    out->add("id", VPackValue(config.id()));
    out->add("pool", VPackValue(VPackValueType::Object));
    for (auto const& i : config.pool()) {
      out->add(i.first, VPackValue(i.second));
    }
    out->close();
    out->close();

    auto const path = privApiPrefix + "gossip";

    // gossip peers
    for (auto const& p : config.gossipPeers()) {
      if (p != config.endpoint()) {
        {
          MUTEX_LOCKER(ackedLocker,_vLock);
          if (_acked[p] >= version) {
            continue;
          }
        }
        std::string clientid = config.id() + std::to_string(j++);
        auto hf =
          std::make_unique<std::unordered_map<std::string, std::string>>();
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Sending gossip message: "
            << out->toJson() << " to peer " << clientid;
        if (this->isStopping() || _agent->isStopping() || cc == nullptr) {
          return;
        }
        cc->asyncRequest(
          clientid, 1, p, rest::RequestType::POST, path,
          std::make_shared<std::string>(out->toJson()), hf,
          std::make_shared<GossipCallback>(_agent, version), 1.0, true, 0.5);
      }
    }
    
    // pool entries
    bool complete = true;
    for (auto const& pair : config.pool()) {
      if (pair.second != config.endpoint()) {
        {
          MUTEX_LOCKER(ackedLocker,_vLock);
          if (_acked[pair.second] >= version) {
            continue;
          }
        }
        complete = false;
        auto const clientid = config.id() + std::to_string(j++);
        auto hf =
          std::make_unique<std::unordered_map<std::string, std::string>>();
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Sending gossip message: "
            << out->toJson() << " to pool member " << clientid;
        if (this->isStopping() || _agent->isStopping() || cc == nullptr) {
          return;
        }
        cc->asyncRequest(
          clientid, 1, pair.second, rest::RequestType::POST, path,
          std::make_shared<std::string>(out->toJson()), hf,
          std::make_shared<GossipCallback>(_agent, version), 1.0, true, 0.5);
      }
    }

    // We're done
    if (config.poolComplete()) {
      if (complete) {
        LOG_TOPIC(INFO, Logger::AGENCY) << "Agent pool completed. Stopping "
          "active gossipping. Starting RAFT process.";
        _agent->startConstituent();
        break;
      }
    }

    // Timed out? :(
    if ((system_clock::now() - startTime) > timeout) {
      if (config.poolComplete()) {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Stopping active gossipping!";
      } else {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << "Failed to find complete pool of agents. Giving up!";
      }
      break;
    }

    // don't panic just yet
    _cv.wait(waitInterval);
    if (waitInterval < 2500000) { // 2.5s
      waitInterval *= 2;
    }

  }
  
}


bool Inception::restartingActiveAgent() {

  if (this->isStopping() || _agent->isStopping()) {
    return false;
  }
  
  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    // nullptr only happens during controlled shutdown
    return false;
  }

  LOG_TOPIC(INFO, Logger::AGENCY) << "Restarting agent from persistence ...";

  using namespace std::chrono;

  auto const  path      = pubApiPrefix + "config";
  auto const  myConfig  = _agent->config();
  auto const  startTime = system_clock::now();
  auto        active    = myConfig.active();
  auto const& clientId  = myConfig.id();
  auto const& clientEp  = myConfig.endpoint();
  auto const majority   = (myConfig.size()+1)/2;

  Builder greeting;
  {
    VPackObjectBuilder b(&greeting);
    greeting.add(clientId, VPackValue(clientEp));
  }
  auto const& greetstr = greeting.toJson();

  seconds const timeout(3600);
  long waitInterval(500000);  
  
  CONDITION_LOCKER(guard, _cv);

  active.erase(
    std::remove(active.begin(), active.end(), myConfig.id()), active.end());

  while (!this->isStopping() && !_agent->isStopping()) {
    
    active.erase(
      std::remove(active.begin(), active.end(), ""), active.end());
    
    if (active.size() < majority) {
      LOG_TOPIC(INFO, Logger::AGENCY)
        << "Found majority of agents in agreement over active pool. "
           "Finishing startup sequence.";
      return true;
    }

    auto gp = myConfig.gossipPeers();
    std::vector<std::string> informed;
    
    for (auto& p : gp) {
      if (this->isStopping() && _agent->isStopping() && cc==nullptr) {
        return false;
      }
      auto comres = cc->syncRequest(
        clientId, 1, p, rest::RequestType::POST, path, greetstr,
        std::unordered_map<std::string, std::string>(), 2.0);
      if (comres->status == CL_COMM_SENT) {
        auto const  theirConfigVP = comres->result->getBodyVelocyPack();
        auto const& theirConfig   = theirConfigVP->slice();
        auto const& tcc           = theirConfig.get("configuration");
        auto const& theirId       = tcc.get("id").copyString();
        
        _agent->updatePeerEndpoint(theirId, p);
        informed.push_back(p);
      }
    }
    
    auto pool = _agent->config().pool();    
    for (const auto& i : informed) {
      active.erase(
        std::remove(active.begin(), active.end(), i), active.end());
    }
    
    for (auto& p : pool) {
      
      if (p.first != myConfig.id() && p.first != "") {

        if (this->isStopping() || _agent->isStopping() || cc == nullptr) {
          return false;
        }

        auto comres = cc->syncRequest(
          clientId, 1, p.second, rest::RequestType::POST, path, greetstr,
          std::unordered_map<std::string, std::string>(), 2.0);
        
        if (comres->status == CL_COMM_SENT) {
          try {
            
            auto const  theirConfigVP = comres->result->getBodyVelocyPack();
            auto const& theirConfig   = theirConfigVP->slice();
            auto const& theirLeaderId = theirConfig.get("leaderId").copyString();
            auto const& tcc           = theirConfig.get("configuration");
            auto const& theirId       = tcc.get("id").copyString();            
            
            // Found RAFT with leader
            if (!theirLeaderId.empty()) {
              LOG_TOPIC(INFO, Logger::AGENCY) <<
                "Found active RAFTing agency lead by " << theirLeaderId <<
                ". Finishing startup sequence.";
              
              auto const theirLeaderEp =
                tcc.get(
                  std::vector<std::string>({"pool", theirLeaderId})).copyString();

              // Contact leader to update endpoint
              if (theirLeaderId != theirId) {
                if (this->isStopping() || _agent->isStopping() || cc==nullptr) {
                  return false;
                }
                comres = cc->syncRequest(
                  clientId, 1, theirLeaderEp, rest::RequestType::POST, path,
                  greetstr, std::unordered_map<std::string, std::string>(), 2.0);
                // Failed to contact leader move on until we do. This way at
                // least we inform everybody individually of the news.
                if (comres->status != CL_COMM_SENT) {
                  continue;
                }
              }
              
              auto agency = std::make_shared<Builder>();
              agency->openObject();
              agency->add("term", theirConfig.get("term"));
              agency->add("id", VPackValue(theirLeaderId));
              agency->add("active",      tcc.get("active"));
              agency->add("pool",        tcc.get("pool"));
              agency->add("min ping",    tcc.get("min ping"));
              agency->add("max ping",    tcc.get("max ping"));
              agency->add("timeoutMult", tcc.get("timeoutMult"));
              agency->close();
              _agent->notify(agency);
              return true;
            }
            
            auto const theirActive = tcc.get("active").toJson();
            auto const myActive = myConfig.activeToBuilder()->toJson();
            auto i = std::find(active.begin(),active.end(),p.first);
            
            if (i != active.end()) {
              if (theirActive != myActive) {
                if (!this->isStopping()) {
                  LOG_TOPIC(FATAL, Logger::AGENCY)
                    << "Assumed active RAFT peer and I disagree on active membership:";
                  LOG_TOPIC(FATAL, Logger::AGENCY)
                    << "Their active list is " << theirActive;  
                  LOG_TOPIC(FATAL, Logger::AGENCY)
                    << "My active list is " << myActive;  
                  FATAL_ERROR_EXIT();
                }
                return false;
              } else {
                *i = "";
              }
            }
            
          } catch (std::exception const& e) {
            if (!this->isStopping()) {
              LOG_TOPIC(FATAL, Logger::AGENCY)
                << "Assumed active RAFT peer has no active agency list: "
                << e.what() << ", administrative intervention needed.";
              FATAL_ERROR_EXIT();
            }
            return false;
          }
        } 
        
      }
      
    }
    
    // Timed out? :(
    if ((system_clock::now() - startTime) > timeout) {
      if (myConfig.poolComplete()) {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Joined complete pool!";
      } else {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << "Failed to find complete pool of agents. Giving up!";
      }
      break;
    }
    
    _cv.wait(waitInterval);
    if (waitInterval < 2500000) { // 2.5s
      waitInterval *= 2;
    }
    
  }

  return false;
  
}

void Inception::reportIn(query_t const& query) {
  // does nothing at the moment
}


void Inception::reportVersionForEp(std::string const& endpoint, size_t version) {
  MUTEX_LOCKER(versionLocker, _vLock);
  if (_acked[endpoint] < version) {
    _acked[endpoint] = version;
  }
}

// @brief Thread main
void Inception::run() {
  while (arangodb::rest::RestHandlerFactory::isMaintenance() &&
         !this->isStopping() && !_agent->isStopping()) {
    usleep(1000000);
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Waiting for RestHandlerFactory to exit maintenance mode before we "
         " start gossip protocol...";
  }

  config_t config = _agent->config();
  
  // Are we starting from persisted pool?
  if (config.startup() == "persistence") {
    if (restartingActiveAgent()) {
      LOG_TOPIC(INFO, Logger::AGENCY) << "Activating agent.";
      _agent->ready(true);
    } else {
      if (!this->isStopping()) {
        LOG_TOPIC(FATAL, Logger::AGENCY)
          << "Unable to restart with persisted pool. Fatal exit.";
        FATAL_ERROR_EXIT();
      }
    }
    return;
  }
  
  // Gossip
  gossip();

  // No complete pool after gossip?
  config = _agent->config();
  if (!_agent->ready() && !config.poolComplete()) {
    if (!this->isStopping()) {
      LOG_TOPIC(FATAL, Logger::AGENCY)
        << "Failed to build environment for RAFT algorithm. Bailing out!";
      FATAL_ERROR_EXIT();
    }
  }

  LOG_TOPIC(INFO, Logger::AGENCY) << "Activating agent.";
  _agent->ready(true);

}

// @brief Graceful shutdown
void Inception::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}
