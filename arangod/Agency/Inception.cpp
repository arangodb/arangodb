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

#include "Inception.h"

#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/application-exit.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"

#include <chrono>
#include <thread>

using namespace arangodb::consensus;

namespace {
void handleGossipResponse(arangodb::network::Response const& r,
                          arangodb::consensus::Agent* agent,
                          size_t version) {
  using namespace arangodb;
  std::string newLocation;

  if (r.ok()) {
    velocypack::Slice payload = r.response->slice();

    switch (r.response->statusCode()) {
      case 200:  // Digest other configuration
        LOG_TOPIC("4995a", DEBUG, Logger::AGENCY)
            << "Got result of gossip message, code: 200"
            << " body: " << payload.toJson();
        agent->gossip(payload, true, version);
        break;

      case 307:  // Add new endpoint to gossip peers
        bool found;
        newLocation = r.response->header.metaByKey("location", found);

        if (found) {
          if (newLocation.compare(0, 5, "https") == 0) {
            newLocation = newLocation.replace(0, 5, "ssl");
          } else if (newLocation.compare(0, 4, "http") == 0) {
            newLocation = newLocation.replace(0, 4, "tcp");
          } else {
            LOG_TOPIC("60be0", FATAL, Logger::AGENCY)
                << "Invalid URL specified as gossip endpoint";
            FATAL_ERROR_EXIT();
          }

          LOG_TOPIC("4c822", DEBUG, Logger::AGENCY) << "Got redirect to " << newLocation
                                           << ". Adding peer to gossip peers";
          bool added = agent->addGossipPeer(newLocation);
          if (added) {
            LOG_TOPIC("d41c8", DEBUG, Logger::AGENCY) << "Added " << newLocation << " to gossip peers";
          } else {
            LOG_TOPIC("4fcf3", DEBUG, Logger::AGENCY) << "Endpoint " << newLocation << " already known";
          }
        } else {
          LOG_TOPIC("1886b", ERR, Logger::AGENCY) << "Redirect lacks 'Location' header";
        }
        break;

      default:
        LOG_TOPIC("bed89", ERR, Logger::AGENCY) << "Got error " << r.response->statusCode()
        << " from gossip endpoint";
        std::this_thread::sleep_for(std::chrono::seconds(40));
        break;
    }
  }

  LOG_TOPIC("e2ef9", DEBUG, Logger::AGENCY)
      << "Got error from gossip message, status:" << fuerte::to_string(r.error);
}
}

Inception::Inception(Agent& agent)
    : Thread(agent.server(), "Inception"), _agent(agent) {}

// Shutdown if not already
Inception::~Inception() {
  shutdown();
}

