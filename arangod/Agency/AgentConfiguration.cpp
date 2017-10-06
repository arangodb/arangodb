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

#include "AgentConfiguration.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"

using namespace arangodb::consensus;

config_t::config_t()
    : _agencySize(0),
      _poolSize(0),
      _minPing(0.0),
      _maxPing(0.0),
      _timeoutMult(1),
      _endpoint(defaultEndpointStr),
      _supervision(false),
      _waitForSync(true),
      _supervisionFrequency(5.0),
      _compactionStepSize(2000),
      _compactionKeepSize(500),
      _supervisionGracePeriod(15.0),
      _cmdLineTimings(false),
      _version(0),
      _startup("origin"),
      _maxAppendSize(250),
      _lock()
      {}

config_t::config_t(size_t as, size_t ps, double minp, double maxp,
                   std::string const& e, std::vector<std::string> const& g,
                   bool s, bool w, double f, uint64_t c, uint64_t k, double p,
                   bool t, size_t a)
    : _agencySize(as),
      _poolSize(ps),
      _minPing(minp),
      _maxPing(maxp),
      _timeoutMult(1),
      _endpoint(e),
      _gossipPeers(g),
      _supervision(s),
      _waitForSync(w),
      _supervisionFrequency(f),
      _compactionStepSize(c),
      _compactionKeepSize(k),      
      _supervisionGracePeriod(p),
      _cmdLineTimings(t),
      _version(0),
      _startup("origin"),
      _maxAppendSize(a),
      _lock()
      {}

config_t::config_t(config_t const& other) { *this = other; }

config_t::config_t(config_t&& other)
    : _id(std::move(other._id)),
      _agencySize(std::move(other._agencySize)),
      _poolSize(std::move(other._poolSize)),
      _minPing(std::move(other._minPing)),
      _maxPing(std::move(other._maxPing)),
      _timeoutMult(std::move(other._timeoutMult)),
      _endpoint(std::move(other._endpoint)),
      _pool(std::move(other._pool)),
      _gossipPeers(std::move(other._gossipPeers)),
      _active(std::move(other._active)),
      _supervision(std::move(other._supervision)),
      _waitForSync(std::move(other._waitForSync)),
      _supervisionFrequency(std::move(other._supervisionFrequency)),
      _compactionStepSize(std::move(other._compactionStepSize)),
      _compactionKeepSize(std::move(other._compactionKeepSize)),
      _supervisionGracePeriod(std::move(other._supervisionGracePeriod)),
      _cmdLineTimings(std::move(other._cmdLineTimings)),
      _version(std::move(other._version)),
      _startup(std::move(other._startup)),
      _maxAppendSize(std::move(other._maxAppendSize)){}

config_t& config_t::operator=(config_t const& other) {
  // must hold the lock of other to copy _pool, _minPing, _maxPing etc.
  READ_LOCKER(readLocker, other._lock);

  _id = other._id;
  _agencySize = other._agencySize;
  _poolSize = other._poolSize;
  _minPing = other._minPing;
  _maxPing = other._maxPing;
  _timeoutMult = other._timeoutMult;
  _endpoint = other._endpoint;
  _pool = other._pool;
  _gossipPeers = other._gossipPeers;
  _active = other._active;
  _supervision = other._supervision;
  _waitForSync = other._waitForSync;
  _supervisionFrequency = other._supervisionFrequency;
  _compactionStepSize = other._compactionStepSize;
  _compactionKeepSize = other._compactionKeepSize;
  _supervisionGracePeriod = other._supervisionGracePeriod;
  _cmdLineTimings = other._cmdLineTimings;
  _version = other._version;
  _startup = other._startup;
  _maxAppendSize = other._maxAppendSize;
  return *this;
}

config_t& config_t::operator=(config_t&& other) {
  _id = std::move(other._id);
  _agencySize = std::move(other._agencySize);
  _poolSize = std::move(other._poolSize);
  _minPing = std::move(other._minPing);
  _maxPing = std::move(other._maxPing);
  _timeoutMult = std::move(other._timeoutMult);
  _endpoint = std::move(other._endpoint);
  _pool = std::move(other._pool);
  _gossipPeers = std::move(other._gossipPeers);
  _active = std::move(other._active);
  _supervision = std::move(other._supervision);
  _waitForSync = std::move(other._waitForSync);
  _supervisionFrequency = std::move(other._supervisionFrequency);
  _compactionStepSize = std::move(other._compactionStepSize);
  _compactionKeepSize = std::move(other._compactionKeepSize);
  _supervisionGracePeriod = std::move(other._supervisionGracePeriod);
  _cmdLineTimings = std::move(other._cmdLineTimings);
  _version = std::move(other._version);
  _startup = std::move(other._startup);
  _maxAppendSize = std::move(other._maxAppendSize);
  return *this;
}

