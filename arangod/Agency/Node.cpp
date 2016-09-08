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

#include "Node.h"
#include "Store.h"

#include "Basics/StringUtils.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <deque>
#include <regex>

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

/// Construct with node name
Node::Node(std::string const& name)
    : _node_name(name),
      _parent(nullptr),
      _store(nullptr),
      _vecBufDirty(true),
      _isArray(false) {}

/// Construct with node name in tree structure
Node::Node(std::string const& name, Node* parent)
    : _node_name(name),
      _parent(parent),
      _store(nullptr),
      _vecBufDirty(true),
      _isArray(false) {}

/// Construct for store
Node::Node(std::string const& name, Store* store)
    : _node_name(name),
      _parent(nullptr),
      _store(store),
      _vecBufDirty(true),
      _isArray(false) {}

/// Default dtor
Node::~Node() {}

/// Get slice to value buffer
Slice Node::slice() const {
  // Some array
  if (_isArray) {
    rebuildVecBuf();
    return Slice(_vecBuf.data());
  }

  // Some value
  if (!_value.empty()) {
    return Slice(_value.front().data());
  }

  // Empty object
  return arangodb::basics::VelocyPackHelper::EmptyObjectValue();
}

void Node::rebuildVecBuf() const {
  if (_vecBufDirty) {  // Dirty vector buffer
    Builder tmp;
    tmp.openArray();
    for (auto const& i : _value) {
      tmp.add(Slice(i.data()));
    }
    tmp.close();
    _vecBuf = *tmp.steal();
    _vecBufDirty = false;
  }
}

/// Get name of this node
std::string const& Node::name() const { return _node_name; }

/// Get full path of this node
std::string Node::uri() const {
  Node* par = _parent;
  std::stringstream path;
  std::deque<std::string> names;
  names.push_front(name());
  while (par != nullptr) {
    names.push_front(par->name());
    par = par->_parent;
  }
  for (size_t i = 1; i < names.size(); ++i) {
    path << "/" << names.at(i);
  }
  return path.str();
}

/// Move constructor
Node::Node(Node&& other)
    : _node_name(std::move(other._node_name)),
      _children(std::move(other._children)),
      _value(std::move(other._value)),
      _vecBuf(std::move(other._vecBuf)),
      _vecBufDirty(std::move(other._vecBufDirty)),
      _isArray(std::move(other._isArray)) {}

/// Copy constructor
Node::Node(Node const& other)
    : _node_name(other._node_name),
      _parent(nullptr),
      _store(nullptr),
      _value(other._value),
      _vecBuf(other._vecBuf),
      _vecBufDirty(other._vecBufDirty),
      _isArray(other._isArray) {
  for (auto const& p : other._children) {
    auto copy = std::make_shared<Node>(*p.second);
    _children.insert(std::make_pair(p.first, copy));
  }
}

/// Assignment operator (slice)
Node& Node::operator=(VPackSlice const& slice) {
  // 1. remove any existing time to live entry
  // 2. clear children map
  // 3. copy from rhs buffer to my buffer
  // Must not copy _parent, _ttl, _observers
  removeTimeToLive();
  _children.clear();
  _value.clear();
  if (slice.isArray()) {
    _isArray = true;
    _value.resize(slice.length());
    for (size_t i = 0; i < slice.length(); ++i) {
      _value.at(i).append(reinterpret_cast<char const*>(slice[i].begin()),
                          slice[i].byteSize());
    }
  } else {
    _isArray = false;
    _value.resize(1);
    _value.front().append(reinterpret_cast<char const*>(slice.begin()),
                          slice.byteSize());
  }
  _vecBufDirty = true;
  return *this;
}

/// Move operator
Node& Node::operator=(Node&& rhs) {
  // 1. remove any existing time to live entry
  // 2. move children map over
  // 3. move value over
  // Must not move ober rhs's _parent, _ttl, _observers
  removeTimeToLive();
  _node_name = std::move(rhs._node_name);
  _children = std::move(rhs._children);
  _value = std::move(rhs._value);
  _vecBuf = std::move(rhs._vecBuf);
  _vecBufDirty = std::move(rhs._vecBufDirty);
  _isArray = std::move(rhs._isArray);
  return *this;
}

