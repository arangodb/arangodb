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
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <ctime>
#include <iomanip>
#include <iostream>

using namespace arangodb::consensus;

inline static bool endpointPathFromUrl (
  std::string const& url, std::string& endpoint, std::string& path) {

  std::stringstream ep;
  path = "/";
  size_t pos = 7;
  if (url.find("http://")==0) {
    ep << "tcp://";
  } else if (url.find("https://")==0) {
    ep << "ssl://";
    ++pos;
  } else {
    return false;
  }
  
  size_t slash_p = url.find("/",pos);
  if (slash_p==std::string::npos) {
    ep << url.substr(pos);
  } else {
    ep << url.substr(pos,slash_p-pos);
    path = url.substr(slash_p);
  }

  if (ep.str().find(':')==std::string::npos) {
    ep << ":8529";
  }

  endpoint = ep.str();

  return true;
    
}

struct NotEmpty {
  bool operator()(const std::string& s) { return !s.empty(); }
};
struct Empty {
  bool operator()(const std::string& s) { return s.empty(); }
};

/// @brief Split strings by separator
std::vector<std::string> split(const std::string& value, char separator) {
  std::vector<std::string> result;
  std::string::size_type p = (value.find(separator) == 0) ? 1:0;
  std::string::size_type q;
  while ((q = value.find(separator, p)) != std::string::npos) {
    result.emplace_back(value, p, q - p);
    p = q + 1;
  }
  result.emplace_back(value, p);
  result.erase(std::find_if(result.rbegin(), result.rend(),
                            NotEmpty()).base(), result.end());
  return result;
}

// Construct with node name
Node::Node (std::string const& name) : _node_name(name), _parent(nullptr) {
  _value.clear();
}

// Construct with node name in tree structure
Node::Node (std::string const& name, Node* parent) :
  _node_name(name), _parent(parent) {
  _value.clear();
}

// Default dtor
Node::~Node() {}

Slice Node::slice() const {
  return (_value.size()==0) ? 
    arangodb::basics::VelocyPackHelper::EmptyObjectValue() :
    Slice(_value.data());
}

// Get name of this node
std::string const& Node::name() const {
  return _node_name;
}

// Get full path of this node
std::string Node::uri() const {
  Node *par = _parent;
  std::stringstream path;
  std::deque<std::string> names;
  names.push_front(name());
  while (par != 0) {
    names.push_front(par->name());
    par = par->_parent;
  }
  for (size_t i = 1; i < names.size(); ++i) {
    path << "/" << names.at(i);
  }
  return path.str();
}

// Assignment of rhs slice
Node& Node::operator= (VPackSlice const& slice) {
  // 1. remove any existing time to live entry
  // 2. clear children map
  // 3. copy from rhs to buffer pointer
  // 4. inform all observers here and above
  // Must not copy _parent, _ttl, _observers
  removeTimeToLive();
  _children.clear();
  _value.reset();
  _value.append(reinterpret_cast<char const*>(slice.begin()), slice.byteSize());
/*
  notifyObservers(uri());
  Node *par = _parent;
  while (par != 0) {
    _parent->notifyObservers(uri());
    par = par->_parent;
  }
*/
  return *this;
}

// Assignment of rhs node
Node& Node::operator= (Node const& rhs) {
  // 1. remove any existing time to live entry
  // 2. clear children map
  // 3. copy from rhs to buffer pointer
  // 4. inform all observers here and above
  // Must not copy rhs's _parent, _ttl, _observers
  removeTimeToLive();
  _node_name = rhs._node_name;
  _value = rhs._value;
  _children = rhs._children;
  /*
  notifyObservers(uri());
  Node *par = _parent;
  while (par != 0) {
    _parent->notifyObservers(uri());
    par = par->_parent;
  }
  */
  return *this;
}

// Comparison with slice
bool Node::operator== (VPackSlice const& rhs) const {
  return rhs.equals(slice());
}

// Remove this node from store
bool Node::remove () {
  Node& parent = *_parent;
  return parent.removeChild(_node_name);
}