size_t config_t::version() const {
  READ_LOCKER(readLocker, _lock);
  return _version;
}

bool config_t::cmdLineTimings() const {
  READ_LOCKER(readLocker, _lock);
  return _cmdLineTimings;
}

double config_t::supervisionGracePeriod() const {
  READ_LOCKER(readLocker, _lock);
  return _supervisionGracePeriod;
}

double config_t::minPing() const {
  READ_LOCKER(readLocker, _lock);
  return _minPing;
}

double config_t::maxPing() const {
  READ_LOCKER(readLocker, _lock);
  return _maxPing;
}

int64_t config_t::timeoutMult() const {
  READ_LOCKER(readLocker, _lock);
  return _timeoutMult;
}

void config_t::pingTimes(double minPing, double maxPing) {
  WRITE_LOCKER(writeLocker, _lock);
  if (_minPing != minPing || _maxPing != maxPing ) {
    _minPing = minPing;
    _maxPing = maxPing;
    ++_version;
  }
}

void config_t::setTimeoutMult(int64_t m) {
  WRITE_LOCKER(writeLocker, _lock);
  if (_timeoutMult != m) {
    _timeoutMult = m;
    ++_version;
  }
}

std::unordered_map<std::string, std::string> config_t::pool() const {
  READ_LOCKER(readLocker, _lock);
  return _pool;
}

std::string config_t::id() const {
  READ_LOCKER(readLocker, _lock);
  return _id;
}

std::string config_t::poolAt(std::string const& id) const {
  READ_LOCKER(readLocker, _lock);
  return _pool.at(id);
}

std::string config_t::endpoint() const {
  READ_LOCKER(readLocker, _lock);
  return _endpoint;
}

std::vector<std::string> config_t::active() const {
  READ_LOCKER(readLocker, _lock);
  return _active;
}

bool config_t::activeEmpty() const {
  READ_LOCKER(readLocker, _lock);
  return _active.empty();
}

bool config_t::waitForSync() const {
  READ_LOCKER(readLocker, _lock);
  return _waitForSync;
}

bool config_t::supervision() const {
  READ_LOCKER(readLocker, _lock);
  return _supervision;
}

double config_t::supervisionFrequency() const {
  READ_LOCKER(readLocker, _lock);
  return _supervisionFrequency;
}

bool config_t::activePushBack(std::string const& id) {
  WRITE_LOCKER(writeLocker, _lock);
  if (_active.size() < _agencySize &&
      std::find(_active.begin(), _active.end(), id) == _active.end()) {
    _active.push_back(id);
    ++_version;
    return true;
  }
  return false;
}

std::vector<std::string> config_t::gossipPeers() const {
  READ_LOCKER(readLocker, _lock);
  return _gossipPeers;
}

void config_t::eraseFromGossipPeers(std::string const& endpoint) {
  WRITE_LOCKER(readLocker, _lock);
  if (std::find(_gossipPeers.begin(), _gossipPeers.end(), endpoint) !=
      _gossipPeers.end()) {
    _gossipPeers.erase(
      std::remove(_gossipPeers.begin(), _gossipPeers.end(), endpoint),
      _gossipPeers.end());
    ++_version;
  }
}

bool config_t::addToPool(std::pair<std::string, std::string> const& i) {
  WRITE_LOCKER(readLocker, _lock);
  if (_pool.find(i.first) == _pool.end()) {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Adding " << i.first << "(" << i.second << ") to agent pool";
    _pool[i.first] = i.second;
    ++_version;
  } else {
    if (_pool.at(i.first) != i.second) {  /// discrepancy!
      return false;
    }
  }
  return true;
}

bool config_t::swapActiveMember(
  std::string const& failed, std::string const& repl) {
  try {
    WRITE_LOCKER(writeLocker, _lock);
    LOG_TOPIC(INFO, Logger::AGENCY) << "Replacing " << failed << " with " << repl;
    std::replace (_active.begin(), _active.end(), failed, repl);
    ++_version;
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Replacing " << failed << " with " << repl << "failed : " << e.what();
    return false;
  }

  return true;
}

std::string config_t::nextAgentInLine() const {

  READ_LOCKER(readLocker, _lock);

  if (_poolSize > _agencySize) {
    for (const auto& p : _pool) {
      if (std::find(_active.begin(), _active.end(), p.first) == _active.end()) {
        return p.first;
      }
    }
  }
  
  return ""; // No one left
}

