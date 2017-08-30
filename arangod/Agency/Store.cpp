
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

#include "Agency/Agent.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "StoreCallback.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <ctime>
#include <iomanip>
#include <regex>

using namespace arangodb::consensus;
using namespace arangodb::basics;

/// Non-Emptyness of string
struct NotEmpty {
  bool operator()(const std::string& s) { return !s.empty(); }
};

/// Emptyness of string
struct Empty {
  bool operator()(const std::string& s) { return s.empty(); }
};

/// @brief Split strings by separator
inline static std::vector<std::string> split(const std::string& str,
                                             char separator) {

  std::vector<std::string> result;
  if (str.empty()) {
    return result;
  }
  std::regex reg("/+");
  std::string key = std::regex_replace(str, reg, "/");

  if (!key.empty() && key.front() == '/') { key.erase(0,1); }
  if (!key.empty() && key.back()  == '/') { key.pop_back(); }
  
  std::string::size_type p = 0;
  std::string::size_type q;
  while ((q = key.find(separator, p)) != std::string::npos) {
    result.emplace_back(key, p, q - p);
    p = q + 1;
  }
  result.emplace_back(key, p);
  result.erase(std::find_if(result.rbegin(), result.rend(), NotEmpty()).base(),
               result.end());
  return result;
}

