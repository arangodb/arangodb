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

#ifndef ARANGOD_CONSENSUS_AGENT_CONFIGURATION_H
#define ARANGOD_CONSENSUS_AGENT_CONFIGURATION_H 1

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {

  static const std::string idStr = "id";
  static const std::string agencySizeStr = "agency size";
  static const std::string poolSizeStr = "pool size";
  static const std::string minPingStr = "min ping";
  static const std::string maxPingStr = "max ping";
  static const std::string endpointStr = "endpoint";
  static const std::string uuidStr = "uuid";
  static const std::string poolStr = "pool";
  static const std::string gossipPeersStr = "gissipPeers";
  static const std::string activeStr = "active";
  static const std::string supervisionStr = "supervision";
  static const std::string waitForSyncStr = "wait for sync";
  static const std::string supervisionFrequencyStr = "supervision frequency";
  static const std::string compactionStepSizeStr = "compaction step size";
  static const std::string defaultEndpointStr = "tcp://localhost:8529";

struct config_t {

  arangodb::consensus::id_t id;
  size_t agencySize;
  size_t poolSize;
  double minPing;
  double maxPing;
  std::string endpoint;
  std::string uuid;
  std::map<std::string, std::string> pool;
  std::vector<std::string> gossipPeers;
  std::vector<std::string> active;
  bool supervision;
  bool waitForSync;
  double supervisionFrequency;
  uint64_t compactionStepSize;

  config_t()
    : agencySize(0),
      poolSize(0),
      minPing(0.0),
      maxPing(0.0),
      endpoint(defaultEndpointStr),
      supervision(false),
      waitForSync(true),
      supervisionFrequency(5.0),
      compactionStepSize(1000) {}


  /// @brief ctor
  config_t(size_t as, size_t ps, double minp, double maxp, std::string const& e,
           std::vector<std::string> const& g, bool s, bool w, double f,
           uint64_t c)
    : agencySize(as),
      poolSize(ps),
      minPing(minp),
      maxPing(maxp),
      endpoint(e),
      gossipPeers(g),
      supervision(s),
      waitForSync(w),
      supervisionFrequency(f),
      compactionStepSize(c) {}


  /// @brief active agency size
  inline size_t size() const { return agencySize; }

  
  /// @brief pool size
  inline size_t pSize() const { return poolSize; }


  query_t const activeToBuilder () {
    query_t ret = std::make_shared<arangodb::velocypack::Builder>();
    ret->openArray();
    for (auto const& i : active) {
      ret->add(VPackValue(i));
    }
    ret->close();
    return ret;
  }

  
  /// @brief override this configuration with prevailing opinion (startup)
  void override(VPackSlice const& conf) {

    if (conf.hasKey(agencySizeStr) && conf.get(agencySizeStr).isUInt()) {
      agencySize = conf.get(agencySizeStr).getUInt();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << agencySizeStr << " from " << conf.toJson();
    }
    
    if (conf.hasKey(poolSizeStr) && conf.get(poolSizeStr).isUInt()) {
      poolSize = conf.get(poolSizeStr).getUInt();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << poolSizeStr << " from " << conf.toJson();
    }
    
    if (conf.hasKey(minPingStr) && conf.get(minPingStr).isDouble()) {
      minPing = conf.get(minPingStr).getDouble();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << minPingStr << " from " << conf.toJson();
    }

    if (conf.hasKey(maxPingStr) && conf.get(maxPingStr).isDouble()) {
      maxPing = conf.get(maxPingStr).getDouble();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << maxPingStr << " from " << conf.toJson();
    }

    if (conf.hasKey(poolStr) && conf.get(poolStr).isArray()) {
      pool.clear();
      for (auto const& peer : VPackArrayIterator(conf.get(poolStr))) {
        auto key = peer.get(idStr).copyString();
        auto value = peer.get(endpointStr).copyString(); 
        pool[key] = value;
      }
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << poolStr << " from " << conf.toJson();
    }

    if (conf.hasKey(activeStr) && conf.get(activeStr).isArray()) {
      active.clear();
      for (auto const& peer : VPackArrayIterator(conf.get(activeStr))) {
        active.push_back(peer.copyString());
      }
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override poolSize from " << conf.toJson();
    }

    if (conf.hasKey(supervisionStr) && conf.get(supervisionStr).isBoolean()) {
      supervision = conf.get(supervisionStr).getBoolean();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << supervisionStr << " from " << conf.toJson();
    }

    if (conf.hasKey(waitForSyncStr) && conf.get(waitForSyncStr).isBoolean()) {
      waitForSync = conf.get(waitForSyncStr).getBoolean();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << waitForSyncStr << " from " << conf.toJson();
    }

    if (conf.hasKey(supervisionFrequencyStr) &&
        conf.get(supervisionFrequencyStr).isDouble()) {
      supervisionFrequency = conf.get(supervisionFrequencyStr).getDouble();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << supervisionFrequencyStr << " from "
        << conf.toJson();
    }

    if (conf.hasKey(compactionStepSizeStr) &&
        conf.get(compactionStepSizeStr).isUInt()) {
      compactionStepSize = conf.get(compactionStepSizeStr).getUInt();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to override " << compactionStepSizeStr << " from "
        << conf.toJson();
    }

  }

  
  /// @brief vpack representation
  query_t const toBuilder() const {
    query_t ret = std::make_shared<arangodb::velocypack::Builder>();
    ret->openObject();
    ret->add(poolStr, VPackValue(VPackValueType::Object));
    for (auto const& i : pool) {
      ret->add(i.first, VPackValue(i.second));
    }
    ret->close();
    ret->add(activeStr, VPackValue(VPackValueType::Array));
    for (auto const& i : active) {
      ret->add(VPackValue(i));
    }
    ret->close();
    ret->add(idStr, VPackValue(id));
    ret->add(agencySizeStr, VPackValue(agencySize));
    ret->add(poolSizeStr, VPackValue(poolSize));
    ret->add(endpointStr, VPackValue(endpoint));
    ret->add(minPingStr, VPackValue(minPing));
    ret->add(maxPingStr, VPackValue(maxPing));
    ret->add(supervisionStr, VPackValue(supervision));
    ret->add(supervisionFrequencyStr, VPackValue(supervisionFrequency));
    ret->add(compactionStepSizeStr, VPackValue(compactionStepSize));
    ret->close();
    return ret;
  }

  bool setId(arangodb::consensus::id_t const& i) {
    if (id.empty()) {
      id = i;
      pool[id] = endpoint; // Register my endpoint with it
      return true;
    } else {
      return false;
    }
  }


  /// @brief merge from persisted configuration
  bool merge(VPackSlice const& conf) { 

    LOG(WARN) << conf.typeName();
    
    id = conf.get(idStr).copyString(); // I get my id
    pool[id] = endpoint; // Register my endpoint with it

    LOG(WARN) << __FILE__ << ":" << __LINE__;
    std::stringstream ss;
    ss << "Agency size: ";
    if (conf.hasKey(agencySizeStr)) { // Persistence beats command line
      agencySize = conf.get(agencySizeStr).getUInt();
      ss << agencySize << " (persisted)";
    } else {
      if (agencySize == 0) {
        agencySize = 1;
        ss << agencySize << " (default)";
      } else {
        ss << agencySize << " (command line)";
      }
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    LOG(WARN) << __FILE__ << ":" << __LINE__;
    ss.str(""); ss.clear();
    ss << "Agency pool size: ";
    if (poolSize == 0) { // Command line beats persistence
      if (conf.hasKey(poolSizeStr)) {
        poolSize = conf.get(poolSizeStr).getUInt();
        ss << poolSize << " (persisted)";
      } else {
        poolSize = agencySize;
        ss << poolSize << " (default)";
      }
    } else {
      ss << poolSize << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    LOG(WARN) << __FILE__ << ":" << __LINE__;
    ss.str(""); ss.clear();
    ss << "Agent pool: ";
    if (conf.hasKey(poolStr)) { // Persistence only
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "Found agent pool in persistence:";
      for (auto const& peer : VPackObjectIterator(conf.get(poolStr))) {
        pool[peer.key.copyString()] = peer.value.copyString();
      }
      ss << conf.get(poolStr).toJson() << " (persisted)";
    } else {
      ss << "empty (default)";
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    LOG(WARN) << __FILE__ << ":" << __LINE__;
    ss.str(""); ss.clear();
    ss << "Active agents: ";
    if (conf.hasKey(activeStr)) { // Persistence only?
      for (auto const& a : VPackArrayIterator(conf.get(activeStr))) {
        active.push_back(a.copyString());
        ss << a.copyString() << " ";
      }
      ss << conf.get(activeStr).toJson();
    } else {
      ss << "empty (default)";
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    LOG(WARN) << __FILE__ << ":" << __LINE__;
    ss.str(""); ss.clear();
    ss << "Min RAFT interval: ";
    if (minPing == 0) { // Command line beats persistence
      if (conf.hasKey(minPingStr)) {
        minPing = conf.get(minPingStr).getUInt();
        ss << minPing << " (persisted)";
      } else {
        minPing = 0.5;
        ss << minPing << " (default)";
      }
    } else {
      ss << minPing << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    LOG(WARN) << __FILE__ << ":" << __LINE__;
    ss.str(""); ss.clear();
    ss << "Max RAFT interval: ";
    if (maxPing == 0) { // Command line beats persistence
      if (conf.hasKey(maxPingStr)) {
        maxPing = conf.get(maxPingStr).getUInt();
        ss << maxPing << " (persisted)";
      } else {
        maxPing = 2.5;
        ss << maxPing << " (default)";
      }
    } else {
      ss << maxPing << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    LOG(WARN) << __FILE__ << ":" << __LINE__;
    ss.str(""); ss.clear();
    ss << "Supervision: ";
    if (supervision == false) { // Command line beats persistence
      if (conf.hasKey(supervisionStr)) {
        supervision = conf.get(supervisionStr).getBoolean();
        ss << supervision << " (persisted)";
      } else {
        supervision = true;
        ss << supervision << " (default)";
      }
    } else {
      ss << supervision << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    ss.str(""); ss.clear();
    ss << "Supervision interval [s]: ";
    if (supervisionFrequency == 0) { // Command line beats persistence
      if (conf.hasKey(supervisionFrequencyStr)) {
        supervisionFrequency = conf.get(supervisionFrequencyStr).getDouble();
        ss << supervisionFrequency << " (persisted)";
      } else {
        supervisionFrequency = 2.5;
        ss << supervisionFrequency << " (default)";
      }
    } else {
      ss << supervisionFrequency << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    LOG(WARN) << __FILE__ << ":" << __LINE__;
    ss.str(""); ss.clear();
    ss << "Compaction step size: ";
    if (compactionStepSize == 0) { // Command line beats persistence
      if (conf.hasKey(compactionStepSizeStr)) {
        compactionStepSize = conf.get(compactionStepSizeStr).getUInt();
        ss << compactionStepSize << " (persisted)";
      } else {
        compactionStepSize = 2.5;
        ss << compactionStepSize << " (default)";
      }
    } else {
      ss << compactionStepSize << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

    return true;
    
  }
  
};
}
}

#endif
