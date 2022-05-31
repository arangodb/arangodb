////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <boost/container_hash/hash.hpp>

#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"

namespace arangodb::replication2::replicated_state::agency {
static inline std::size_t hash_value(Target const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.version);
  boost::hash_combine(seed, s.leader);
  boost::hash_combine(seed, s.participants);
  return seed;
}
static inline std::size_t hash_value(Target::Participant const& s) { return 1; }
static inline std::size_t hash_value(Plan::Participant const& p) {
  return boost::hash_value(p.generation.value);
}
static inline std::size_t hash_value(Plan const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.generation.value);
  boost::hash_combine(seed, s.participants);
  return seed;
}
static inline std::size_t hash_value(Current::ParticipantStatus const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.generation.value);
  boost::hash_combine(seed, s.snapshot.status);
  return seed;
}
static inline std::size_t hash_value(Current::Supervision const& s) {
  return boost::hash_value(s.version);
}
static inline std::size_t hash_value(Current const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.supervision);
  boost::hash_combine(seed, s.participants);
  return seed;
}
static inline std::size_t hash_value(State const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.target);
  boost::hash_combine(seed, s.plan);
  boost::hash_combine(seed, s.current);
  return seed;
}
}  // namespace arangodb::replication2::replicated_state::agency
namespace std {
template<typename T, typename K>
auto hash_value(std::unordered_map<K, T> const& m) {
  std::size_t seed = 0;
  for (auto const& [k, v] : m) {
    std::size_t subseed = 0;
    boost::hash_combine(subseed, v);
    boost::hash_combine(subseed, k);
    seed ^= subseed;
  }
  return seed;
};
}  // namespace std
namespace arangodb::replication2 {
static inline std::size_t hash_value(ParticipantFlags const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.allowedAsLeader);
  boost::hash_combine(seed, s.allowedInQuorum);
  boost::hash_combine(seed, s.forced);
  return seed;
}
}  // namespace arangodb::replication2
namespace arangodb::replication2::agency {
static inline std::size_t hash_value(LogPlanConfig const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.effectiveWriteConcern);
  boost::hash_combine(seed, s.waitForSync);
  return seed;
}
static inline std::size_t hash_value(ParticipantsConfig const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.generation);
  boost::hash_combine(seed, s.participants);
  boost::hash_combine(seed, s.config);
  return seed;
}
static inline std::size_t hash_value(LogTarget const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.version);
  boost::hash_combine(seed, s.leader);
  boost::hash_combine(seed, s.participants);
  return seed;
}
static inline std::size_t hash_value(LogCurrent::Leader const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.serverId);
  boost::hash_combine(seed, s.term.value);
  boost::hash_combine(seed, s.leadershipEstablished);
  return seed;
}
static inline std::size_t hash_value(LogCurrentLocalState const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.term.value);
  boost::hash_combine(seed, s.spearhead.index.value);
  boost::hash_combine(seed, s.spearhead.term.value);
  return seed;
}

static inline std::size_t hash_value(LogCurrent const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.targetVersion);
  boost::hash_combine(seed, s.localState);
  boost::hash_combine(seed, s.leader);
  return seed;
}
static inline std::size_t hash_value(
    LogPlanTermSpecification::Leader const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.serverId);
  boost::hash_combine(seed, s.rebootId.value());
  return seed;
}

static inline std::size_t hash_value(LogPlanTermSpecification const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.term.value);
  boost::hash_combine(seed, s.leader);
  return seed;
}
static inline std::size_t hash_value(LogPlanSpecification const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.id.id());
  boost::hash_combine(seed, s.currentTerm);
  boost::hash_combine(seed, s.participantsConfig);

  return seed;
}

static inline std::size_t hash_value(Log const& s) {
  std::size_t seed = 0;
  boost::hash_combine(seed, s.target);
  boost::hash_combine(seed, s.plan);
  boost::hash_combine(seed, s.current);
  return seed;
}
}  // namespace arangodb::replication2::agency
namespace arangodb::replication2::replicated_log {
static inline std::size_t hash_value(ParticipantHealth const& h) {
  std::size_t seed = 0;
  boost::hash_combine(seed, h.rebootId.value());
  boost::hash_combine(seed, h.notIsFailed);
  return seed;
}
static inline std::size_t hash_value(ParticipantsHealth const& h) {
  return hash_value(h._health);
}
}  // namespace arangodb::replication2::replicated_log
