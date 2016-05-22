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

#include "Store.h"

#include "StoreCallback.h"
#include "Agency/Agent.h"
#include "Basics/ConditionLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <ctime>
#include <iomanip>

using namespace arangodb::consensus;
using namespace arangodb::basics;

struct NotEmpty {
  bool operator()(const std::string& s) { return !s.empty(); }
};

struct Empty {
  bool operator()(const std::string& s) { return s.empty(); }
};

/// @brief Split strings by separator
inline std::vector<std::string> split(const std::string& value,
                                      char separator) {
  std::vector<std::string> result;
  std::string::size_type p = (value.find(separator) == 0) ? 1 : 0;
  std::string::size_type q;
  while ((q = value.find(separator, p)) != std::string::npos) {
    result.emplace_back(value, p, q - p);
    p = q + 1;
  }
  result.emplace_back(value, p);
  result.erase(std::find_if(result.rbegin(), result.rend(), NotEmpty()).base(),
               result.end());
  return result;
}

inline static bool endpointPathFromUrl(std::string const& url,
                                       std::string& endpoint,
                                       std::string& path) {
  std::stringstream ep;
  path = "/";
  size_t pos = 7;
  if (url.find("http://") == 0) {
    ep << "tcp://";
  } else if (url.find("https://") == 0) {
    ep << "ssl://";
    ++pos;
  } else {
    return false;
  }

  size_t slash_p = url.find("/", pos);
  if (slash_p == std::string::npos) {
    ep << url.substr(pos);
  } else {
    ep << url.substr(pos, slash_p - pos);
    path = url.substr(slash_p);
  }

  if (ep.str().find(':') == std::string::npos) {
    ep << ":8529";
  }

  endpoint = ep.str();

  return true;
}

// Create with name
Store::Store(std::string const& name) : Thread(name), _node(name, this) {}

Store::Store(Store const& other) :
  Thread(other._node.name()), _agent(other._agent), _timeTable(other._timeTable),
  _observerTable(other._observerTable), _observedTable(other._observedTable),
  _node(other._node) {}
  
  Store::Store(Store&& other) :
  Thread(other._node.name()), _agent(std::move(other._agent)),
  _timeTable(std::move(other._timeTable)),
  _observerTable(std::move(other._observerTable)),
  _observedTable(std::move(other._observedTable)),
  _node(std::move(other._node)) {}

Store& Store::operator=(Store const& rhs) {
  _agent = rhs._agent;
  _timeTable = rhs._timeTable;
  _observerTable = rhs._observerTable;
  _observedTable = rhs._observedTable;
  _node = rhs._node;
  return *this;
}

Store& Store::operator=(Store&& rhs) {
  _agent = std::move(rhs._agent);
  _timeTable = std::move(rhs._timeTable);
  _observerTable = std::move(rhs._observerTable);
  _observedTable = std::move(rhs._observedTable);
  _node = std::move(rhs._node);
  return *this;
}

// Default ctor
Store::~Store() {}

// Apply queries multiple queries to store
std::vector<bool> Store::apply(query_t const& query) {
  std::vector<bool> applied;
  MUTEX_LOCKER(storeLocker, _storeLock);
  for (auto const& i : VPackArrayIterator(query->slice())) {
    switch (i.length()) {
      case 1:
        applied.push_back(applies(i[0]));
        break;  // no precond
      case 2:
        if (check(i[1])) {  // precondition
          applied.push_back(applies(i[0]));
        } else {  // precondition failed
          LOG_TOPIC(TRACE, Logger::AGENCY) << "Precondition failed!";
          applied.push_back(false);
        }
        break;
      default:  // wrong
        LOG_TOPIC(ERR, Logger::AGENCY)
            << "We can only handle log entry with or without precondition!";
        applied.push_back(false);
        break;
    }
  }

  _cv.signal();

  return applied;
}