// Remove child by name
bool Node::removeChild (std::string const& key) {
  auto found = _children.find(key);
  if (found == _children.end()) {
    return false;
  }
  found->second->removeTimeToLive();
  _children.erase(found);
  return true;
}

// Node type
NodeType Node::type() const {
  return _children.size() ? NODE : LEAF;
}

// lh-value at path vector
Node& Node::operator ()(std::vector<std::string> const& pv) {
  if (pv.size()) {
    std::string const key = pv.at(0);
    if (_children.find(key) == _children.end()) {
      _children[key] = std::make_shared<Node>(key, this);
    }
    auto pvc(pv);
    pvc.erase(pvc.begin());
    return (*_children[key])(pvc);
  } else {
    return *this;
  }
}

// rh-value at path vector
Node const& Node::operator ()(std::vector<std::string> const& pv) const {
  if (pv.size()) {
    std::string const key = pv.at(0);
    if (_children.find(key) == _children.end()) {
      throw StoreException(
        std::string("Node ") + key + std::string(" not found"));
    }
    const Node& child = *_children.at(key);
    auto pvc(pv);
    pvc.erase(pvc.begin());
    return child(pvc);
  } else {
    return *this;
  }
}

// lh-value at path
Node& Node::operator ()(std::string const& path) {
  PathType pv = split(path,'/');
  return this->operator()(pv);
}

// rh-value at path
Node const& Node::operator ()(std::string const& path) const {
  PathType pv = split(path,'/');
  return this->operator()(pv);
}

// lh-store 
Node const& Node::root() const {
  Node *par = _parent, *tmp = 0;
  while (par != 0) {
    tmp = par;
    par = par->_parent;
  }
  return *tmp;
}

// rh-store
Node& Node::root() {
  Node *par = _parent, *tmp = 0;
  while (par != 0) {
    tmp = par;
    par = par->_parent;
  }
  return *tmp;
}

// velocypack value type of this node
ValueType Node::valueType() const {
  return slice().type();
}

// file time to live entry for this node to now + millis
bool Node::addTimeToLive (long millis) {
  auto tkey = std::chrono::system_clock::now() +
    std::chrono::milliseconds(millis);
  root()._time_table.insert(
    std::pair<TimePoint,std::shared_ptr<Node>>(
      tkey, _parent->_children[_node_name]));
  _ttl = tkey;
  return true;
}

// remove time to live entry for this node
bool Node::removeTimeToLive () {
  if (_ttl != std::chrono::system_clock::time_point()) {
    auto ret = root()._time_table.equal_range(_ttl);
    for (auto it = ret.first; it!=ret.second;) {
      if (it->second == _parent->_children[_node_name]) {
        root()._time_table.erase(it);
        break;
        ++it;
      }
    }
  }
  return true;
}

// Add observing url for this node
/*bool Node::addObserver (std::string const& uri) {
  auto it = std::find(_observers.begin(), _observers.end(), uri);
  if (it==_observers.end()) {
    _observers.emplace(uri);
    return true;
  }
  return false;
  }*/

/*void Node::notifyObservers (std::string const& origin) const {

  for (auto const& i : _observers) {

    Builder body;
    body.openObject();
    body.add(uri(), VPackValue(VPackValueType::Object));
    body.add("op",VPackValue("modified"));
    body.close();
    body.close();
    
    std::stringstream endpoint;
    std::string path = "/";
    size_t pos = 7;
    if (i.find("http://")==0) {
      endpoint << "tcp://";
    } else if (i.find("https://")==0) {
      endpoint << "ssl://";
      ++pos;
    } else {
      LOG_TOPIC(WARN,Logger::AGENCY) << "Malformed notification URL " << i;
      return;
    }

    size_t slash_p = i.find("/",pos);
    if ((slash_p==std::string::npos)) {
      endpoint << i.substr(pos);
    } else {
      endpoint << i.substr(pos,slash_p-pos);
      path = i.substr(slash_p);
    }

    std::unique_ptr<std::map<std::string, std::string>> headerFields =
      std::make_unique<std::map<std::string, std::string> >();
    
    ClusterCommResult res =
      arangodb::ClusterComm::instance()->asyncRequest(
        "1", 1, endpoint.str(), GeneralRequest::RequestType::POST, path,
        std::make_shared<std::string>(body.toString()), headerFields, nullptr,
        0.0, true);

  }

  }*/