size_t config_t::maxAppendSize() const {
  READ_LOCKER(readLocker, _lock);
  return _maxAppendSize;
}

size_t config_t::compactionStepSize() const {
  READ_LOCKER(readLocker, _lock);
  return _compactionStepSize;
}

size_t config_t::compactionKeepSize() const {
  READ_LOCKER(readLocker, _lock);
  return _compactionKeepSize;
}

size_t config_t::size() const {
  READ_LOCKER(readLocker, _lock);
  return _agencySize;
}

size_t config_t::poolSize() const {
  READ_LOCKER(readLocker, _lock);
  return _poolSize;
}

bool config_t::poolComplete() const {
  READ_LOCKER(readLocker, _lock);
  return _poolSize == _pool.size();
}

query_t config_t::activeToBuilder() const {
  query_t ret = std::make_shared<arangodb::velocypack::Builder>();
  {
    READ_LOCKER(readLocker, _lock);
    VPackArrayBuilder r(ret.get());
    for (auto const& i : _active) {
      ret->add(VPackValue(i));
    }
  }
  return ret;
}

query_t config_t::activeAgentsToBuilder() const {
  query_t ret = std::make_shared<arangodb::velocypack::Builder>();
  {
    READ_LOCKER(readLocker, _lock);
    VPackObjectBuilder r(ret.get());
    for (auto const& i : _active) {
      ret->add(i, VPackValue(_pool.at(i)));
    }
  }
  return ret;
}

query_t config_t::poolToBuilder() const {
  query_t ret = std::make_shared<arangodb::velocypack::Builder>();
  {
    READ_LOCKER(readLocker, _lock);
    VPackObjectBuilder r(ret.get());
    for (auto const& i : _pool) {
      ret->add(i.first, VPackValue(i.second));
    }
  }
  return ret;
}


bool config_t::updateEndpoint(std::string const& id, std::string const& ep) {
  WRITE_LOCKER(readLocker, _lock);
  if (_pool[id] != ep) {
    _pool[id] = ep;
    ++_version;
    return true;
  }
  return false;
}


void config_t::update(query_t const& message) {
  VPackSlice slice = message->slice();
  std::unordered_map<std::string, std::string> pool;
  bool changed = false;
  for (auto const& p : VPackObjectIterator(slice.get(poolStr))) {
    auto const& id = p.key.copyString();
    if (id != _id) {
      pool[id] = p.value.copyString();
    } else {
      pool[id] = _endpoint;
    }
  }
  std::vector<std::string> active;
  for (auto const& a : VPackArrayIterator(slice.get(activeStr))) {
    active.push_back(a.copyString());
  }
  double minPing = slice.get(minPingStr).getNumber<double>();
  double maxPing = slice.get(maxPingStr).getNumber<double>();
  int64_t timeoutMult = slice.get(timeoutMultStr).getNumber<int64_t>();
  WRITE_LOCKER(writeLocker, _lock);
  if (pool != _pool) {
    _pool = pool;
    changed=true;
  }
  if (active != _active) {
    _active = active;
    changed=true;
  }
  if (minPing != _minPing) {
    _minPing = minPing;
    changed=true;
  }
  if (maxPing != _maxPing) {
    _maxPing = maxPing;
    changed=true;
  }
  if (timeoutMult != _timeoutMult) {
    _timeoutMult = timeoutMult;
    changed=true;
  }
  if (changed) {
    ++_version;
  }
}