// template<class T, class U> std::multimap<std::string, std::string>
std::ostream& operator<<(std::ostream& os,
                         std::multimap<std::string, std::string> const& m) {
  for (auto const& i : m) {
    os << i.first << ": " << i.second << std::endl;
  }
  return os;
}

// Apply external
struct notify_t {
  std::string key;
  std::string modified;
  std::string oper;
  notify_t(std::string const& k, std::string const& m, std::string const& o)
      : key(k), modified(m), oper(o) {}
};

std::vector<bool> Store::apply(std::vector<VPackSlice> const& queries,
                               bool inform) {
  std::vector<bool> applied;
  {
    MUTEX_LOCKER(storeLocker, _storeLock);
    for (auto const& i : queries) {
      applied.push_back(applies(i));  // no precond
    }
  }

  std::multimap<std::string, std::shared_ptr<notify_t>> in;
  for (auto const& i : queries) {
    for (auto const& j : VPackObjectIterator(i)) {
      if (j.value.isObject() && j.value.hasKey("op")) {
        std::string oper = j.value.get("op").copyString();
        if (!(oper == "observe" || oper == "unobserve")) {
          std::string uri = j.key.copyString();
          while (true) {
            auto ret = _observedTable.equal_range(uri);
            for (auto it = ret.first; it != ret.second; ++it) {
              in.emplace(it->second, std::make_shared<notify_t>(
                                         it->first, j.key.copyString(), oper));
            }
            size_t pos = uri.find_last_of('/');
            if (pos == std::string::npos || pos == 0) {
              break;
            } else {
              uri = uri.substr(0, pos);
            }
          }
        }
      }
    }
  }

  std::vector<std::string> urls;
  for (auto it = in.begin(), end = in.end(); it != end;
       it = in.upper_bound(it->first)) {
    urls.push_back(it->first);
  }

  for (auto const& url : urls) {
    Builder body;  // host
    body.openObject();
    body.add("term", VPackValue(0));
    body.add("index", VPackValue(0));
    auto ret = in.equal_range(url);

    for (auto it = ret.first; it != ret.second; ++it) {
      body.add(it->second->key, VPackValue(VPackValueType::Object));
      body.add(it->second->modified, VPackValue(VPackValueType::Object));
      body.add("op", VPackValue(it->second->oper));
      body.close();
      body.close();
    }

    body.close();

    std::string endpoint, path;
    if (endpointPathFromUrl(url, endpoint, path)) {
      auto headerFields =
          std::make_unique<std::unordered_map<std::string, std::string>>();

      ClusterCommResult res = arangodb::ClusterComm::instance()->asyncRequest(
          "1", 1, endpoint, GeneralRequest::RequestType::POST, path,
          std::make_shared<std::string>(body.toString()), headerFields,
          std::make_shared<StoreCallback>(), 0.0, true);

    } else {
      LOG_TOPIC(WARN, Logger::AGENCY) << "Malformed URL " << url;
    }
  }

  return applied;
}

// Check precondition
bool Store::check(VPackSlice const& slice) const {
  if (!slice.isObject()) {  // Must be object
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "Cannot check precondition: " << slice.toJson();
    return false;
  }

  for (auto const& precond : VPackObjectIterator(slice)) {  // Preconditions
    std::string path = precond.key.copyString();
    bool found = false;
    Node node("precond");

    try {
      node = (*this)(path);
      found = true;
    } catch (StoreException const&) {
    }

    if (precond.value.isObject()) {
      for (auto const& op : VPackObjectIterator(precond.value)) {
        std::string const& oper = op.key.copyString();
        if (oper == "old") {  // old
          if (node != op.value) {
            return false;
          }
        } else if (oper == "isArray") {  // isArray
          if (!op.value.isBoolean()) {
            LOG_TOPIC(ERR, Logger::AGENCY)
                << "Non boolsh expression for 'isArray' precondition";
            return false;
          }
          bool isArray = (node.type() == LEAF && node.slice().isArray());
          if (op.value.getBool() ? !isArray : isArray) {
            return false;
          }
        } else if (oper == "oldEmpty") {  // isEmpty
          if (!op.value.isBoolean()) {
            LOG_TOPIC(ERR, Logger::AGENCY)
                << "Non boolsh expression for 'oldEmpty' precondition";
            return false;
          }
          if (op.value.getBool() ? found : !found) {
            return false;
          }
        }
      }
    } else {
      if (node != precond.value) {
        return false;
      }
    }
  }

  return true;
}