/// Gossip to others
/// - Get snapshot of gossip peers and agent pool
/// - Create outgoing gossip.
/// - Send to all peers
void Inception::gossip() {
  if (this->isStopping() || _agent.isStopping()) {
    return;
  }
  
  
  auto const& nf = _agent.server().getFeature<arangodb::NetworkFeature>();
  network::ConnectionPool* cp = nf.pool();


  LOG_TOPIC("7b6f3", INFO, Logger::AGENCY) << "Entering gossip phase ...";
  using namespace std::chrono;

  auto startTime = steady_clock::now();
  seconds timeout(3600);
  long waitInterval = 250000;
  
  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(1);

  CONDITION_LOCKER(guard, _cv);

  while (!this->isStopping() && !_agent.isStopping()) {
    auto const config = _agent.config();  // get a copy of conf
    auto const version = config.version();

    // Build gossip message
    VPackBuffer<uint8_t> buffer;
    VPackBuilder out(buffer);
    out.openObject();
    out.add("endpoint", VPackValue(config.endpoint()));
    out.add("id", VPackValue(config.id()));
    out.add("pool", VPackValue(VPackValueType::Object));
    for (auto const& i : config.pool()) {
      out.add(i.first, VPackValue(i.second));
    }
    out.close();
    out.close();

    auto const path = privApiPrefix + "gossip";

    // gossip peers
    for (auto const& p : config.gossipPeers()) {
      if (p != config.endpoint()) {
        {
          MUTEX_LOCKER(ackedLocker, _vLock);
          auto const& ackedPeer = _acked.find(p);
          if (ackedPeer != _acked.end() && ackedPeer->second >= version) {
            continue;
          }
        }
        LOG_TOPIC("cc3fd", DEBUG, Logger::AGENCY)
            << "Sending gossip message 1: " << out.toJson() << " to peer " << p;
        if (this->isStopping() || _agent.isStopping()) {
          return;
        }

        network::sendRequest(cp, p, fuerte::RestVerb::Post, path,
                             buffer, reqOpts).thenValue([=](network::Response r) {
          ::handleGossipResponse(r, &_agent, version);
        });
      }
    }

    if (config.poolComplete()) {
      _agent.activateAgency();
      return;
    }
    
    // pool entries
    bool complete = true;
    for (auto const& pair : config.pool()) {
      if (pair.second != config.endpoint()) {
        {
          MUTEX_LOCKER(ackedLocker, _vLock);
          if (_acked[pair.second] > version) {
            continue;
          }
        }
        complete = false;
        LOG_TOPIC("07338", DEBUG, Logger::AGENCY)
            << "Sending gossip message 2: " << out.toJson()
            << " to pool member " << pair.second;
        if (this->isStopping() || _agent.isStopping()) {
          return;
        }

        network::sendRequest(cp, pair.second, fuerte::RestVerb::Post, path,
                             buffer, reqOpts).thenValue([=](network::Response r) {
          ::handleGossipResponse(r, &_agent, version);
        });
      }
    }

    // We're done
    if (config.poolComplete()) {
      if (complete) {
        LOG_TOPIC("d04d7", INFO, Logger::AGENCY)
            << "Agent pool completed. Stopping "
               "active gossipping. Starting RAFT process.";
        _agent.activateAgency();
        break;
      }
    }

    // Timed out? :(
    if ((steady_clock::now() - startTime) > timeout) {
      if (config.poolComplete()) {
        LOG_TOPIC("28033", DEBUG, Logger::AGENCY) << "Stopping active gossipping!";
      } else {
        LOG_TOPIC("5d169", ERR, Logger::AGENCY)
            << "Failed to find complete pool of agents. Giving up!";
      }
      break;
    }

    // don't panic just yet
    //  wait() is true on signal, false on timeout
    if (_cv.wait(waitInterval)) {
      waitInterval = 250000;
    } else {
      if (waitInterval < 2500000) {  // 2.5s
        waitInterval *= 2;
      }
    }
  }
}

