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
#include "Agency/MeasureCallback.h"
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
Inception::~Inception() { shutdown(); }

/// Gossip to others
/// - Get snapshot of gossip peers and agent pool
/// - Create outgoing gossip.
/// - Send to all peers
void Inception::gossip() {

  LOG_TOPIC(INFO, Logger::AGENCY) << "Entering gossip phase ...";
  
  auto s = std::chrono::system_clock::now();
  std::chrono::seconds timeout(3600);
  size_t j = 0;
  bool complete = false;
  long waitInterval = 250000;

  CONDITION_LOCKER(guard, _cv);
  
  while (!this->isStopping() && !_agent->isStopping()) {

    config_t config = _agent->config();  // get a copy of conf
    size_t version = config.version();

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
        arangodb::ClusterComm::instance()->asyncRequest(
          clientid, 1, p, rest::RequestType::POST, path,
          std::make_shared<std::string>(out->toJson()), hf,
          std::make_shared<GossipCallback>(_agent, version), 1.0, true, 0.5);
      }
    }
    
    // pool entries
    for (auto const& pair : config.pool()) {
      if (pair.second != config.endpoint()) {
        {
          MUTEX_LOCKER(ackedLocker,_vLock);
          if (_acked[pair.second] >= version) {
            continue;
          }
        }
        std::string clientid = config.id() + std::to_string(j++);
        auto hf =
          std::make_unique<std::unordered_map<std::string, std::string>>();
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Sending gossip message: "
            << out->toJson() << " to pool member " << clientid;
        arangodb::ClusterComm::instance()->asyncRequest(
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
      complete = true;
    }

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

    // don't panic just yet
    _cv.wait(waitInterval);
    waitInterval *= 2;

  }
  
}


// @brief Active agency from persisted database
bool Inception::activeAgencyFromPersistence() {

  LOG_TOPIC(INFO, Logger::AGENCY) << "Found persisted agent pool ...";
  
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

    long waitInterval(500000);  

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
          } 
        }
        
      }
      
      // Timed out? :(
      if ((std::chrono::system_clock::now() - s) > timeout) {
        if (myConfig.poolComplete()) {
          LOG_TOPIC(DEBUG, Logger::AGENCY) << "Joined complete pool!";
        } else {
          LOG_TOPIC(ERR, Logger::AGENCY)
            << "Failed to find complete pool of agents. Giving up!";
        }
        break;
      }
      
      _cv.wait(waitInterval);
      waitInterval *= 2;

    }
  }

  return false;
  
}

inline static int64_t timeStamp() {
  using namespace std::chrono;
  return duration_cast<microseconds>(
    steady_clock::now().time_since_epoch()).count();
}

void Inception::reportIn(std::string const& peerId, uint64_t start) {
  MUTEX_LOCKER(lock, _pLock);
  _pings.push_back(1.0e-3*(double)(timeStamp()-start));
}

void Inception::reportIn(query_t const& query) {

  VPackSlice slice = query->slice();

  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.hasKey("mean"));
  TRI_ASSERT(slice.hasKey("stdev"));
  TRI_ASSERT(slice.hasKey("min"));
  TRI_ASSERT(slice.hasKey("max"));

  MUTEX_LOCKER(lock, _mLock);
  _measurements.push_back(
    std::vector<double>(
      {slice.get("mean").getDouble(), slice.get("stdev").getDouble(),
          slice.get("max").getDouble(), slice.get("min").getDouble()} ));

}

void Inception::reportVersionForEp(std::string const& endpoint, size_t version) {
  MUTEX_LOCKER(versionLocker, _vLock);
  if (_acked[endpoint] < version) {
    _acked[endpoint] = version;
  }
}


