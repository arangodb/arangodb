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
        arangodb::ClusterComm::instance()->asyncRequest(
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

  LOG_TOPIC(INFO, Logger::AGENCY) << "Restarting agent from persistence ...";

  using namespace std::chrono;

  auto const  path = pubApiPrefix + "config";
  auto const  myConfig   = _agent->config();
  auto const  startTime  = system_clock::now();
  auto        pool       = myConfig.pool();
  auto        active     = myConfig.active();
  auto const& clientId   = myConfig.id();
  auto const majority   = (myConfig.size()+1)/2;

  seconds const timeout(3600);
  
  CONDITION_LOCKER(guard, _cv);
  
  long waitInterval(500000);  
  
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
    
    for (auto& p : pool) {
      
      if (p.first != myConfig.id() && p.first != "") {
        
        auto comres = arangodb::ClusterComm::instance()->syncRequest(
          clientId, 1, p.second, rest::RequestType::GET, path, std::string(),
          std::unordered_map<std::string, std::string>(), 2.0);
        
        if (comres->status == CL_COMM_SENT) {
          try {
            
            auto const  theirConfigVP = comres->result->getBodyVelocyPack();
            auto const& theirConfig   = theirConfigVP->slice();
            auto const& theirLeaderId = theirConfig.get("leaderId").copyString();
            auto const& tcc           = theirConfig.get("configuration");
            
            // Known leader. We are done.
            if (!theirLeaderId.empty()) {
              LOG_TOPIC(INFO, Logger::AGENCY) <<
                "Found active RAFTing agency lead by " << theirLeaderId <<
                ". Finishing startup sequence.";
              auto agency = std::make_shared<Builder>();
              agency->openObject();
              agency->add("term", theirConfig.get("term"));
              agency->add("id", VPackValue(theirLeaderId));
              agency->add("active",   tcc.get("active"));
              agency->add("pool",     tcc.get("pool"));
              agency->add("min ping", tcc.get("min ping"));
              agency->add("max ping", tcc.get("max ping"));
              agency->close();
              _agent->notify(agency);
              return true;
            }
            
            auto const theirActive = tcc.get("active").toJson();
            auto const myActive = myConfig.activeToBuilder()->toJson();
            auto i = std::find(active.begin(),active.end(),p.first);

            if (i != active.end()) {
              if (theirActive != myActive) {
                LOG_TOPIC(FATAL, Logger::AGENCY)
                  << "Assumed active RAFT peer and I disagree on active membership:";
                LOG_TOPIC(FATAL, Logger::AGENCY)
                  << "Their active list is " << theirActive;  
                LOG_TOPIC(FATAL, Logger::AGENCY)
                  << "My active list is " << myActive;  
                FATAL_ERROR_EXIT();
                return false;
              } else {
                *i = "";
              }
            }
            
          } catch (std::exception const& e) {
            LOG_TOPIC(FATAL, Logger::AGENCY)
              << "Assumed active RAFT peer has no active agency list: "
              << e.what() << "Administrative intervention needed.";
            FATAL_ERROR_EXIT();
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
  size_t nrep = 10;
    
  std::string path("/_api/agency/config");
  auto config = _agent->config();

  auto myid = _agent->id();
  auto to = duration<double,std::milli>(10.0); // 

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
    std::this_thread::sleep_for(to);
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
        std::ceil((1. / precision)*(.3 + precision * (maxmean + 3.*maxstdev)));
      if (config.waitForSync()) {
        mn *= 4.;
      }
      if (mn > 2.0) {
        mn = 2.0;
      }
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
        LOG_TOPIC(FATAL, Logger::AGENCY)
          << "Unable to restart with persisted pool. Fatal exit.";
        FATAL_ERROR_EXIT();
      // FATAL ERROR
    }
    return;
  }
  
  // Gossip
  gossip();

  // No complete pool after gossip?
  config = _agent->config();
  if (!_agent->ready() && !config.poolComplete()) {
    LOG_TOPIC(FATAL, Logger::AGENCY)
      << "Failed to build environment for RAFT algorithm. Bailing out!";
    FATAL_ERROR_EXIT();
  }

  // If command line RAFT timings have not been set explicitly
  // Try good estimate of RAFT time limits
  if (!config.cmdLineTimings()) {
    estimateRAFTInterval();
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
