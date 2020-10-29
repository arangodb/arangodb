////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb::consensus;

std::string const config_t::idStr = "id";
std::string const config_t::agencySizeStr = "agency size";
std::string const config_t::poolSizeStr = "pool size";
std::string const config_t::minPingStr = "min ping";
std::string const config_t::maxPingStr = "max ping";
std::string const config_t::timeoutMultStr = "timeoutMult";
std::string const config_t::endpointStr = "endpoint";
std::string const config_t::uuidStr = "uuid";
std::string const config_t::poolStr = "pool";
std::string const config_t::gossipPeersStr = "gossipPeers";
std::string const config_t::activeStr = "active";
std::string const config_t::supervisionStr = "supervision";
std::string const config_t::waitForSyncStr = "wait for sync";
std::string const config_t::supervisionFrequencyStr = "supervision frequency";
std::string const config_t::supervisionGracePeriodStr =
    "supervision grace period";
std::string const config_t::supervisionOkThresholdStr =
    "supervision ok threshold";
std::string const config_t::compactionStepSizeStr = "compaction step size";
std::string const config_t::compactionKeepSizeStr = "compaction keep size";
std::string const config_t::defaultEndpointStr = "tcp://localhost:8529";
std::string const config_t::versionStr = "version";
std::string const config_t::startupStr = "startup";

config_t::config_t()
    : _agencySize(0),
      _poolSize(0),
      _minPing(0.0),
      _maxPing(0.0),
      _timeoutMult(1),
      _endpoint(defaultEndpointStr),
      _supervision(false),
      _supervisionTouched(false),
      _waitForSync(true),
      _supervisionFrequency(5.0),
      _compactionStepSize(1000),
      _compactionKeepSize(50000),
      _supervisionGracePeriod(15.0),
      _supervisionOkThreshold(5.0),
      _cmdLineTimings(false),
      _version(0),
      _startup("origin"),
      _maxAppendSize(250),
      _lock() {}

config_t::config_t(std::string const& rid, size_t as, size_t ps, double minp,
                   double maxp, std::string const& e,
                   std::vector<std::string> const& g, bool s, bool st, bool w,
                   double f, uint64_t c, uint64_t k, double p, double o, bool t, size_t a)
    : _recoveryId(rid),
      _agencySize(as),
      _poolSize(ps),
      _minPing(minp),
      _maxPing(maxp),
      _timeoutMult(1),
      _endpoint(e),
      _gossipPeers(g.begin(), g.end()),
      _supervision(s),
      _supervisionTouched(st),
      _waitForSync(w),
      _supervisionFrequency(f),
      _compactionStepSize(c),
      _compactionKeepSize(k),
      _supervisionGracePeriod(p),
      _supervisionOkThreshold(o),
      _cmdLineTimings(t),
      _version(0),
      _startup("origin"),
      _maxAppendSize(a),
      _lock() {}

config_t::config_t(config_t const& other) {
  // will call operator=, which will ensure proper locking
  *this = other;
}

config_t& config_t::operator=(config_t const& other) {
  // must hold the lock of other to copy _pool, _minPing, _maxPing etc.
  READ_LOCKER(readLocker, other._lock);
  _id = other._id;
  _recoveryId = other._recoveryId;
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
  _supervisionTouched = other._supervisionTouched;
  _waitForSync = other._waitForSync;
  _supervisionFrequency = other._supervisionFrequency;
  _compactionStepSize = other._compactionStepSize;
  _compactionKeepSize = other._compactionKeepSize;
  _supervisionGracePeriod = other._supervisionGracePeriod;
  _supervisionOkThreshold = other._supervisionOkThreshold;
  _cmdLineTimings = other._cmdLineTimings;
  _version = other._version;
  _startup = other._startup;
  _maxAppendSize = other._maxAppendSize;
  return *this;
}