inline bool Node::observedBy (std::string const& url) const {
  auto ret = root()._observer_table.equal_range(url);
  for (auto it = ret.first; it!=ret.second; ++it) {
    if (it->second == uri()) {
      return true;
    }
  }
  return false;
}

namespace arangodb {
namespace consensus {
template<> bool Node::handle<SET> (VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "Operator set without new value";
    LOG_TOPIC(WARN, Logger::AGENCY) << slice.toJson();
    return false;
  }
  *this = slice.get("new");
  if (slice.hasKey("ttl")) {
    VPackSlice ttl_v = slice.get("ttl");
    if (ttl_v.isNumber()) {
      long ttl = 1000l * (
        (ttl_v.isDouble()) ?
        static_cast<long>(slice.get("ttl").getDouble()):
        slice.get("ttl").getInt());
      addTimeToLive (ttl);
    } else {
      LOG_TOPIC(WARN, Logger::AGENCY) <<
        "Non-number value assigned to ttl: " << ttl_v.toJson();
    }
  }
  return true;
}

template<> bool Node::handle<INCREMENT> (VPackSlice const& slice) {
  Builder tmp;
  tmp.openObject();
  try {
    tmp.add("tmp", Value(this->slice().getInt()+1));
  } catch (std::exception const&) {
    tmp.add("tmp",Value(1));
  }
  tmp.close();
  *this = tmp.slice().get("tmp");
  return true;
}

template<> bool Node::handle<DECREMENT> (VPackSlice const& slice) {
  Builder tmp;
  tmp.openObject();
  try {
    tmp.add("tmp", Value(this->slice().getInt()-1));
  } catch (std::exception const&) {
    tmp.add("tmp",Value(-1));
  }
  tmp.close();
  *this = tmp.slice().get("tmp");
  return true;
}

template<> bool Node::handle<PUSH> (VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Operator push without new value: " << slice.toJson();
    return false;
  }
  Builder tmp;
  tmp.openArray();
  if (this->slice().isArray()) {
    for (auto const& old : VPackArrayIterator(this->slice()))
      tmp.add(old);
  }
  tmp.add(slice.get("new"));
  tmp.close();
  *this = tmp.slice();
  return true;
}

template<> bool Node::handle<POP> (VPackSlice const& slice) {
  Builder tmp;
  tmp.openArray();
  if (this->slice().isArray()) {
    VPackArrayIterator it(this->slice());
    if (it.size()>1) {
      size_t j = it.size()-1;
      for (auto old : it) {
        tmp.add(old);
        if (--j==0)
          break;
      }
    }
  }
  tmp.close();
  *this = tmp.slice();
  return true;
}

template<> bool Node::handle<PREPEND> (VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Operator prepend without new value: " << slice.toJson();
    return false;
  }
  Builder tmp;
  tmp.openArray();
  tmp.add(slice.get("new"));
  if (this->slice().isArray()) {
    for (auto const& old : VPackArrayIterator(this->slice()))
      tmp.add(old);
  }
  tmp.close();
  *this = tmp.slice();
  return true;
}

template<> bool Node::handle<SHIFT> (VPackSlice const& slice) {
  Builder tmp;
  tmp.openArray();
  if (this->slice().isArray()) { // If a
    VPackArrayIterator it(this->slice());
    bool first = true;
    for (auto old : it) {
      if (first) {
        first = false;
      } else {
        tmp.add(old);
      }
    }
  } 
  tmp.close();
  *this = tmp.slice();
  return true;
}