/// @brief override this configuration with prevailing opinion (startup)
void config_t::override(VPackSlice const& conf) {
  WRITE_LOCKER(writeLocker, _lock);

  if (conf.hasKey(agencySizeStr) && conf.get(agencySizeStr).isUInt()) {
    _agencySize = conf.get(agencySizeStr).getUInt();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override " << agencySizeStr
                                   << " from " << conf.toJson();
  }

  if (conf.hasKey(poolSizeStr) && conf.get(poolSizeStr).isUInt()) {
    _poolSize = conf.get(poolSizeStr).getUInt();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override " << poolSizeStr
                                   << " from " << conf.toJson();
  }

  if (conf.hasKey(minPingStr) && conf.get(minPingStr).isDouble()) {
    _minPing = conf.get(minPingStr).getNumber<double>();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override " << minPingStr
                                   << " from " << conf.toJson();
  }

  if (conf.hasKey(maxPingStr) && conf.get(maxPingStr).isDouble()) {
    _maxPing = conf.get(maxPingStr).getNumber<double>();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override " << maxPingStr
                                   << " from " << conf.toJson();
  }

  if (conf.hasKey(timeoutMultStr) && conf.get(timeoutMultStr).isInteger()) {
    _timeoutMult = conf.get(timeoutMultStr).getNumber<int64_t>();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override " << timeoutMultStr
                                   << " from " << conf.toJson();
  }

  if (conf.hasKey(poolStr) && conf.get(poolStr).isArray()) {
    _pool.clear();
    for (auto const& peer : VPackArrayIterator(conf.get(poolStr))) {
      auto key = peer.get(idStr).copyString();
      auto value = peer.get(endpointStr).copyString();
      _pool[key] = value;
    }
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override " << poolStr
                                   << " from " << conf.toJson();
  }

  if (conf.hasKey(activeStr) && conf.get(activeStr).isArray()) {
    _active.clear();
    for (auto const& peer : VPackArrayIterator(conf.get(activeStr))) {
      _active.push_back(peer.copyString());
    }
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override poolSize from "
                                   << conf.toJson();
  }

  if (conf.hasKey(supervisionStr) && conf.get(supervisionStr).isBoolean()) {
    _supervision = conf.get(supervisionStr).getBoolean();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override " << supervisionStr
                                   << " from " << conf.toJson();
  }

  if (conf.hasKey(waitForSyncStr) && conf.get(waitForSyncStr).isBoolean()) {
    _waitForSync = conf.get(waitForSyncStr).getBoolean();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override " << waitForSyncStr
                                   << " from " << conf.toJson();
  }

  if (conf.hasKey(supervisionFrequencyStr) &&
      conf.get(supervisionFrequencyStr).isDouble()) {
    _supervisionFrequency = conf.get(supervisionFrequencyStr).getNumber<double>();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override "
                                   << supervisionFrequencyStr << " from "
                                   << conf.toJson();
  }

  if (conf.hasKey(compactionStepSizeStr) &&
      conf.get(compactionStepSizeStr).isUInt()) {
    _compactionStepSize = conf.get(compactionStepSizeStr).getUInt();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override "
                                   << compactionStepSizeStr << " from "
                                   << conf.toJson();
  }

  if (conf.hasKey(compactionKeepSizeStr) &&
      conf.get(compactionKeepSizeStr).isUInt()) {
    _compactionKeepSize = conf.get(compactionKeepSizeStr).getUInt();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to override "
                                   << compactionKeepSizeStr << " from "
                                   << conf.toJson();
  }

  ++_version;
}

/// @brief vpack representation
query_t config_t::toBuilder() const {

  query_t ret = std::make_shared<arangodb::velocypack::Builder>();
  {
    VPackObjectBuilder b(ret.get());
    READ_LOCKER(readLocker, _lock);

    ret->add(VPackValue(poolStr));
    {
      VPackObjectBuilder bb(ret.get());
      for (auto const& i : _pool) {
        ret->add(i.first, VPackValue(i.second));
      }
    }

    ret->add(VPackValue(activeStr));
    {
      VPackArrayBuilder bb(ret.get());
      for (auto const& i : _active) {
        ret->add(VPackValue(i));
      }
    }

    ret->add(idStr, VPackValue(_id));
    ret->add(agencySizeStr, VPackValue(_agencySize));
    ret->add(poolSizeStr, VPackValue(_poolSize));
    ret->add(endpointStr, VPackValue(_endpoint));
    ret->add(minPingStr, VPackValue(_minPing));
    ret->add(maxPingStr, VPackValue(_maxPing));
    ret->add(timeoutMultStr, VPackValue(_timeoutMult));
    ret->add(supervisionStr, VPackValue(_supervision));
    ret->add(supervisionFrequencyStr, VPackValue(_supervisionFrequency));
    ret->add(compactionStepSizeStr, VPackValue(_compactionStepSize));
    ret->add(compactionKeepSizeStr, VPackValue(_compactionKeepSize));
    ret->add(supervisionGracePeriodStr, VPackValue(_supervisionGracePeriod));
    ret->add(versionStr, VPackValue(_version));
    ret->add(startupStr, VPackValue(_startup));
  }

  return ret;
}

// Set my id
bool config_t::setId(std::string const& i) {
  WRITE_LOCKER(writeLocker, _lock);
  if (_id.empty()) {
    _id = i;
    _pool[_id] = _endpoint;  // Register my endpoint with it
    ++_version;
    return true;
  } else {
    return false;
  }
}

// Get startup fix
std::string config_t::startup() const {
  READ_LOCKER(readLocker, _lock);
  return _startup;
}

