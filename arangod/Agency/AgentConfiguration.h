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

#include "Agency/AgencyCommon.h"
#include "Basics/ReadWriteLock.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <map>
#include <string>

namespace arangodb {
namespace consensus {

static const std::string idStr = "id";
static const std::string agencySizeStr = "agency size";
static const std::string poolSizeStr = "pool size";
static const std::string minPingStr = "min ping";
static const std::string maxPingStr = "max ping";
static const std::string timeoutMultStr = "timeoutMult";
static const std::string endpointStr = "endpoint";
static const std::string uuidStr = "uuid";
static const std::string poolStr = "pool";
static const std::string gossipPeersStr = "gossipPeers";
static const std::string activeStr = "active";
static const std::string supervisionStr = "supervision";
static const std::string waitForSyncStr = "wait for sync";
static const std::string supervisionFrequencyStr = "supervision frequency";
static const std::string supervisionGracePeriodStr = "supervision grace period";
static const std::string compactionStepSizeStr = "compaction step size";
static const std::string compactionKeepSizeStr = "compaction keep size";
static const std::string defaultEndpointStr = "tcp://localhost:8529";
static const std::string versionStr = "version";
static const std::string startupStr = "startup";

struct config_t {
  std::string _id;
  size_t _agencySize;
  size_t _poolSize;
  double _minPing;
  double _maxPing;
  int64_t _timeoutMult;
  std::string _endpoint;
  std::unordered_map<std::string, std::string> _pool;
  std::vector<std::string> _gossipPeers;
  std::vector<std::string> _active;
  bool _supervision;
  bool _waitForSync;
  double _supervisionFrequency;
  uint64_t _compactionStepSize;
  uint64_t _compactionKeepSize;
  double _supervisionGracePeriod;
  bool _cmdLineTimings;
  size_t _version;
  std::string _startup;
  size_t _maxAppendSize;

  mutable arangodb::basics::ReadWriteLock _lock; // guard member variables

  /// @brief default ctor
  config_t();

  /// @brief ctor
  config_t(size_t as, size_t ps, double minp, double maxp, std::string const& e,
           std::vector<std::string> const& g, bool s, bool w, double f,
           uint64_t c, uint64_t k, double p, bool t, size_t a);

  /// @brief copy constructor
  config_t(config_t const&);

  /// @brief move constructor
  config_t(config_t&&);

  /// @brief assignement operator
  config_t& operator=(config_t const&);

  /// @brief move assignment operator
  config_t& operator=(config_t&&);

  /// @brief update leadership changes
  void update(query_t const&);

  /// @brief agent id
  std::string id() const;

  /// @brief pool size
  bool poolComplete() const;

  /// @brief pool size
  bool supervision() const;

  /// @brief pool size
  double supervisionFrequency() const;

  /// @brief wait for sync requested
  bool waitForSync() const;

  /// @brief add pool member
  bool addToPool(std::pair<std::string, std::string> const& i);

  /// @brief active agency size
  size_t size() const;

  /// @brief maximum appendEntries #logs
  size_t maxAppendSize() const;

  /// @brief active empty?
  bool activeEmpty() const;

  /// @brief pool size
  size_t poolSize() const;

  /// @brief pool size
  size_t compactionStepSize() const;

  /// @brief pool size
  size_t compactionKeepSize() const;

  /// @brief pool size
  size_t version() const;

  /// @brief of active agents
  query_t activeToBuilder() const;
  query_t activeAgentsToBuilder() const;
  query_t poolToBuilder() const;

  /// @brief override this configuration with prevailing opinion (startup)
  void override(VPackSlice const& conf);

  /// @brief vpack representation
  query_t toBuilder() const;

  /// @brief set id
  bool setId(std::string const& i);

  /// @brief merge from persisted configuration
  bool merge(VPackSlice const& conf);

  /// @brief gossip peers
  std::vector<std::string> gossipPeers() const;

  /// @brief remove endpoint from gossip peers
  void eraseFromGossipPeers(std::string const& endpoint);

  /// @brief add active agents
  bool activePushBack(std::string const& id);

  /// @brief my endpoint
  std::string endpoint() const;

  /// @brief copy of pool
  std::unordered_map<std::string, std::string> pool() const;

  /// @brief get one pair out of pool
  std::string poolAt(std::string const& id) const;

  /// @brief get active agents
  std::vector<std::string> active() const;

  /// @brief Get minimum RAFT timeout
  double minPing() const;

  /// @brief Get maximum RAFT timeout
  double maxPing() const;

  /// @brief Get timeout multiplier
  int64_t timeoutMult() const;

  /// @brief Reset RAFT timing
  void pingTimes(double, double);

  /// @brief Reset timeout multiplier
  void setTimeoutMult(int64_t);

  /// @brief Supervision grace period
  bool cmdLineTimings() const;

  /// @brief Supervision grace period
  double supervisionGracePeriod() const;

  /// @brief Get replacement for deceased active agent
  bool swapActiveMember(std::string const&, std::string const&);

  /// @brief Get next agent in line of succession
  std::string nextAgentInLine() const;

  /// @brief
  std::string startup() const;

  /// @brief Update an indivdual uuid's endpoint
  bool updateEndpoint(std::string const&, std::string const&);

};
}
}

#endif