/// Add observer for this node
template<> bool Node::handle<OBSERVE> (VPackSlice const& slice) {

  if (!slice.hasKey("url"))
    return false;
  if (!slice.get("url").isString())
    return false;
  std::string url (slice.get("url").copyString()),
    uri (this->uri());
  
  // check if such entry exists
  if (!observedBy(url)) {
    root()._observer_table.emplace(std::pair<std::string,std::string>(url,uri));
    root()._observed_table.emplace(std::pair<std::string,std::string>(uri,url));
//    _observers.emplace(url);
    return true;
  }
  
  return false;

}

template<> bool Node::handle<UNOBSERVE> (VPackSlice const& slice) {

  if (!slice.hasKey("url"))
    return false;
  if (!slice.get("url").isString())
    return false;
  std::string url (slice.get("url").copyString()),
    uri (this->uri());
  
  auto ret = root()._observer_table.equal_range(url);
  for (auto it = ret.first; it!=ret.second; ++it) {
    if (it->second == uri) {
      root()._observer_table.erase(it);
      break;
    }
  }
  ret = root()._observed_table.equal_range(uri);
  for (auto it = ret.first; it!=ret.second; ++it) {
    if (it->second == url) {
      root()._observed_table.erase(it);
      return true;
    }
  }

  return false;
  
}

}}

// Apply slice to this node
bool Node::applies (VPackSlice const& slice) {
  if (slice.isObject()) {
    for (auto const& i : VPackObjectIterator(slice)) {

      std::string key = i.key.copyString();
      
      if (slice.hasKey("op")) {
        std::string oper = slice.get("op").copyString();
        if (oper == "delete") {
          return _parent->removeChild(_node_name);
        } else if (oper == "set") { //
          return handle<SET>(slice);
        } else if (oper == "increment") { // Increment
          return handle<INCREMENT>(slice);
        } else if (oper == "decrement") { // Decrement
          return handle<DECREMENT>(slice);
        } else if (oper == "push") { // Push
          return handle<PUSH>(slice);
        } else if (oper == "pop") { // Pop
          return handle<POP>(slice);
        } else if (oper == "prepend") { // Prepend
          return handle<PREPEND>(slice);
        } else if (oper == "shift") { // Shift
          return handle<SHIFT>(slice);
        } else if (oper == "observe") {
          return handle<OBSERVE>(slice);
        } else if (oper == "unobserve") {
          return handle<UNOBSERVE>(slice);
        } else {
          LOG_TOPIC(WARN, Logger::AGENCY) << "Unknown operation " << oper;
          return false;
        }
      } else if (slice.hasKey("new")) { // new without set
        *this = slice.get("new");
        return true;
      } else if (key.find('/')!=std::string::npos) {
        (*this)(key).applies(i.value);
      } else {
        auto found = _children.find(key);
        if (found == _children.end()) {
          _children[key] = std::make_shared<Node>(key, this);
        }
      _children[key]->applies(i.value);
      }
    }
  } else {
    *this = slice;
  }
  return true;
}

void Node::toBuilder (Builder& builder) const {
  try {
    if (type()==NODE) {
      VPackObjectBuilder guard(&builder);
      for (auto const& child : _children) {
        builder.add(VPackValue(child.first));
        child.second->toBuilder(builder);
      }
    } else {
      if (!slice().isNone()) {
        builder.add(slice());
      }
    }
    
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what();
  }
  
}

// Print internals to ostream
std::ostream& Node::print (std::ostream& o) const {
  Node const* par = _parent;
  while (par != 0) {
      par = par->_parent;
      o << "  ";
  }

  o << _node_name << " : ";

  if (type() == NODE) {
    o << std::endl;
    for (auto const& i : _children)
      o << *(i.second);
  } else {
    o << ((slice().isNone()) ? "NONE" : slice().toJson());
    if (_ttl != std::chrono::system_clock::time_point()) {
      o << " ttl! ";
    }
    o << std::endl;
  }

  if (!_time_table.empty()) {
    for (auto const& i : _time_table) {
      o << i.second.get() << std::endl;
    }
  }

  return o;
}