bool Inception::restartingActiveAgent() {
  if (this->isStopping() || _agent.isStopping()) {
    return false;
  }

  LOG_TOPIC("d7476", INFO, Logger::AGENCY) << "Restarting agent from persistence ...";

  using namespace std::chrono;

  auto const path = pubApiPrefix + "config";
  auto const myConfig = _agent.config();
  auto const startTime = steady_clock::now();
  auto active = myConfig.active();
  auto const& clientId = myConfig.id();
  auto const& clientEp = myConfig.endpoint();
  auto const majority = myConfig.size() / 2 + 1;

  VPackBuffer<uint8_t> greetBuffer;
  {
    Builder greeting(greetBuffer);
    VPackObjectBuilder b(&greeting);
    greeting.add(clientId, VPackValue(clientEp));
  }
  
  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(2);
  reqOpts.skipScheduler = true; // hack to speed up future.get()

  seconds const timeout(3600);
  long waitInterval(500000);
  
  auto const& nf = _agent.server().getFeature<arangodb::NetworkFeature>();
  network::ConnectionPool* cp = nf.pool();

  CONDITION_LOCKER(guard, _cv);

  active.erase(std::remove(active.begin(), active.end(), myConfig.id()), active.end());

  while (!this->isStopping() && !_agent.isStopping()) {
    active.erase(std::remove(active.begin(), active.end(), ""), active.end());

    if (active.size() < majority) {
      LOG_TOPIC("9530f", INFO, Logger::AGENCY)
          << "Found majority of agents in agreement over active pool. "
             "Finishing startup sequence.";
      return true;
    }

    auto gp = myConfig.gossipPeers();
    std::vector<std::string> informed;

    for (std::string const& p : gp) {
      if (this->isStopping() || _agent.isStopping()) {
        return false;
      }
      
      auto comres = network::sendRequest(cp, p, fuerte::RestVerb::Post, path,
                                         greetBuffer, reqOpts).get();
      
      if (comres.ok() && comres.response->statusCode() == fuerte::StatusOK) {
        
        VPackSlice const theirConfig = comres.slice();

        if (!theirConfig.isObject()) {
          continue;
        }
        auto const& tcc = theirConfig.get("configuration");

        if (!tcc.isObject() || !tcc.hasKey("id")) {
          continue;
        }
        auto const& theirId = tcc.get("id").copyString();

        _agent.updatePeerEndpoint(theirId, p);
        informed.push_back(p);
      }
    }

    // ServerId to endpoint map
    std::unordered_map<std::string, std::string> pool = _agent.config().pool();
    for (auto const& i : informed) {
      active.erase(std::remove(active.begin(), active.end(), i), active.end());
    }
    
    for (auto& p : pool) {
      if (p.first != myConfig.id() && p.first != "") {
        if (this->isStopping() || _agent.isStopping()) {
          return false;
        }
        
        auto comres = network::sendRequest(cp, p.second, fuerte::RestVerb::Post, path,
                                           greetBuffer, reqOpts).get();
        
        if (comres.ok()) {
          try {
            VPackSlice theirConfig = comres.slice();
          
            auto const& theirLeaderId = theirConfig.get("leaderId").copyString();
            auto const& tcc = theirConfig.get("configuration");
            auto const& theirId = tcc.get("id").copyString();

            // Found RAFT with leader
            if (!theirLeaderId.empty()) {
              LOG_TOPIC("d96f6", INFO, Logger::AGENCY)
                  << "Found active RAFTing agency lead by " << theirLeaderId
                  << ". Finishing startup sequence.";

              auto const theirLeaderEp =
                  tcc.get(std::vector<std::string>({"pool", theirLeaderId})).copyString();

              if (theirLeaderId == myConfig.id()) {
                continue;
              }

              // Contact leader to update endpoint
              if (theirLeaderId != theirId) {
                if (this->isStopping() || _agent.isStopping()) {
                  return false;
                }
                
                comres = network::sendRequest(cp, theirLeaderEp, fuerte::RestVerb::Post, path,
                                              greetBuffer, reqOpts).get();
                
                // Failed to contact leader move on until we do. This way at
                // least we inform everybody individually of the news.
                if (comres.fail()) {
                  continue;
                }
                theirConfig = comres.slice();
              }
              auto const& lcc = theirConfig.get("configuration");
              auto agency = std::make_shared<Builder>();
              {
                VPackObjectBuilder b(agency.get());
                agency->add("term", theirConfig.get("term"));
                agency->add("id", VPackValue(theirLeaderId));
                agency->add("active", lcc.get("active"));
                agency->add("pool", lcc.get("pool"));
                agency->add("min ping", lcc.get("min ping"));
                agency->add("max ping", lcc.get("max ping"));
                agency->add("timeoutMult", lcc.get("timeoutMult"));
              }
              _agent.notify(agency);
              return true;
            }

            auto const theirActive = tcc.get("active");
            auto const myActiveB = myConfig.activeToBuilder();
            auto const myActive = myActiveB->slice();
            auto i = std::find(active.begin(), active.end(), p.first);

            if (i != active.end()) {  // Member in my active list
              TRI_ASSERT(theirActive.isArray());
              if (theirActive.length() == 0 || theirActive.length() == myActive.length()) {
                std::vector<std::string> theirActVec, myActVec;
                for (auto const i : VPackArrayIterator(theirActive)) {
                  theirActVec.push_back(i.copyString());
                }
                for (auto const i : VPackArrayIterator(myActive)) {
                  myActVec.push_back(i.copyString());
                }
                std::sort(myActVec.begin(), myActVec.end());
                std::sort(theirActVec.begin(), theirActVec.end());
                if (!theirActVec.empty() && theirActVec != myActVec) {
                  if (!this->isStopping()) {
                    LOG_TOPIC("93486", FATAL, Logger::AGENCY)
                        << "Assumed active RAFT peer and I disagree on active "
                           "membership:";
                    LOG_TOPIC("8c9e7", FATAL, Logger::AGENCY)
                        << "Their active list is " << theirActive.toJson();
                    LOG_TOPIC("b7ea1", FATAL, Logger::AGENCY)
                        << "My active list is " << myActive.toJson();
                    FATAL_ERROR_EXIT();
                  }
                  return false;
                } else {
                  *i = "";
                }
              } else {
                LOG_TOPIC("13d31", FATAL, Logger::AGENCY)
                    << "Assumed active RAFT peer and I disagree on active "
                       "agency size:";
                LOG_TOPIC("5d11c", FATAL, Logger::AGENCY)
                    << "Their active list is " << theirActive.toJson();
                LOG_TOPIC("568e6", FATAL, Logger::AGENCY)
                    << "My active list is " << myActive.toJson();
                FATAL_ERROR_EXIT();
              }
            }
          } catch (std::exception const& e) {
            if (!this->isStopping()) {
              LOG_TOPIC("e971a", FATAL, Logger::AGENCY)
                  << "Assumed active RAFT peer has no active agency list: " << e.what()
                  << ", administrative intervention needed.";
              FATAL_ERROR_EXIT();
            }
            return false;
          }
        }
      }
    }

    // Timed out? :(
    if ((steady_clock::now() - startTime) > timeout) {
      if (myConfig.poolComplete()) {
        LOG_TOPIC("26f6d", DEBUG, Logger::AGENCY) << "Joined complete pool!";
      } else {
        LOG_TOPIC("be8b4", ERR, Logger::AGENCY)
            << "Failed to find complete pool of agents. Giving up!";
      }
      break;
    }

    _cv.wait(waitInterval);
    if (waitInterval < 2500000) {  // 2.5s
      waitInterval *= 2;
    }
  }

  return false;
}

