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

namespace arangodb {
namespace consensus {

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
      endpoint("tcp://localhost:8529"),
      supervision(false),
      waitForSync(true),
      supervisionFrequency(5.0),
      compactionStepSize(1000) {}
  
  config_t(size_t as, size_t ps, double minp, double maxp, std::string const& e,
           std::vector<std::string> const& g, bool s, bool w, double f,
           uint64_t c)
    : agencySize(as),
      poolSize(ps),
      minPing(minp),
      maxPing(maxp),
      endpoint(e),
      gossipPeers(g)
      supervision(s),
      waitForSync(w),
      supervisionFrequency(f),
      compactionStepSize(c) {}

  inline size_t size() const { return size; }

  query_t const toBuilder() const {
    query_t ret = std::make_shared<arangodb::velocypack::Builder>();
    ret->openObject();
    ret->add("pool", VPackValue(VPackValueType::Object));
    for (auto const& i : pool) {
      ret->add(i.first, VPackValue(i.second));
    }
    ret->close();
    ret->add("agency size": VPackValue(agencySize));
    ret->add("pool size": VPackValue(poolSize));
    ret->add("endpoint", VPackValue(endpoint));
    ret->add("id", VPackValue(id));
    ret->add("min ping", VPackValue(minPing));
    ret->add("max ping", VPackValue(maxPing));
    ret->add("supervision", VPackValue(supervision));
    ret->add("supervision frequency", VPackValue(supervisionFrequency));
    ret->add("compaction step size", VPackValue(compactionStepSize));
    ret->close();
    return ret;
  }

  void merge (VPackSlice const& conf) { 
    
    id = conf.get("id").copyString(); // I get my id
    pool[id] = endpoint; // Register my endpoint with it

    std::stringstream ss;
    ss << "Agency size: ";
    if (conf.hasKey("agency size")) { // Persistence beats command line
      agencySize = conf.get("agency size").getUInt();
      ss << agencySize << " (persisted)";
    } else {
      if (agencySize == 0) {
        agencySize = 1;
        ss << agencySize << " (default)";
      } else {
        ss << agencySize << " (command line)";
      }
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

    ss.clear();
    ss << "Agency pool size: ";
    if (poolSize == 0) { // Command line beats persistence
      if (conf.hasKey("pool size")) {
        poolSize = conf.get("pool size").getUInt();
        ss << poolSize << " (persisted)";
      } else {
        poolSize = agencySize;
        ss << poolSize << " (default)";
      }
    } else {
      ss << poolSize << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

    ss.clear();
    ss << "Agent pool: ";
    if (conf.hasKey("pool")) { // Persistence only
      LOG_TOPIC(DEBUG, Agency::Logger) << "Found agent pool in persistence:";
      for (auto const& peer : VPackArrayIterator(conf.get("pool"))) {
        auto key = peer.get("id").copyString();
        auto value = peer.get("endpoint").copyString(); 
        pool[key] = value;
      }
      ss << con.get("pool").toJson() << " (persisted)";
    } else {
      ss << "empty (default)";
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

    ss.clear();
    ss << "Active agents: "
    if (conf.hasKey("active")) { // Persistence only?
      for (auto const& a : VPackArrayIterator(conf.get("active"))) {
        active.push_back(a.copyString());
        ss << a.copyString() << " ";
      }
      ss << conf.get("active").toJson();
    } else {
      ss << "empty (default)";
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

    ss.clear();
    ss << "Min RAFT interval: ";
    if (minPing == 0) { // Command line beats persistence
      if (conf.hasKey("min ping")) {
        minPing = conf.get("min ping").getUInt();
        ss << minPing << " (persisted)";
      } else {
        minPing = 0.5;
        ss << minPing << " (default)";
      }
    } else {
      ss << minPing << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

    ss.clear();
    ss << "Max RAFT interval: ";
    if (maxPing == 0) { // Command line beats persistence
      if (conf.hasKey("max ping")) {
        maxPing = conf.get("max ping").getUInt();
        ss << maxPing << " (persisted)";
      } else {
        maxPing = 2.5;
        ss << maxPing << " (default)";
      }
    } else {
      ss << maxPing << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

    ss.clear();
    ss << "Supervision: ";
    if (supervision == false) { // Command line beats persistence
      if (conf.hasKey("supervision")) {
        supervision = conf.get("supervision").getBoolean();
        ss << supervision << " (persisted)";
      } else {
        supervision = true;
        ss << supervision << " (default)";
      }
    } else {
      ss << supervision << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

    ss.clear();
    ss << "Supervision interval [s]: ";
    if (supervisionFrequency == 0) { // Command line beats persistence
      if (conf.hasKey("supervision frequency")) {
        supervisionFrequency = conf.get("supervision frequency").getDouble();
        ss << supervisionFrequency << " (persisted)";
      } else {
        supervisionFrequency = 2.5;
        ss << supervisionFrequency << " (default)";
      }
    } else {
      ss << supervisionFrequency << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

    ss.clear();
    ss << "Compaction step size: ";
    if (compactionStepSize == 0) { // Command line beats persistence
      if (conf.hasKey("compaction step size")) {
        compactionStepSize = conf.get("compaction step size").getUInt();
        ss << compactionStepSize << " (persisted)";
      } else {
        compactionStepSize = 2.5;
        ss << compactionStepSize << " (default)";
      }
    } else {
      ss << compactionStepSize << " (command line)";
    }
    LOG_TOPIC(DEBUG, Logger::Agency) << ss.str();

  }
  
};
}
}

#endif