/// Build endpoint from URL
inline static bool endpointPathFromUrl(std::string const& url,
                                       std::string& endpoint,
                                       std::string& path) {
  std::stringstream ep;
  path = "/";
  size_t pos = 7;

  if (url.compare(0, pos, "http://") == 0) {
    ep << "tcp://";
  } else if (url.compare(0, ++pos, "https://") == 0) {
    ep << "ssl://";
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

/// Ctor with name
Store::Store(Agent* agent, std::string const& name)
  : Thread(name), _agent(agent), _node(name, this) {}

/// Copy assignment operator
Store& Store::operator=(Store const& rhs) {
  if (&rhs != this) {
    MUTEX_LOCKER(otherLock, rhs._storeLock); 
    _agent = rhs._agent;
    _timeTable = rhs._timeTable;
    _observerTable = rhs._observerTable;
    _observedTable = rhs._observedTable;
    _node = rhs._node;
  }
  return *this;
}

/// Move assignment operator
Store& Store::operator=(Store&& rhs) {
  if (&rhs != this) {
    MUTEX_LOCKER(otherLock, rhs._storeLock); 
    _agent = std::move(rhs._agent);
    _timeTable = std::move(rhs._timeTable);
    _observerTable = std::move(rhs._observerTable);
    _observedTable = std::move(rhs._observedTable);
    _node = std::move(rhs._node);
  }
  return *this;
}

/// Default dtor
Store::~Store() {
  if (!isStopping()) {
    shutdown();
  }
}

/// Apply array of transactions multiple queries to store
/// Return vector of according success
std::vector<bool> Store::applyTransactions(query_t const& query) {
  std::vector<bool> success;

  if (query->slice().isArray()) {
    try {
      for (auto const& i : VPackArrayIterator(query->slice())) {
        MUTEX_LOCKER(storeLocker, _storeLock);
        switch (i.length()) {
        case 1:  // No precondition
          success.push_back(applies(i[0]));
          break;
        case 2: // precondition + uuid
        case 3:
          if (check(i[1]).successful()) {
            success.push_back(applies(i[0]));
          } else {  // precondition failed
            LOG_TOPIC(TRACE, Logger::AGENCY) << "Precondition failed!";
            success.push_back(false);
          }
          break;
        default:  // Wrong
          LOG_TOPIC(ERR, Logger::AGENCY)
            << "We can only handle log entry with or without precondition!";
          success.push_back(false);
          break;
        }
      }

      // Wake up TTL processing
      {
        CONDITION_LOCKER(guard, _cv);
        _cv.signal();
      }

    } catch (std::exception const& e) {  // Catch any erorrs
      LOG_TOPIC(ERR, Logger::AGENCY) << __FILE__ << ":" << __LINE__ << " "
                                     << e.what();
    }

  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(30000,
                                   "Agency request syntax is [[<queries>]]");
  }
  return success;
}


/// Apply single transaction
check_ret_t Store::applyTransaction(Slice const& query) {

  check_ret_t ret(true);

  try {
    MUTEX_LOCKER(storeLocker, _storeLock);
    switch (query.length()) {
    case 1:  // No precondition
      applies(query[0]);
      break;
    case 2:  // precondition
    case 3:  // precondition + clientId
      ret = check(query[1], CheckMode::FULL);
      if (ret.successful()) {
        applies(query[0]);
      } else {  // precondition failed
        LOG_TOPIC(TRACE, Logger::AGENCY) << "Precondition failed!";
      }
      break;
    default:  // Wrong
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "We can only handle log entry with or without precondition!";
      break;
    }  
    // Wake up TTL processing
    {
      CONDITION_LOCKER(guard, _cv);
      _cv.signal();
    }
      
  } catch (std::exception const& e) {  // Catch any erorrs
    LOG_TOPIC(ERR, Logger::AGENCY)
      << __FILE__ << ":" << __LINE__ << " " << e.what();
  }
    
  return ret;
  
}


/// template<class T, class U> std::multimap<std::string, std::string>
std::ostream& operator<<(std::ostream& os,
                         std::unordered_multimap<std::string, std::string> const& m) {
  for (auto const& i : m) {
    os << i.first << ": " << i.second << std::endl;
  }

  return os;
}


/// Notification type
struct notify_t {
  std::string key;
  std::string modified;
  std::string oper;
  notify_t(std::string const& k, std::string const& m, std::string const& o)
      : key(k), modified(m), oper(o) {}
};


/// Apply (from logs)
std::vector<bool> Store::applyLogEntries(
  arangodb::velocypack::Builder const& queries, index_t index,
  term_t term, bool inform) {
  std::vector<bool> applied;

  // Apply log entries
  {
    VPackArrayIterator queriesIterator(queries.slice());

    MUTEX_LOCKER(storeLocker, _storeLock);

    while (queriesIterator.valid()) {
      applied.push_back(applies(queriesIterator.value()));
      queriesIterator.next();
    }
  }

  if (inform && _agent->leading()) {
    // Find possibly affected callbacks
    std::multimap<std::string, std::shared_ptr<notify_t>> in;
    VPackArrayIterator queriesIterator(queries.slice());

    while (queriesIterator.valid()) {
      VPackSlice const& i = queriesIterator.value();

      for (auto const& j : VPackObjectIterator(i)) {
        if (j.value.isObject() && j.value.hasKey("op")) {
          std::string oper = j.value.get("op").copyString();
          if (!(oper == "observe" || oper == "unobserve")) {
            std::string uri = j.key.copyString();
            if (!uri.empty() && uri.at(0)!='/') {
              uri = std::string("/") + uri;
            }
            while (true) {
              // TODO: Check if not a special lock will help
              {
                MUTEX_LOCKER(storeLocker, _storeLock) ;
                auto ret = _observedTable.equal_range(uri);
                for (auto it = ret.first; it != ret.second; ++it) {
                  in.emplace(it->second, std::make_shared<notify_t>(
                               it->first, j.key.copyString(), oper));
                }
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

      queriesIterator.next();
    }
    
    // Sort by URLS to avoid multiple callbacks
    std::vector<std::string> urls;
    for (auto it = in.begin(), end = in.end(); it != end;
         it = in.upper_bound(it->first)) {
      urls.push_back(it->first);
    }
    
    // Callback

    for (auto const& url : urls) {
      Builder body;  // host
      { VPackObjectBuilder b(&body);
        body.add("term", VPackValue(term));
        body.add("index", VPackValue(index));
        auto ret = in.equal_range(url);
        std::string currentKey;
        for (auto it = ret.first; it != ret.second; ++it) {
          if (currentKey != it->second->key) {
            if (!currentKey.empty()) {
              body.close();
            }
            body.add(it->second->key, VPackValue(VPackValueType::Object));
            currentKey = it->second->key;
          }
          body.add(VPackValue(it->second->modified));
          { VPackObjectBuilder b(&body);
            body.add("op", VPackValue(it->second->oper)); }
        }
        if (!currentKey.empty()) {
          body.close();
        }
      }
      
      std::string endpoint, path;
      if (endpointPathFromUrl(url, endpoint, path)) {
        auto headerFields =
          std::make_unique<std::unordered_map<std::string, std::string>>();
        
        arangodb::ClusterComm::instance()->asyncRequest(
          "1", 1, endpoint, rest::RequestType::POST, path,
          std::make_shared<std::string>(body.toString()), headerFields,
          std::make_shared<StoreCallback>(path, body.toJson()), 1.0, true, 0.01);
        
      } else {
        LOG_TOPIC(WARN, Logger::AGENCY) << "Malformed URL " << url;
      }
    }
  }
  
  return applied;
}

/// Check precodition object
check_ret_t Store::check(VPackSlice const& slice, CheckMode mode) const {

  TRI_ASSERT(slice.isObject());
  check_ret_t ret;
  ret.open();

  _storeLock.assertLockedByCurrentThread();

  for (auto const& precond : VPackObjectIterator(slice)) {  // Preconditions

    std::string key = precond.key.copyString();
    std::vector<std::string> pv = split(key, '/');
    
    Node node("precond");

    // Check is guarded in ::apply
    bool found = (_node.exists(pv).size() == pv.size());
    if (found) {
      node = _node(pv);
    }

    if (precond.value.isObject()) {
      for (auto const& op : VPackObjectIterator(precond.value)) {
        std::string const& oper = op.key.copyString();
        if (oper == "old") {  // old
          if (node != op.value) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == "isArray") {  // isArray
          if (!op.value.isBoolean()) {
            LOG_TOPIC(ERR, Logger::AGENCY)
              << "Non boolean expression for 'isArray' precondition";
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
          bool isArray = (node.type() == LEAF && node.slice().isArray());
          if (op.value.getBool() ? !isArray : isArray) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == "oldEmpty") {  // isEmpty
          if (!op.value.isBoolean()) {
            LOG_TOPIC(ERR, Logger::AGENCY)
                << "Non boolsh expression for 'oldEmpty' precondition";
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
          if (op.value.getBool() ? found : !found) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == "in") {  // in
          if (found) {
            if (node.slice().isArray()) {
              bool _found = false;
              for (auto const& i : VPackArrayIterator(node.slice())) {
                if (i == op.value) {
                  _found = true;
                  continue;
                }
              }
              if (_found) {
                continue;
              } else {
                ret.push_back(precond.key);
              }
            }
          } 
          ret.push_back(precond.key);
          if (mode == FIRST_FAIL) {
            break;
          }
        } else if (oper == "notin") {  // in
          if (!found) {
            continue;
          } else {
            if (node.slice().isArray()) {
              bool _found = false;
              for (auto const& i : VPackArrayIterator(node.slice())) {
                if (i == op.value) {
                  _found = true;
                  continue;
                }
              }
              if (_found) {
                ret.push_back(precond.key);
              } else {
                continue;
              }
            }
          } 
          ret.push_back(precond.key);
          if (mode == FIRST_FAIL) {
            break;
          }
        }
      } 
    } else {
      if (node != precond.value) {
        ret.push_back(precond.key);
        if (mode == FIRST_FAIL) {
          break;
        }
      }
    }
  }

  ret.close();
  return ret;
}


/// Read queries into result
std::vector<bool> Store::read(query_t const& queries, query_t& result) const {
  std::vector<bool> success;
  if (queries->slice().isArray()) {
    VPackArrayBuilder r(result.get());
    for (auto const& query : VPackArrayIterator(queries->slice())) {
      success.push_back(read(query, *result));
    }
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Read queries to stores must be arrays";
  }
  return success;
}

/// Read single query into ret
bool Store::read(VPackSlice const& query, Builder& ret) const {
  bool success = true;
  bool showHidden = false;

  // Collect all paths
  std::list<std::string> query_strs;
  if (query.isArray()) {
    for (auto const& sub_query : VPackArrayIterator(query)) {
      std::string subqstr = sub_query.copyString();
      query_strs.push_back(subqstr);
      showHidden |= (subqstr.find("/.") != std::string::npos);
    }
  } else {
    return false;
  }

  // Remove double ranges (inclusion / identity)
  query_strs.sort();  // sort paths
  for (auto i = query_strs.begin(), j = i; i != query_strs.end(); ++i) {
    if (i != j && i->compare(0, i->size(), *j) == 0) {
      *i = "";
    } else {
      j = i;
    }
  }
  auto cut = std::remove_if(query_strs.begin(), query_strs.end(), Empty());
  query_strs.erase(cut, query_strs.end());

  // Create response tree
  Node copy("copy");
  MUTEX_LOCKER(storeLocker, _storeLock); // Freeze KV-Store for read
  for (auto const path : query_strs) {
    std::vector<std::string> pv = split(path, '/');
    size_t e = _node.exists(pv).size();
    if (e == pv.size()) {  // existing
      copy(pv) = _node(pv);
    } else {  // non-existing
      for (size_t i = 0; i < pv.size() - e + 1; ++i) {
        pv.pop_back();
      }
      if (copy(pv).type() == LEAF && copy(pv).slice().isNone()) {
        copy(pv) = arangodb::basics::VelocyPackHelper::EmptyObjectValue();
      }
    }
  }

  // Into result builder
  copy.toBuilder(ret, showHidden);

  return success;
}

/// Shutdown
void Store::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}

/// TTL clear values from store
query_t Store::clearExpired() const {

  query_t tmp = std::make_shared<Builder>();
  { VPackArrayBuilder t(tmp.get());
    MUTEX_LOCKER(storeLocker, _storeLock);
    if (!_timeTable.empty()) {
      for (auto it = _timeTable.cbegin(); it != _timeTable.cend(); ++it) {
        if (it->first < std::chrono::system_clock::now()) {
          VPackArrayBuilder ttt(tmp.get());
          { VPackObjectBuilder tttt(tmp.get());
            tmp->add(VPackValue(it->second));
            { VPackObjectBuilder ttttt(tmp.get());
              tmp->add("op", VPackValue("delete"));
            }}
        } else {
          break;
        }
      }
    }
  }
  return tmp;
}

/// Dump internal data to builder
void Store::dumpToBuilder(Builder& builder) const {
  MUTEX_LOCKER(storeLocker, _storeLock);
  toBuilder(builder, true);
  {
    VPackObjectBuilder guard(&builder);
    for (auto const& i : _timeTable) {
      auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                    i.first.time_since_epoch())
                    .count();
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

/// Start thread
bool Store::start() {
  Thread::start();
  return true;
}

/// Work ttls and callbacks
void Store::run() {
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

    {
      CONDITION_LOCKER(guard, _cv);
      if (t != std::chrono::microseconds{0}) {
        _cv.wait(t.count());
      } else {
        _cv.wait();
      }
    }

    toClear = clearExpired();
    if (_agent && _agent->leading()) {
      //_agent->write(toClear);
    }
  }
}

/// Apply transaction to key value store. Guarded by caller 
bool Store::applies(arangodb::velocypack::Slice const& transaction) {
  std::vector<std::string> keys;
  std::vector<std::string> abskeys;
  std::vector<size_t> idx;
  std::regex reg("/+");
  size_t counter = 0;

  for (const auto& atom : VPackObjectIterator(transaction)) {
    std::string key(atom.key.copyString());
    keys.push_back(key);
    key = std::regex_replace(key, reg, "/");
    abskeys.push_back(((key[0] == '/') ? key : std::string("/") + key));
    idx.push_back(counter++);
  }

  sort(idx.begin(), idx.end(),
       [&abskeys](size_t i1, size_t i2) { return abskeys[i1] < abskeys[i2]; });
  
  _storeLock.assertLockedByCurrentThread();

  for (const auto& i : idx) {
    std::string const& key = keys.at(i);
    Slice value = transaction.get(key);

    if (value.isObject() && value.hasKey("op")) {
      _node(abskeys.at(i)).applieOp(value);
    } else {
      _node(abskeys.at(i)).applies(value);
    }
  }

  return true;
}


// Clear my data
void Store::clear() {
  MUTEX_LOCKER(storeLocker, _storeLock);
  _timeTable.clear();
  _observerTable.clear();
  _observedTable.clear();
  _node.clear();
}


/// Apply a request to my key value store
Store& Store::operator=(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());
  TRI_ASSERT(slice.length() == 4);

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

/// Put key value store in velocypack, guarded by caller
void Store::toBuilder(Builder& b, bool showHidden) const {
  _storeLock.assertLockedByCurrentThread();
  _node.toBuilder(b, showHidden); }

/// Time table
std::multimap<TimePoint, std::string>& Store::timeTable() { 
  _storeLock.assertLockedByCurrentThread();
  return _timeTable; 
}

/// Time table
std::multimap<TimePoint, std::string> const& Store::timeTable() const {
  _storeLock.assertLockedByCurrentThread();
  return _timeTable;
}

/// Observer table
std::unordered_multimap<std::string, std::string>& Store::observerTable() {
  _storeLock.assertLockedByCurrentThread();
  return _observerTable;
}

/// Observer table
std::unordered_multimap<std::string, std::string> const& Store::observerTable() const {
  _storeLock.assertLockedByCurrentThread();
  return _observerTable;
}

/// Observed table
std::unordered_multimap<std::string, std::string>& Store::observedTable() {
  _storeLock.assertLockedByCurrentThread();
  return _observedTable;
}

/// Observed table
std::unordered_multimap<std::string, std::string> const& Store::observedTable() const {
  _storeLock.assertLockedByCurrentThread();
  return _observedTable;
}

/// Get node at path under mutex
Node Store::get(std::string const& path) const {
  MUTEX_LOCKER(storeLocker, _storeLock);
  return _node(path);
}

/// Get node at path under mutex
bool Store::has(std::string const& path) const {
  MUTEX_LOCKER(storeLocker, _storeLock);
  return _node.has(path);
}

/// Remove ttl entry for path, guarded by caller
void Store::removeTTL(std::string const& uri) {
  _storeLock.assertLockedByCurrentThread();

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

