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

// Construct with node name
Node::Node(std::string const& name)
    : _node_name(name), _parent(nullptr), _store(nullptr) {
  _value.clear();
}

// Construct with node name in tree structure
Node::Node(std::string const& name, Node* parent)
    : _node_name(name), _parent(parent), _store(nullptr) {
  _value.clear();
}

// Construct for store
Node::Node(std::string const& name, Store* store)
    : _node_name(name), _parent(nullptr), _store(store) {
  _value.clear();
}

// Default dtor
Node::~Node() {}

Slice Node::slice() const {
  return (_value.size() == 0)
             ? arangodb::basics::VelocyPackHelper::EmptyObjectValue()
             : Slice(_value.data());
}

// Get name of this node
std::string const& Node::name() const { return _node_name; }

// Get full path of this node
std::string Node::uri() const {
  Node* par = _parent;
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
Node& Node::operator=(VPackSlice const& slice) {
  // 1. remove any existing time to live entry
  // 2. clear children map
  // 3. copy from rhs to buffer pointer
  // 4. inform all observers here and above
  // Must not copy _parent, _ttl, _observers
  removeTimeToLive();
  _children.clear();
  _value.reset();
  _value.append(reinterpret_cast<char const*>(slice.begin()), slice.byteSize());
  return *this;
}

// Assignment of rhs node
Node& Node::operator=(Node const& rhs) {
  // 1. remove any existing time to live entry
  // 2. clear children map
  // 3. copy from rhs to buffer pointer
  // 4. inform all observers here and above
  // Must not copy rhs's _parent, _ttl, _observers
  removeTimeToLive();
  _node_name = rhs._node_name;
  _value = rhs._value;
  _children = rhs._children;
  return *this;
}

// Comparison with slice
bool Node::operator==(VPackSlice const& rhs) const {
  if (rhs.isObject()) {
    return rhs.toJson() == toJson();
  } else {
    return rhs.equals(slice());
  }
}

// Comparison with slice
bool Node::operator!=(VPackSlice const& rhs) const { return !(*this == rhs); }

// Remove this node from store
bool Node::remove() {
  Node& parent = *_parent;
  return parent.removeChild(_node_name);
}

// Remove child by name
bool Node::removeChild(std::string const& key) {
  auto found = _children.find(key);
  if (found == _children.end()) {
    return false;
  }
  found->second->removeTimeToLive();
  _children.erase(found);
  return true;
}

// Node type
NodeType Node::type() const { return _children.size() ? NODE : LEAF; }

// lh-value at path vector
Node& Node::operator()(std::vector<std::string> const& pv) {
  if (pv.size()) {
    std::string const& key = pv.at(0);
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
  if (pv.size()) {
    std::string const& key = pv.at(0);
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
  Node* par = _parent, * tmp = 0;
  while (par != 0) {
    tmp = par;
    par = par->_parent;
  }
  return *tmp;
}

// rh-store
Node& Node::root() {
  Node* par = _parent, * tmp = 0;
  while (par != 0) {
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
      LOG_TOPIC(WARN, Logger::AGENCY)
          << "Non-number value assigned to ttl: " << ttl_v.toJson();
    }
  }

  return true;
}

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

template <>
bool Node::handle<PUSH>(VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "Operator push without new value: " << slice.toJson();
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

template <>
bool Node::handle<PREPEND>(VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "Operator prepend without new value: " << slice.toJson();
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
    return _parent->removeChild(_node_name);
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
  } else {  // "op" might not be a key word after all
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "Keyword 'op' without known operation. Handling as regular key.";
  }

  return false;
}

// Apply slice to this node
bool Node::applies(VPackSlice const& slice) {
  if (slice.isObject()) {
    // Object is an operation?
    if (slice.hasKey("op")) {
      if (applieOp(slice)) {
        return true;
      }
    }

    // Object is special case json
    for (auto const& i : VPackObjectIterator(slice)) {
      std::string key = i.key.copyString();
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

void Node::toBuilder(Builder& builder) const {
  try {
    if (type() == NODE) {
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
std::ostream& Node::print(std::ostream& o) const {
  Node const* par = _parent;
  while (par != 0) {
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