// Read queries into result
std::vector<bool> Store::read(query_t const& queries, query_t& result) const {
  std::vector<bool> success;
  MUTEX_LOCKER(storeLocker, _storeLock);
  if (queries->slice().isArray()) {
    result->add(VPackValue(VPackValueType::Array));  // top node array
    for (auto const& query : VPackArrayIterator(queries->slice())) {
      success.push_back(read(query, *result));
    }
    result->close();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Read queries to stores must be arrays";
  }
  return success;
}

// read single query into ret
bool Store::read(VPackSlice const& query, Builder& ret) const {
  bool success = true;

  // Collect all paths
  std::list<std::string> query_strs;
  if (query.isArray()) {
    for (auto const& sub_query : VPackArrayIterator(query)) {
      query_strs.push_back(sub_query.copyString());
    }
  } else {
    return false;
  }

  // Remove double ranges (inclusion / identity)
  query_strs.sort();  // sort paths
  for (auto i = query_strs.begin(), j = i; i != query_strs.end(); ++i) {
    if (i != j && i->compare(0, j->size(), *j) == 0) {
      *i = "";
    } else {
      j = i;
    }
  }
  auto cut = std::remove_if(query_strs.begin(), query_strs.end(), Empty());
  query_strs.erase(cut, query_strs.end());

  // Create response tree
  Node copy("copy");
  for (auto const path : query_strs) {
    try {
      copy(path) = (*this)(path);
    } catch (StoreException const&) {
      std::vector<std::string> pv = split(path, '/');
      while (!pv.empty()) {
        std::string end = pv.back();
        pv.pop_back();
        copy(pv).removeChild(end);
        try {
          (*this)(pv);
          break;
        } catch (...) {
        }
      }
      if (copy(pv).type() == LEAF && copy(pv).slice().isNone()) {
        copy(pv) = arangodb::basics::VelocyPackHelper::EmptyObjectValue();
      }
    }
  }

  // Into result builder
  copy.toBuilder(ret);

  return success;
}

// Shutdown
void Store::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}

// TTL clear values from store
query_t Store::clearExpired() const {
  query_t tmp = std::make_shared<Builder>();
  tmp->openArray();
  {
    MUTEX_LOCKER(storeLocker, _storeLock);
    for (auto it = _timeTable.cbegin(); it != _timeTable.cend(); ++it) {
      if (it->first < std::chrono::system_clock::now()) {
        tmp->openArray();
        tmp->openObject();
        tmp->add(it->second, VPackValue(VPackValueType::Object));
        tmp->add("op", VPackValue("delete"));
        tmp->close();
        tmp->close();
        tmp->close();
      } else {
        break;
      }
    }
  }
  tmp->close();
  return tmp;
}

// Dump internal data to builder
void Store::dumpToBuilder(Builder& builder) const {
  MUTEX_LOCKER(storeLocker, _storeLock);
  toBuilder(builder);
  {
    VPackObjectBuilder guard(&builder);
    for (auto const& i : _timeTable) {
      auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                    i.first.time_since_epoch()).count();
      builder.add(i.second, VPackValue(ts));
    }
  }
  {
    VPackArrayBuilder garray(&builder);
    for (auto const& i : _observerTable) {
      VPackObjectBuilder guard(&builder);
      builder.add(i.first, VPackValue(i.second));
    }
  }
  {
    VPackArrayBuilder garray(&builder);
    for (auto const& i : _observedTable) {
      VPackObjectBuilder guard(&builder);
      builder.add(i.first, VPackValue(i.second));
    }
  }
}