void Inception::reportVersionForEp(std::string const& endpoint, size_t version) {
  MUTEX_LOCKER(versionLocker, _vLock);
  if (_acked[endpoint] < version) {
    _acked[endpoint] = version;
  }
}

// @brief Thread main
void Inception::run() {
  auto server = ServerState::instance();
  while (server->isMaintenance() && !this->isStopping() && !_agent.isStopping()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    LOG_TOPIC("1b613", DEBUG, Logger::AGENCY)
        << "Waiting for RestHandlerFactory to exit maintenance mode before we "
           " start gossip protocol...";
  }

  config_t config = _agent.config();

  // Are we starting from persisted pool?
  if (config.startup() == "persistence") {
    if (restartingActiveAgent()) {
      LOG_TOPIC("79fd7", INFO, Logger::AGENCY) << "Activating agent.";
      _agent.ready(true);
    } else {
      if (!this->isStopping()) {
        LOG_TOPIC("27391", FATAL, Logger::AGENCY)
            << "Unable to restart with persisted pool. Fatal exit.";
        FATAL_ERROR_EXIT();
      }
    }
    return;
  }

  // Gossip
  gossip();

  // No complete pool after gossip?
  config = _agent.config();
  if (!_agent.ready() && !config.poolComplete()) {
    if (!this->isStopping()) {
      LOG_TOPIC("68763", FATAL, Logger::AGENCY)
          << "Failed to build environment for RAFT algorithm. Bailing out!";
      FATAL_ERROR_EXIT();
    }
  }

  LOG_TOPIC("c1d8f", INFO, Logger::AGENCY) << "Activating agent.";
  _agent.ready(true);
}

// @brief Graceful shutdown
void Inception::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}

// @brief Let external routines, like Agent::gossip(), signal our condition
void Inception::signalConditionVar() {
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}