// Create with name
Store::Store (std::string const& name) : Node(name), Thread(name) {}

// Default ctor
Store::~Store () {}

// Apply queries multiple queries to store
std::vector<bool> Store::apply (query_t const& query) {    
  std::vector<bool> applied;
  MUTEX_LOCKER(storeLocker, _storeLock);
  for (auto const& i : VPackArrayIterator(query->slice())) {
    switch (i.length()) {
    case 1:
      applied.push_back(applies(i[0])); break; // no precond
    case 2:
      if (check(i[1])) {
        applied.push_back(applies(i[0]));      // precondition
      } else {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Precondition failed!";
        applied.push_back(false);
      }
      break;
    default:                                   // wrong
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "We can only handle log entry with or without precondition!";
      applied.push_back(false);
      break;
    }
  }

  _cv.signal();

  return applied;
}

//template<class T, class U> std::multimap<std::string, std::string>
std::ostream& operator<< (
  std::ostream& os, std::multimap<std::string,std::string> const& m) {
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
    notify_t (std::string const& k, std::string const& m, std::string const& o) :
      key(k), modified(m), oper(o) {}
  };

std::vector<bool> Store::apply (
  std::vector<VPackSlice> const& queries, bool inform) {
  std::vector<bool> applied;
  {
    MUTEX_LOCKER(storeLocker, _storeLock);
    for (auto const& i : queries) {
      applied.push_back(applies(i)); // no precond
    }
  }

  std::multimap<std::string,std::shared_ptr<notify_t>> in;
  for (auto const& i : queries) {
    for (auto const& j : VPackObjectIterator(i)) {
      if (j.value.isObject() && j.value.hasKey("op")) {
        std::string oper = j.value.get("op").copyString();
        if (!(oper == "observe" || oper == "unobserve")) {
          std::string uri = j.key.copyString();
          while (true) {
            auto ret = _observed_table.equal_range(uri);
            for (auto it = ret.first; it!=ret.second; ++it) {
              in.emplace (
                it->second, std::make_shared<notify_t>(
                  it->first, j.key.copyString(), oper));
            }
            size_t pos = uri.find_last_of('/');
            if (pos == std::string::npos || pos == 0) {
              break;
            } else {
              uri = uri.substr(0,pos);
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

    Builder body; // host
    body.openObject();
    body.add("term",VPackValue(0));
    body.add("index",VPackValue(0));
    auto ret = in.equal_range(url);
    
    for (auto it = ret.first; it!=ret.second; ++it) {
      body.add(it->second->key,VPackValue(VPackValueType::Object));
      body.add("op",VPackValue(it->second->oper));
      body.close();
    }
    
    body.close();

    std::string endpoint, path;
    if (endpointPathFromUrl (url,endpoint,path)) {

      std::unique_ptr<std::map<std::string, std::string>> headerFields =
        std::make_unique<std::map<std::string, std::string> >();
      
      ClusterCommResult res =
        arangodb::ClusterComm::instance()->asyncRequest(
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
bool Store::check (VPackSlice const& slice) const {
  if (!slice.isObject()) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Cannot check precondition: " << slice.toJson();
    return false;
  }
  for (auto const& precond : VPackObjectIterator(slice)) {
    std::string path = precond.key.copyString();
    bool found = false;
    Node node ("precond");
    
    try {
      node = (*this)(path);
      found = true;
    } catch (StoreException const&) {}
    
    if (precond.value.isObject()) {
      for (auto const& op : VPackObjectIterator(precond.value)) {
        std::string const& oper = op.key.copyString();
        if (oper == "old") {                           // old
          return (node == op.value);
        } else if (oper == "isArray") {                // isArray
          if (!op.value.isBoolean()) {
            LOG (FATAL) << "Non boolsh expression for 'isArray' precondition";
            return false;
          }
          bool isArray =
            (node.type() == LEAF && node.slice().isArray());
          return op.value.getBool() ? isArray : !isArray;
        } else if (oper == "oldEmpty") {              // isEmpty
          if (!op.value.isBoolean()) {
            LOG (FATAL) << "Non boolsh expression for 'oldEmpty' precondition";
            return false;
          }
          return op.value.getBool() ? !found : found;
        }
      }
    } else {
      return node == precond.value;
    }
  }
  
  return true;
}

// Read queries into result
std::vector<bool> Store::read (query_t const& queries, query_t& result) const { 
  std::vector<bool> success;
  MUTEX_LOCKER(storeLocker, _storeLock);
  if (queries->slice().isArray()) {
    result->add(VPackValue(VPackValueType::Array)); // top node array
    for (auto const& query : VPackArrayIterator(queries->slice())) {
      success.push_back(read (query, *result));
    } 
    result->close();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Read queries to stores must be arrays";
  }
  return success;
}

// read single query into ret
bool Store::read (VPackSlice const& query, Builder& ret) const {
  
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
  query_strs.sort();     // sort paths
  for (auto i = query_strs.begin(), j = i; i != query_strs.end(); ++i) {
    if (i!=j && i->compare(0,j->size(),*j)==0) {
      *i="";
    } else {
      j = i;
    }
  }
  auto cut = std::remove_if(query_strs.begin(), query_strs.end(), Empty());
  query_strs.erase (cut,query_strs.end());
  
  // Create response tree 
  Node copy("copy");
  for (auto const path :  query_strs) {
    try {
      copy(path) = (*this)(path);
    } catch (StoreException const&) {
      std::vector<std::string> pv = split(path,'/');
      while (!pv.empty()) {
        std::string end = pv.back();
        pv.pop_back();
        copy(pv).removeChild(end);
        try {
          (*this)(pv);
          break;
        } catch(...) {}
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
query_t Store::clearExpired () const {
  query_t tmp = std::make_shared<Builder>();
  tmp->openArray(); 
  for (auto it = _time_table.cbegin(); it != _time_table.cend(); ++it) {
    if (it->first < std::chrono::system_clock::now()) {
      tmp->openArray(); tmp->openObject();
      tmp->add(it->second->uri(), VPackValue(VPackValueType::Object));
      tmp->add("op",VPackValue("delete"));
      tmp->close(); tmp->close(); tmp->close();
    } else {
      break;
    }
  }
  tmp->close();
  return tmp;
}

// Dump internal data to builder
void Store::dumpToBuilder (Builder& builder) const {
  MUTEX_LOCKER(storeLocker, _storeLock);
  toBuilder(builder);
  {
    VPackObjectBuilder guard(&builder);
    for (auto const& i : _time_table) {
      auto in_time_t = std::chrono::system_clock::to_time_t(i.first);
      std::string ts = ctime(&in_time_t);
      ts.resize(ts.size()-1);
      builder.add(ts, VPackValue((size_t)i.second.get()));
    }
  }
  {
    VPackObjectBuilder guard(&builder);
    for (auto const& i : _observer_table) {
      builder.add(i.first, VPackValue(i.second));
    }
  }
  {
    VPackObjectBuilder guard(&builder);
    for (auto const& i : _observed_table) {
      builder.add(i.first, VPackValue(i.second));
    }
  }
}

// Start thread
bool Store::start () {
  Thread::start();
  return true;
}

// Start thread with agent
bool Store::start (Agent* agent) {
  _agent = agent;
  return start();
}

// Work ttls and callbacks
void Store::run() {
  CONDITION_LOCKER(guard, _cv);
  while (!this->isStopping()) { // Check timetable and remove overage entries
    if (!_time_table.empty()) {
      auto t = std::chrono::duration_cast<std::chrono::microseconds>(
        _time_table.begin()->first - std::chrono::system_clock::now());
      _cv.wait(t.count());
    } else {
      _cv.wait();           // better wait to next known time point
    }
    auto toclear = clearExpired();
    _agent->write(toclear);
  }
}