size_t Store::matchPath(std::vector<std::string> const& pv) const {
  //  Node* cur(this);
  /*  for (size_t i = 0; i < pv.size(); ++i) {
      if (cur.find(pv.at(i))) {
      }
      }*/
  return 0;
}

// Start thread
bool Store::start() {
  Thread::start();
  return true;
}

// Start thread with agent
bool Store::start(Agent* agent) {
  _agent = agent;
  return start();
}

// Work ttls and callbacks
void Store::run() {
  CONDITION_LOCKER(guard, _cv);
  while (!this->isStopping()) {  // Check timetable and remove overage entries

    std::chrono::microseconds t{0};
    query_t toClear;

    {  // any entries in time table?
      MUTEX_LOCKER(storeLocker, _storeLock);
      if (!_timeTable.empty()) {
        t = std::chrono::duration_cast<std::chrono::microseconds>(
            _timeTable.begin()->first - std::chrono::system_clock::now());
      }
    }

    if (t != std::chrono::microseconds{0}) {
      _cv.wait(t.count());
    } else {
      _cv.wait();
    }

    toClear = clearExpired();
    _agent->write(toClear);
  }
}

bool Store::applies(arangodb::velocypack::Slice const& slice) {
  return _node.applies(slice);
}

Store& Store::operator=(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());

  MUTEX_LOCKER(storeLocker, _storeLock);
  _node.applies(slice[0]);

  TRI_ASSERT(slice[1].isObject());
  for (auto const& entry : VPackObjectIterator(slice[1])) {
    long long tse = entry.value.getInt();
    _timeTable.emplace(std::pair<TimePoint, std::string>(
        TimePoint(std::chrono::duration<int>(tse)), entry.key.copyString()));
  }

  TRI_ASSERT(slice[2].isArray());
  for (auto const& entry : VPackArrayIterator(slice[2])) {
    TRI_ASSERT(entry.isObject());
    _observerTable.emplace(std::pair<std::string, std::string>(
        entry.keyAt(0).copyString(), entry.valueAt(0).copyString()));
  }

  TRI_ASSERT(slice[3].isArray());
  for (auto const& entry : VPackArrayIterator(slice[3])) {
    TRI_ASSERT(entry.isObject());
    _observedTable.emplace(std::pair<std::string, std::string>(
        entry.keyAt(0).copyString(), entry.valueAt(0).copyString()));
  }

  return *this;
}

void Store::toBuilder(Builder& b) const { _node.toBuilder(b); }

Node Store::operator()(std::vector<std::string> const& pv) { return _node(pv); }

Node const Store::operator()(std::vector<std::string> const& pv) const {
  return _node(pv);
}

Node Store::operator()(std::string const& path) { return _node(path); }

Node const Store::operator()(std::string const& path) const {
  return _node(path);
}

std::multimap<TimePoint, std::string>& Store::timeTable() { return _timeTable; }

const std::multimap<TimePoint, std::string>& Store::timeTable() const {
  return _timeTable;
}

std::multimap<std::string, std::string>& Store::observerTable() {
  return _observerTable;
}
std::multimap<std::string, std::string> const& Store::observerTable() const {
  return _observerTable;
}

std::multimap<std::string, std::string>& Store::observedTable() {
  return _observedTable;
}

std::multimap<std::string, std::string> const& Store::observedTable() const {
  return _observedTable;
}

Node const Store::get(std::string const& path) const {
  MUTEX_LOCKER(storeLocker, _storeLock);
  return _node(path);
}

void Store::removeTTL(std::string const& uri) {
  if (!_timeTable.empty()) {
    for (auto it = _timeTable.cbegin(); it != _timeTable.cend();) {
      if (it->second == uri) {
        _timeTable.erase(it++);
      } else {
        ++it;
      }
    }
  }
}