/// Assignment operator
Node& Node::operator=(Node const& rhs) {
  // 1. remove any existing time to live entry
  // 2. clear children map
  // 3. move from rhs to buffer pointer
  // Must not move rhs's _parent, _ttl, _observers
  removeTimeToLive();
  _node_name = rhs._node_name;
  _children.clear();
  for (auto const& p : rhs._children) {
    auto copy = std::make_shared<Node>(*p.second);
    _children.insert(std::make_pair(p.first, copy));
  }
  _value = rhs._value;
  _vecBuf = rhs._vecBuf;
  _vecBufDirty = rhs._vecBufDirty;
  _isArray = rhs._isArray;
  return *this;
}

/// Comparison with slice
bool Node::operator==(VPackSlice const& rhs) const {
  if (rhs.isObject()) {
    return rhs.toJson() == toJson();
  } else {
    return rhs.equals(slice());
  }
}

// Comparison with slice
bool Node::operator!=(VPackSlice const& rhs) const { return !(*this == rhs); }

/// Remove this node from store
bool Node::remove() {
  Node& parent = *_parent;
  return parent.removeChild(_node_name);
}

/// Remove child by name
bool Node::removeChild(std::string const& key) {
  auto found = _children.find(key);
  if (found == _children.end()) {
    return false;
  }
  found->second->removeTimeToLive();
  _children.erase(found);
  return true;
}

/// Node type
NodeType Node::type() const { return _children.size() ? NODE : LEAF; }