bool Inception::estimateRAFTInterval() {

  using namespace std::chrono;
  LOG_TOPIC(INFO, Logger::AGENCY) << "Estimating RAFT timeouts ...";
  size_t nrep = 25;
    
  std::string path("/_api/agency/config");
  auto config = _agent->config();

  auto myid = _agent->id();
  double to = 0.25;

  for (size_t i = 0; i < nrep; ++i) {
    for (auto const& peer : config.pool()) {
      if (peer.first != myid) {
        std::string clientid = peer.first + std::to_string(i);
        auto hf =
          std::make_unique<std::unordered_map<std::string, std::string>>();
        arangodb::ClusterComm::instance()->asyncRequest(
          clientid, 1, peer.second, rest::RequestType::GET, path,
          std::make_shared<std::string>(), hf,
          std::make_shared<MeasureCallback>(this, peer.second, timeStamp()),
          2.0, true);
      }
    }
    std::this_thread::sleep_for(std::chrono::duration<double,std::milli>(to));
    to *= 1.01;
  }

  auto s = system_clock::now();
  seconds timeout(15);

  CONDITION_LOCKER(guard, _cv);

  while (true) {
    
    _cv.wait(50000);
    
    {
      MUTEX_LOCKER(lock, _pLock);
      if (_pings.size() == nrep*(config.size()-1)) {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "All pings are in";
        break;
      }
    }
    
    if ((system_clock::now() - s) > timeout) {
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "Timed out waiting for pings";
      break;
    }
    
  }

  if (! _pings.empty()) {

    double mean, stdev, mx, mn;
  
    MUTEX_LOCKER(lock, _pLock);
    size_t num = _pings.size();
    mean   = std::accumulate(_pings.begin(), _pings.end(), 0.0) / num;
    mx     = *std::max_element(_pings.begin(), _pings.end());
    mn     = *std::min_element(_pings.begin(), _pings.end());
    std::transform(_pings.begin(), _pings.end(), _pings.begin(),
                   std::bind2nd(std::minus<double>(), mean));
    stdev =
      std::sqrt(
        std::inner_product(
          _pings.begin(), _pings.end(), _pings.begin(), 0.0) / num);
    
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "mean(" << mean << ") stdev(" << stdev<< ")";
      
    Builder measurement;
    measurement.openObject();
    measurement.add("mean", VPackValue(mean));
    measurement.add("stdev", VPackValue(stdev));
    measurement.add("min", VPackValue(mn));
    measurement.add("max", VPackValue(mx));
    measurement.close();
    std::string measjson = measurement.toJson();

    path = privApiPrefix + "measure";
    for (auto const& peer : config.pool()) {
      if (peer.first != myid) {
        auto clientId = "1";
        auto comres   = arangodb::ClusterComm::instance()->syncRequest(
          clientId, 1, peer.second, rest::RequestType::POST, path,
          measjson, std::unordered_map<std::string, std::string>(), 5.0);
      }
    }
    
    {
      MUTEX_LOCKER(lock, _mLock);
      _measurements.push_back(std::vector<double>({mean, stdev, mx, mn}));
    }
    s = system_clock::now();
    while (true) {
      
      _cv.wait(50000);
      
      {
        MUTEX_LOCKER(lock, _mLock);
        if (_measurements.size() == config.size()) {
          LOG_TOPIC(DEBUG, Logger::AGENCY) << "All measurements are in";
          break;
        }
      }
      
      if ((system_clock::now() - s) > timeout) {
        LOG_TOPIC(WARN, Logger::AGENCY) <<
          "Timed out waiting for other measurements. Auto-adaptation failed!"
          "Will stick to command line arguments";
        return false;
      }
      
    }

    if (_measurements.size() == config.size()) {
    
      double maxmean  = .0;
      double maxstdev = .0;
      for (auto const& meas : _measurements) {
        if (maxmean < meas[0]) {
          maxmean = meas[0];
        }
        if (maxstdev < meas[1]) {
          maxstdev = meas[1];
        }
      }

      double precision = 1.0e-2;
      mn = precision *
        std::ceil((1./precision)*(.25 + precision*(maxmean+3*maxstdev)));
      mx = 5. * mn;
      
      LOG_TOPIC(INFO, Logger::AGENCY)
        << "Auto-adapting RAFT bracket to: {"
        << std::fixed << std::setprecision(2) << mn << ", " << mx << "} seconds";
      
      _agent->resetRAFTTimes(mn, mx);

    } else {

      return false;

    }
    
  }

  return true;
  
}
  

// @brief Active agency from persisted database
bool Inception::activeAgencyFromCommandLine() {
  return false;
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
  // 1. If active agency, do as you're told
  if (config.startup() == "persistence" && activeAgencyFromPersistence()) {
    _agent->ready(true);  
  }
  
  // 2. If we think that we used to be active agent
  if (!_agent->ready() && restartingActiveAgent()) {
    _agent->ready(true);  
  }
  
  // 3. Else gossip
  config = _agent->config();
  if (!_agent->ready() && !config.poolComplete()) {
    gossip();
  }

  // 4. If still incomplete bail out :(
  config = _agent->config();
  if (!_agent->ready() && !config.poolComplete()) {
    LOG_TOPIC(FATAL, Logger::AGENCY)
      << "Failed to build environment for RAFT algorithm. Bailing out!";
    FATAL_ERROR_EXIT();
  }

  // 5. If command line RAFT timings have not been set explicitly
  //    Try good estimate of RAFT time limits
  if (!config.cmdLineTimings()) {
    estimateRAFTInterval();
  }

  _agent->ready(true);

}

// @brief Graceful shutdown
void Inception::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}
