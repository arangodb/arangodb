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

struct config_t {
 private:
  std::string _id;
  std::string _recoveryId;
  size_t _agencySize;
  size_t _poolSize;
  double _minPing;
  double _maxPing;
  int64_t _timeoutMult;
  std::string _endpoint;
  std::unordered_map<std::string, std::string> _pool;
  std::unordered_set<std::string> _gossipPeers;
  std::vector<std::string> _active;
  bool _supervision;
  bool _supervisionTouched;
  bool _waitForSync;
  double _supervisionFrequency;
  uint64_t _compactionStepSize;
  uint64_t _compactionKeepSize;
  double _supervisionGracePeriod;
  double _supervisionOkThreshold;
  bool _cmdLineTimings;
  size_t _version;
  std::string _startup;
  size_t _maxAppendSize;

  mutable arangodb::basics::ReadWriteLock _lock;  // guard member variables

 public:
  static std::string const idStr;
  static std::string const agencySizeStr;
  static std::string const poolSizeStr;
  static std::string const minPingStr;
  static std::string const maxPingStr;
  static std::string const timeoutMultStr;
  static std::string const endpointStr;
  static std::string const uuidStr;
  static std::string const poolStr;
  static std::string const gossipPeersStr;
  static std::string const activeStr;
  static std::string const supervisionStr;
  static std::string const waitForSyncStr;
  static std::string const supervisionFrequencyStr;
  static std::string const supervisionGracePeriodStr;
  static std::string const supervisionOkThresholdStr;
  static std::string const compactionStepSizeStr;
  static std::string const compactionKeepSizeStr;
  static std::string const defaultEndpointStr;
  static std::string const versionStr;
  static std::string const startupStr;

  using upsert_t = enum { UNCHANGED = 0, CHANGED, WRONG };

  /// @brief default ctor
  config_t();

  /// @brief ctor
  config_t(std::string const& rid, size_t as, size_t ps, double minp, double maxp,
           std::string const& e, std::vector<std::string> const& g, bool s, bool st,
           bool w, double f, uint64_t c, uint64_t k, double p, double o, bool t, size_t a);

  /// @brief copy constructor
  config_t(config_t const&);

  /// @brief move constructor
  config_t(config_t&&) = delete;

  /// @brief assignement operator
  config_t& operator=(config_t const&);

  /// @brief move assignment operator
  config_t& operator=(config_t&&);

  /// @brief update leadership changes
  void update(query_t const&);

  /// @brief agent id
  std::string id() const;

  /// @brief agent id
  std::string recoveryId() const;

  /// @brief pool completed
  bool poolComplete() const;

  /// @brief is supervision enables
  bool supervision() const;

  /// @brief pool size
  double supervisionFrequency() const;

  /// @brief wait for sync requested
  bool waitForSync() const;

  /// @brief find id in pool
  bool findInPool(std::string const&) const;

  /// @brief match id and endpoint with pool
  bool matchPeer(std::string const& id, std::string const& endpoint) const;

  /**
   * @brief           Verify other agent's pool against our own:
   *                  - We only get here, if our pool is not complete yet or the
   *                    id is member of this agency
   *                  - We match their pool to ours and allow only for an update
   *                    of it's own endpoint
   *
   * @param otherPool Other agent's pool
   * @param otherId   Other agent's id
   *
   * @return          Success
   */
  upsert_t upsertPool(VPackSlice const& otherPool, std::string const& otherId);

  /// @brief active agency size
  void activate();

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

  /// @brief vpack representation
  query_t toBuilder() const;

  /// @brief vpack representation
  void toBuilder(VPackBuilder&) const;

  /// @brief set id
  bool setId(std::string const& i);

  /// @brief merge from persisted configuration
  bool merge(VPackSlice const& conf);

  /// @brief gossip peers
  std::unordered_set<std::string> gossipPeers() const;

  /// @brief remove endpoint from gossip peers
  size_t eraseGossipPeer(std::string const& endpoint);

  /// @brief remove endpoint from gossip peers
  bool addGossipPeer(std::string const& endpoint);

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

  /// @brief Supervision ok threshold
  double supervisionOkThreshold() const;

  /// @brief set Supervision grace period
  void setSupervisionGracePeriod(double d) {
    _supervisionGracePeriod = d;
  }

  /// @brief set Supervision ok threshold
  void setSupervisionOkThreshold(double d) {
    _supervisionOkThreshold = d;
  }

  /// @brief
  std::string startup() const;

  /// @brief Update an indivdual uuid's endpoint
  bool updateEndpoint(std::string const&, std::string const&);

  /// @brief Update configuration with an other
  void updateConfiguration(VPackSlice const& other);
};
}  // namespace consensus
}  // namespace arangodb

#endif