/// lh-value at path vector
Node& Node::operator()(std::vector<std::string> const& pv) {
  if (!pv.empty()) {
    std::string const& key = pv.front();
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
Node const& Node::operator()(std::vector<std::string> const& pv) const {
  if (!pv.empty()) {
    std::string const& key = pv.front();
    if (_children.find(key) == _children.end()) {
      throw StoreException(std::string("Node ") + key +
                           std::string(" not found"));
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
Node& Node::operator()(std::string const& path) {
  return this->operator()(split(path, '/'));
}

// rh-value at path
Node const& Node::operator()(std::string const& path) const {
  return this->operator()(split(path, '/'));
}

// lh-store
Node const& Node::root() const {
  Node *par = _parent, *tmp = nullptr;
  while (par != nullptr) {
    tmp = par;
    par = par->_parent;
  }
  return *tmp;
}

// rh-store
Node& Node::root() {
  Node *par = _parent, *tmp = nullptr;
  while (par != nullptr) {
    tmp = par;
    par = par->_parent;
  }
  return *tmp;
}

Store& Node::store() { return *(root()._store); }

Store const& Node::store() const { return *(root()._store); }

// velocypack value type of this node
ValueType Node::valueType() const { return slice().type(); }

// file time to live entry for this node to now + millis
bool Node::addTimeToLive(long millis) {
  auto tkey =
      std::chrono::system_clock::now() + std::chrono::milliseconds(millis);
  store().timeTable().insert(std::pair<TimePoint, std::string>(tkey, uri()));
  _ttl = tkey;
  return true;
}

// remove time to live entry for this node
bool Node::removeTimeToLive() {
  if (_ttl != std::chrono::system_clock::time_point()) {
    store().removeTTL(uri());
  }
  return true;
}

inline bool Node::observedBy(std::string const& url) const {
  auto ret = store().observerTable().equal_range(url);
  for (auto it = ret.first; it != ret.second; ++it) {
    if (it->second == uri()) {
      return true;
    }
  }
  return false;
}

namespace arangodb {
namespace consensus {

/// Set value
template <>
bool Node::handle<SET>(VPackSlice const& slice) {
  Slice val = slice.get("new");

  if (val.isObject()) {
    if (val.hasKey("op")) {  // No longer a keyword but a regular key "op"
      if (_children.find("op") == _children.end()) {
        _children["op"] = std::make_shared<Node>("op", this);
      }
      *(_children["op"]) = val.get("op");
    } else {  // Deeper down
      this->applies(val);
    }
  } else {
    *this = val;
  }

  if (slice.hasKey("ttl")) {
    VPackSlice ttl_v = slice.get("ttl");
    if (ttl_v.isNumber()) {
      long ttl = 1000l * ((ttl_v.isDouble())
                              ? static_cast<long>(slice.get("ttl").getDouble())
                              : static_cast<long>(slice.get("ttl").getInt()));
      addTimeToLive(ttl);
    } else {
      LOG_TOPIC(WARN, Logger::AGENCY) << "Non-number value assigned to ttl: "
                                      << ttl_v.toJson();
    }
  }

  return true;
}

/// Increment integer value or set 1
template <>
bool Node::handle<INCREMENT>(VPackSlice const& slice) {
  Builder tmp;
  tmp.openObject();
  try {
    tmp.add("tmp", Value(this->slice().getInt() + 1));
  } catch (std::exception const&) {
    tmp.add("tmp", Value(1));
  }
  tmp.close();
  *this = tmp.slice().get("tmp");
  return true;
}

/// Decrement integer value or set -1
template <>
bool Node::handle<DECREMENT>(VPackSlice const& slice) {
  Builder tmp;
  tmp.openObject();
  try {
    tmp.add("tmp", Value(this->slice().getInt() - 1));
  } catch (std::exception const&) {
    tmp.add("tmp", Value(-1));
  }
  tmp.close();
  *this = tmp.slice().get("tmp");
  return true;
}

/// Append element to array
template <>
bool Node::handle<PUSH>(VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "Operator push without new value: "
                                    << slice.toJson();
    return false;
  }
  Builder tmp;
  tmp.openArray();
  if (this->slice().isArray()) {
    for (auto const& old : VPackArrayIterator(this->slice())) tmp.add(old);
  }
  tmp.add(slice.get("new"));
  tmp.close();
  *this = tmp.slice();
  return true;
}

/// Remove element from any place in array by value array
template <>
bool Node::handle<ERASE>(VPackSlice const& slice) {
  if (!slice.hasKey("val")) {
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "Operator erase without value to be erased: " << slice.toJson();
    return false;
  }
  Builder tmp;
  tmp.openArray();
  if (this->slice().isArray()) {
    for (auto const& old : VPackArrayIterator(this->slice())) {
      if (old != slice.get("val")) {
        tmp.add(old);
      }
    }
  }
  tmp.close();
  *this = tmp.slice();
  return true;
}

/// Replace element from any place in array by new value
template <>
bool Node::handle<REPLACE>(VPackSlice const& slice) {
  if (!slice.hasKey("val")) {
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "Operator erase without value to be erased: " << slice.toJson();
    return false;
  }
  if (!slice.hasKey("new")) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "Operator replace without new value: "
                                    << slice.toJson();
    return false;
  }
  Builder tmp;
  tmp.openArray();
  if (this->slice().isArray()) {
    for (auto const& old : VPackArrayIterator(this->slice())) {
      if (old == slice.get("val")) {
        tmp.add(slice.get("new"));
      } else {
        tmp.add(old);
      }
    }
  }
  tmp.close();
  *this = tmp.slice();
  return true;
}

/// Remove element from end of array.
template <>
bool Node::handle<POP>(VPackSlice const& slice) {
  Builder tmp;
  tmp.openArray();
  if (this->slice().isArray()) {
    VPackArrayIterator it(this->slice());
    if (it.size() > 1) {
      size_t j = it.size() - 1;
      for (auto old : it) {
        tmp.add(old);
        if (--j == 0) break;
      }
    }
  }
  tmp.close();
  *this = tmp.slice();
  return true;
}

/// Prepend element to array
template <>
bool Node::handle<PREPEND>(VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "Operator prepend without new value: "
                                    << slice.toJson();
    return false;
  }
  Builder tmp;
  tmp.openArray();
  tmp.add(slice.get("new"));
  if (this->slice().isArray()) {
    for (auto const& old : VPackArrayIterator(this->slice())) tmp.add(old);
  }
  tmp.close();
  *this = tmp.slice();
  return true;
}

/// Remove element from front of array
template <>
bool Node::handle<SHIFT>(VPackSlice const& slice) {
  Builder tmp;
  tmp.openArray();
  if (this->slice().isArray()) {  // If a
    VPackArrayIterator it(this->slice());
    bool first = true;
    for (auto const& old : it) {
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
template <>
bool Node::handle<OBSERVE>(VPackSlice const& slice) {
  if (!slice.hasKey("url")) return false;
  if (!slice.get("url").isString()) return false;
  std::string url(slice.get("url").copyString()), uri(this->uri());

  // check if such entry exists
  if (!observedBy(url)) {
    store().observerTable().emplace(
        std::pair<std::string, std::string>(url, uri));
    store().observedTable().emplace(
        std::pair<std::string, std::string>(uri, url));
    return true;
  }

  return false;
}

/// Remove observer for this node
template <>
bool Node::handle<UNOBSERVE>(VPackSlice const& slice) {
  if (!slice.hasKey("url")) return false;
  if (!slice.get("url").isString()) return false;
  std::string url(slice.get("url").copyString()), uri(this->uri());

  // delete in both cases a single entry (ensured above)
  // breaking the iterators is fine then
  auto ret = store().observerTable().equal_range(url);
  for (auto it = ret.first; it != ret.second; ++it) {
    if (it->second == uri) {
      store().observerTable().erase(it);
      break;
    }
  }
  ret = store().observedTable().equal_range(uri);
  for (auto it = ret.first; it != ret.second; ++it) {
    if (it->second == url) {
      store().observedTable().erase(it);
      return true;
    }
  }

  return false;
}
}
}

bool Node::applieOp(VPackSlice const& slice) {
  std::string oper = slice.get("op").copyString();

  if (oper == "delete") {
    if (_parent == nullptr) {  // root node
      _children.clear();
      _value.clear();
      return true;
    } else {
      return _parent->removeChild(_node_name);
    }
  } else if (oper == "set") {  // "op":"set"
    return handle<SET>(slice);
  } else if (oper == "increment") {  // "op":"increment"
    return handle<INCREMENT>(slice);
  } else if (oper == "decrement") {  // "op":"decrement"
    return handle<DECREMENT>(slice);
  } else if (oper == "push") {  // "op":"push"
    return handle<PUSH>(slice);
  } else if (oper == "pop") {  // "op":"pop"
    return handle<POP>(slice);
  } else if (oper == "prepend") {  // "op":"prepend"
    return handle<PREPEND>(slice);
  } else if (oper == "shift") {  // "op":"shift"
    return handle<SHIFT>(slice);
  } else if (oper == "observe") {  // "op":"observe"
    return handle<OBSERVE>(slice);
  } else if (oper == "unobserve") {  // "op":"unobserve"
    return handle<UNOBSERVE>(slice);
  } else if (oper == "erase") {  // "op":"erase"
    return handle<ERASE>(slice);
  } else if (oper == "replace") {  // "op":"replace"
    return handle<REPLACE>(slice);
  } else {  // "op" might not be a key word after all
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "Keyword 'op' without known operation. Handling as regular key: \""
        << oper << "\"";
  }

  return false;
}

// Apply slice to this node
bool Node::applies(VPackSlice const& slice) {
  std::regex reg("/+");

  if (slice.isObject()) {
    for (auto const& i : VPackObjectIterator(slice)) {
      std::string key = std::regex_replace(i.key.copyString(), reg, "/");
      if (key.find('/') != std::string::npos) {
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

void Node::toBuilder(Builder& builder, bool showHidden) const {
  try {
    if (type() == NODE) {
      VPackObjectBuilder guard(&builder);
      for (auto const& child : _children) {
        if (child.first[0] == '.' && !showHidden) {
          continue;
        }
        builder.add(VPackValue(child.first));
        child.second->toBuilder(builder);
      }
    } else {
      if (!slice().isNone()) {
        builder.add(slice());
      }
    }

  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
  }
}

// Print internals to ostream
std::ostream& Node::print(std::ostream& o) const {
  Node const* par = _parent;
  while (par != nullptr) {
    par = par->_parent;
    o << "  ";
  }

  o << _node_name << " : ";

  if (type() == NODE) {
    o << std::endl;
    for (auto const& i : _children) o << *(i.second);
  } else {
    o << ((slice().isNone()) ? "NONE" : slice().toJson());
    if (_ttl != std::chrono::system_clock::time_point()) {
      o << " ttl! ";
    }
    o << std::endl;
  }

  return o;
}

Node::Children& Node::children() { return _children; }

Node::Children const& Node::children() const { return _children; }

std::string Node::toJson() const {
  Builder builder;
  builder.openArray();
  toBuilder(builder);
  builder.close();
  std::string strval = builder.slice()[0].isString()
                           ? builder.slice()[0].copyString()
                           : builder.slice()[0].toJson();
  return strval;
}

Node const* Node::parent() const { return _parent; }

std::vector<std::string> Node::exists(
    std::vector<std::string> const& rel) const {
  std::vector<std::string> result;
  Node const* cur = this;
  for (auto const& sub : rel) {
    auto tmp = cur->children().find(sub);
    if (tmp != cur->children().end()) {
      cur = tmp->second.get();
      result.push_back(sub);
    } else {
      break;
    }
  }
  return result;
}

std::vector<std::string> Node::exists(std::string const& rel) const {
  return exists(split(rel, '/'));
}

int Node::getInt() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to int");
  }
  return static_cast<int>(slice().getInt());
}

uint64_t Node::getUInt() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to unsigned int");
  }
  return slice().getUInt();
}

bool Node::getBool() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to bool");
  }
  return slice().getBool();
}

double Node::getDouble() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to int");
  }
  return slice().getDouble();
}

std::string Node::getString() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to string");
  }
  return slice().copyString();
}