config_t& config_t::operator=(config_t&& other) {
  READ_LOCKER(readLocker, other._lock);

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
  _supervisionTouched = std::move(other._supervisionTouched);
  _waitForSync = std::move(other._waitForSync);
  _supervisionFrequency = std::move(other._supervisionFrequency);
  _compactionStepSize = std::move(other._compactionStepSize);
  _compactionKeepSize = std::move(other._compactionKeepSize);
  _supervisionGracePeriod = std::move(other._supervisionGracePeriod);
  _supervisionOkThreshold = std::move(other._supervisionOkThreshold);
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

double config_t::supervisionOkThreshold() const {
  READ_LOCKER(readLocker, _lock);
  return _supervisionOkThreshold;
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
  if (_minPing != minPing || _maxPing != maxPing) {
    _minPing = minPing;
    _maxPing = maxPing;
    ++_version;
  }
}

void config_t::setTimeoutMult(int64_t m) {
  WRITE_LOCKER(writeLocker, _lock);
  if (_timeoutMult != m) {
    _timeoutMult = m;
    // this is called during election, do NOT update ++_version
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

std::string config_t::recoveryId() const {
  READ_LOCKER(readLocker, _lock);
  return _recoveryId;
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

void config_t::activate() {
  WRITE_LOCKER(readLocker, _lock);
  _active.clear();
  for (auto const& pair : _pool) {
    _active.push_back(pair.first);
  }
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

std::unordered_set<std::string> config_t::gossipPeers() const {
  READ_LOCKER(lock, _lock);
  return _gossipPeers;
}

size_t config_t::eraseGossipPeer(std::string const& endpoint) {
  WRITE_LOCKER(lock, _lock);
  auto ret = _gossipPeers.erase(endpoint);
  ++_version;
  return ret;
}

bool config_t::addGossipPeer(std::string const& endpoint) {
  WRITE_LOCKER(lock, _lock);
  ++_version;
  return _gossipPeers.emplace(endpoint).second;
}

config_t::upsert_t config_t::upsertPool(VPackSlice const& otherPool,
                                        std::string const& otherId) {
  WRITE_LOCKER(lock, _lock);
  for (auto const& entry : VPackObjectIterator(otherPool)) {
    auto const id = entry.key.copyString();
    auto const endpoint = entry.value.copyString();
    if (_pool.find(id) == _pool.end()) {
      LOG_TOPIC("95b8d", INFO, Logger::AGENCY) << "Adding " << id << "(" << endpoint << ") to agent pool";
      _pool[id] = endpoint;
      ++_version;
      return CHANGED;
    } else {
      if (_pool.at(id) != endpoint) {
        if (id != otherId) {  /// discrepancy!
          return WRONG;
        } else {  /// we trust the other guy on his own endpoint
          _pool.at(id) = endpoint;
        }
      }
    }
  }
  return UNCHANGED;
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
    changed = true;
  }
  if (active != _active) {
    _active = active;
    changed = true;
  }
  if (minPing != _minPing) {
    _minPing = minPing;
    changed = true;
  }
  if (maxPing != _maxPing) {
    _maxPing = maxPing;
    changed = true;
  }
  if (timeoutMult != _timeoutMult) {
    _timeoutMult = timeoutMult;
    changed = true;
  }
  if (changed) {
    ++_version;
  }
}

void config_t::toBuilder(VPackBuilder& builder) const {
  READ_LOCKER(readLocker, _lock);
  {
    builder.add(VPackValue(poolStr));
    {
      VPackObjectBuilder bb(&builder);
      for (auto const& i : _pool) {
        builder.add(i.first, VPackValue(i.second));
      }
    }

    builder.add(VPackValue(activeStr));
    {
      VPackArrayBuilder bb(&builder);
      for (auto const& i : _active) {
        builder.add(VPackValue(i));
      }
    }

    builder.add(idStr, VPackValue(_id));
    builder.add(agencySizeStr, VPackValue(_agencySize));
    builder.add(poolSizeStr, VPackValue(_poolSize));
    builder.add(endpointStr, VPackValue(_endpoint));
    builder.add(minPingStr, VPackValue(_minPing));
    builder.add(maxPingStr, VPackValue(_maxPing));
    builder.add(timeoutMultStr, VPackValue(_timeoutMult));
    builder.add(supervisionStr, VPackValue(_supervision));
    builder.add(supervisionFrequencyStr, VPackValue(_supervisionFrequency));
    builder.add(compactionStepSizeStr, VPackValue(_compactionStepSize));
    builder.add(compactionKeepSizeStr, VPackValue(_compactionKeepSize));
    builder.add(supervisionGracePeriodStr, VPackValue(_supervisionGracePeriod));
    builder.add(supervisionOkThresholdStr, VPackValue(_supervisionOkThreshold));
    builder.add(versionStr, VPackValue(_version));
    builder.add(startupStr, VPackValue(_startup));
  }
}

/// @brief vpack representation
query_t config_t::toBuilder() const {
  query_t ret = std::make_shared<arangodb::velocypack::Builder>();
  {
    VPackObjectBuilder a(ret.get());
    toBuilder(*ret);
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

/// @brief findIdInPool
bool config_t::matchPeer(std::string const& id, std::string const& endpoint) const {
  READ_LOCKER(readLocker, _lock);
  auto const& it = _pool.find(id);
  return (it == _pool.end()) ? false : it->second == endpoint;
}

/// @brief findIdInPool
bool config_t::findInPool(std::string const& id) const {
  READ_LOCKER(readLocker, _lock);
  return _pool.find(id) != _pool.end();
}

/// @brief merge from persisted configuration
bool config_t::merge(VPackSlice const& conf) {
  WRITE_LOCKER(writeLocker, _lock);  // All must happen under the lock or else ...

  // FIXME: All these "command line beats persistence" are wrong, since
  // the given default values never happen. Only fixed _supervision with
  // _supervisionTouched as an emergency measure.
  _id = conf.get(idStr).copyString();  // I get my id
  _recoveryId.clear();
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
  LOG_TOPIC("c0b77", DEBUG, Logger::AGENCY) << ss.str();

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
  LOG_TOPIC("474ea", DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Agent pool: ";
  if (conf.hasKey(poolStr)) {  // Persistence only
    LOG_TOPIC("fc6ad", DEBUG, Logger::AGENCY) << "Found agent pool in persistence:";
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
  LOG_TOPIC("445ea", DEBUG, Logger::AGENCY) << ss.str();

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
  LOG_TOPIC("00f99", DEBUG, Logger::AGENCY) << ss.str();

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
  LOG_TOPIC("7095f", DEBUG, Logger::AGENCY) << ss.str();

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
  LOG_TOPIC("d2569", DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Supervision: ";
  if (!_supervisionTouched) {  // Command line beats persistence
    if (conf.hasKey(supervisionStr)) {
      _supervision = conf.get(supervisionStr).getBoolean();
      ss << _supervision << " (persisted)";
    } else {
      ss << _supervision << " (default)";
    }
  } else {
    ss << _supervision << " (command line)";
  }
  LOG_TOPIC("6f913", DEBUG, Logger::AGENCY) << ss.str();

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
  LOG_TOPIC("cb813", DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Compaction step size: ";
  if (_compactionStepSize == 0) {  // Command line beats persistence
    if (conf.hasKey(compactionStepSizeStr)) {
      _compactionStepSize = conf.get(compactionStepSizeStr).getUInt();
      ss << _compactionStepSize << " (persisted)";
    } else {
      _compactionStepSize = 1000;
      ss << _compactionStepSize << " (default)";
    }
  } else {
    ss << _compactionStepSize << " (command line)";
  }
  LOG_TOPIC("06b38", DEBUG, Logger::AGENCY) << ss.str();

  ss.str("");
  ss.clear();
  ss << "Compaction keep size: ";
  if (_compactionKeepSize == 0) {  // Command line beats persistence
    if (conf.hasKey(compactionKeepSizeStr)) {
      _compactionKeepSize = conf.get(compactionKeepSizeStr).getUInt();
      ss << _compactionKeepSize << " (persisted)";
    } else {
      _compactionKeepSize = 50000;
      ss << _compactionKeepSize << " (default)";
    }
  } else {
    ss << _compactionKeepSize << " (command line)";
  }
  LOG_TOPIC("ebf13", DEBUG, Logger::AGENCY) << ss.str();
  ++_version;
  return true;
}

void config_t::updateConfiguration(VPackSlice const& other) {
  WRITE_LOCKER(writeLocker, _lock);

  auto pool = other.get(poolStr);
  TRI_ASSERT(pool.isObject());
  _pool.clear();
  for (auto const p : VPackObjectIterator(pool)) {
    _pool[p.key.copyString()] = p.value.copyString();
  }
  _poolSize = _pool.size();

  auto active = other.get(activeStr);
  TRI_ASSERT(active.isArray());
  _active.clear();
  for (auto const id : VPackArrayIterator(active)) {
    _active.push_back(id.copyString());
  }
  _agencySize = _pool.size();

  if (other.hasKey(minPingStr)) {
    _minPing = other.get(minPingStr).getNumber<double>();
  }
  if (other.hasKey(maxPingStr)) {
    _maxPing = other.get(maxPingStr).getNumber<double>();
  }
  if (other.hasKey(supervisionStr)) {
    _supervision = other.get(supervisionStr).getBoolean();
  }
  if (other.hasKey(supervisionFrequencyStr)) {
    _supervisionFrequency = other.get(supervisionFrequencyStr).getNumber<double>();
  }
  if (other.hasKey(supervisionGracePeriodStr)) {
    _supervisionGracePeriod = other.get(supervisionGracePeriodStr).getNumber<double>();
  }
  if (other.hasKey(supervisionOkThresholdStr)) {
    _supervisionOkThreshold = other.get(supervisionOkThresholdStr).getNumber<double>();
  }
  if (other.hasKey(compactionStepSizeStr)) {
    _compactionStepSize = other.get(compactionStepSizeStr).getNumber<uint64_t>();
  }
  if (other.hasKey(compactionKeepSizeStr)) {
    _compactionKeepSize = other.get(compactionKeepSizeStr).getNumber<uint64_t>();
  }

  ++_version;
}