/// @brief merge from persisted configuration
bool config_t::merge(VPackSlice const& conf) {
  WRITE_LOCKER(writeLocker, _lock); // All must happen under the lock or else ...

  _id = conf.get(idStr).copyString();  // I get my id
  _startup = "persistence";

  std::stringstream ss;
  ss << "Agency size: ";
  if (conf.hasKey(agencySizeStr)) {  // Persistence beats command line
    _agencySize = conf.get(agencySizeStr).getUInt();
    ss << _agencySize << " (persisted)";
  } else {
    if (_agencySize == 0) {
      _agencySize = 1;
      ss << _agencySize << " (default)";
    } else {
      ss << _agencySize << " (command line)";
    }
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Agency pool size: ";
  if (_poolSize == 0) {  // Command line beats persistence
    if (conf.hasKey(poolSizeStr)) {
      _poolSize = conf.get(poolSizeStr).getUInt();
      ss << _poolSize << " (persisted)";
    } else {
      _poolSize = _agencySize;
      ss << _poolSize << " (default)";
    }
  } else {
    ss << _poolSize << " (command line)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Agent pool: ";
  if (conf.hasKey(poolStr)) {  // Persistence only
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Found agent pool in persistence:";
    for (auto const& peer : VPackObjectIterator(conf.get(poolStr))) {
      auto const& id = peer.key.copyString();
      if (id != _id) {
        _pool[id] = peer.value.copyString();
      } else {
        _pool[id] = _endpoint;
      }
    }
    ss << conf.get(poolStr).toJson() << " (persisted)";
  } else {
    ss << "empty (default)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Active agents: ";
  if (conf.hasKey(activeStr)) {  // Persistence only?
    for (auto const& a : VPackArrayIterator(conf.get(activeStr))) {
      _active.push_back(a.copyString());
      ss << a.copyString() << " ";
    }
    ss << conf.get(activeStr).toJson();
  } else {
    ss << "empty (default)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Min RAFT interval: ";
  if (_minPing == 0) {  // Command line beats persistence
    if (conf.hasKey(minPingStr)) {
      _minPing = conf.get(minPingStr).getNumber<double>();
      ss << _minPing << " (persisted)";
    } else {
      _minPing = 1.0;
      ss << _minPing << " (default)";
    }
  } else {
    ss << _minPing << " (command line)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Max RAFT interval: ";
  if (_maxPing == 0) {  // Command line beats persistence
    if (conf.hasKey(maxPingStr)) {
      _maxPing = conf.get(maxPingStr).getNumber<double>();
      ss << _maxPing << " (persisted)";
    } else {
      _maxPing = 5.0;
      ss << _maxPing << " (default)";
    }
  } else {
    ss << _maxPing << " (command line)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Supervision: ";
  if (_supervision == false) {  // Command line beats persistence
    if (conf.hasKey(supervisionStr)) {
      _supervision = conf.get(supervisionStr).getBoolean();
      ss << _supervision << " (persisted)";
    } else {
      _supervision = true;
      ss << _supervision << " (default)";
    }
  } else {
    ss << _supervision << " (command line)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Supervision interval [s]: ";
  if (_supervisionFrequency == 0) {  // Command line beats persistence
    if (conf.hasKey(supervisionFrequencyStr)) {
      _supervisionFrequency = conf.get(supervisionFrequencyStr).getNumber<double>();
      ss << _supervisionFrequency << " (persisted)";
    } else {
      _supervisionFrequency = 2.5;
      ss << _supervisionFrequency << " (default)";
    }
  } else {
    ss << _supervisionFrequency << " (command line)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Compaction step size: ";
  if (_compactionStepSize == 0) {  // Command line beats persistence
    if (conf.hasKey(compactionStepSizeStr)) {
      _compactionStepSize = conf.get(compactionStepSizeStr).getUInt();
      ss << _compactionStepSize << " (persisted)";
    } else {
      _compactionStepSize = 2000;
      ss << _compactionStepSize << " (default)";
    }
  } else {
    ss << _compactionStepSize << " (command line)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Compaction keep size: ";
  if (_compactionKeepSize == 0) {  // Command line beats persistence
    if (conf.hasKey(compactionKeepSizeStr)) {
      _compactionKeepSize = conf.get(compactionKeepSizeStr).getUInt();
      ss << _compactionKeepSize << " (persisted)";
    } else {
      _compactionStepSize =  500;
      ss << _compactionKeepSize << " (default)";
    }
  } else {
    ss << _compactionKeepSize << " (command line)";
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << ss.str();
  ++_version;
  return true;
}


