///////////////////////////////////////////////////////////////////////////////
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

#include "Node.h"
#include "Store.h"

#include "Basics/StringUtils.h"

#include <velocypack/Compare.h>
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

/// @brief Construct with node name
Node::Node(std::string const& name)
    : _nodeName(name),
      _parent(nullptr),
      _store(nullptr),
      _vecBufDirty(true),
      _isArray(false) {}


/// @brief Construct with node name in tree structure
Node::Node(std::string const& name, Node* parent)
    : _nodeName(name),
      _parent(parent),
      _store(nullptr),
      _vecBufDirty(true),
      _isArray(false) {}


/// @brief Construct for store
Node::Node(std::string const& name, Store* store)
    : _nodeName(name),
      _parent(nullptr),
      _store(store),
      _vecBufDirty(true),
      _isArray(false) {}

/// @brief Default dtor
Node::~Node() {}


/// @brief Get slice to value buffer
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
  return arangodb::velocypack::Slice::emptyObjectSlice();
}


/// @brief Optimization, which avoids recreating of Builder for output if
/// changes have not happened since last call
void Node::rebuildVecBuf() const {
  if (_vecBufDirty) {  // Dirty vector buffer
    Builder tmp;
    { VPackArrayBuilder t(&tmp);
      for (auto const& i : _value) {
        tmp.add(Slice(i.data()));
      }
    }
    _vecBuf = *tmp.steal();
    _vecBufDirty = false;
  }
}


/// @brief Get name of this node
std::string const& Node::name() const { return _nodeName; }


/// @brief Get full path of this node
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


/// @brief Move constructor
Node::Node(Node&& other)
    : _nodeName(std::move(other._nodeName)),
      _parent(nullptr),
      _store(nullptr),
      _children(std::move(other._children)),
      _ttl(std::move(other._ttl)),
      _value(std::move(other._value)),
      _vecBuf(std::move(other._vecBuf)),
      _vecBufDirty(std::move(other._vecBufDirty)),
      _isArray(std::move(other._isArray)) {
  // The _children map has been moved here, therefore we must
  // correct the _parent entry of all direct children:
  for (auto& child : _children) {
    child.second->_parent = this;
  }
}

/// @brief Copy constructor
Node::Node(Node const& other)
    : _nodeName(other._nodeName),
      _parent(nullptr),
      _store(nullptr),
      _ttl(other._ttl),
      _value(other._value),
      _vecBuf(other._vecBuf),
      _vecBufDirty(other._vecBufDirty),
      _isArray(other._isArray) {
  for (auto const& p : other._children) {
    auto copy = std::make_shared<Node>(*p.second);
    copy->_parent = this;   // new children have us as _parent!
    _children.insert(std::make_pair(p.first, copy));
  }
}

/// @brief Assignment operator (slice)
/// 1. remove any existing time to live entry
/// 2. clear children map
/// 3. copy from rhs buffer to my buffer
/// @brief Must not copy _parent, _ttl, _observers
Node& Node::operator=(VPackSlice const& slice) {
  removeTimeToLive();
  _children.clear();
  _value.clear();
  if (slice.isArray()) {
    _isArray = true;
    _value.resize(slice.length());
    for (size_t i = 0; i < slice.length(); ++i) {
      _value.at(i).append(
        reinterpret_cast<char const*>(slice[i].begin()), slice[i].byteSize());
    }
  } else {
    _isArray = false;
    _value.resize(1);
    _value.front().append(
      reinterpret_cast<char const*>(slice.begin()), slice.byteSize());
  }
  _vecBufDirty = true;
  return *this;
}

/// @brief Move operator
Node& Node::operator=(Node&& rhs) {
  // 1. remove any existing time to live entry
  // 2. move children map over
  // 3. move value over
  // Must not move over rhs's _parent, _observers
  _nodeName = std::move(rhs._nodeName);
  _children = std::move(rhs._children);
  // The _children map has been moved here, therefore we must
  // correct the _parent entry of all direct children:
  for (auto& child : _children) {
    child.second->_parent = this;
  }
  _value = std::move(rhs._value);
  _vecBuf = std::move(rhs._vecBuf);
  _vecBufDirty = std::move(rhs._vecBufDirty);
  _isArray = std::move(rhs._isArray);
  _ttl = std::move(rhs._ttl);
  return *this;
}

/// Assignment operator
Node& Node::operator=(Node const& rhs) {
  // 1. remove any existing time to live entry
  // 2. clear children map
  // 3. move from rhs to buffer pointer
  // Must not move rhs's _parent, _observers
  removeTimeToLive();
  _nodeName = rhs._nodeName;
  _children.clear();
  for (auto const& p : rhs._children) {
    auto copy = std::make_shared<Node>(*p.second);
    copy->_parent = this;   // new child copy has us as _parent
    _children.insert(std::make_pair(p.first, copy));
  }
  _value = rhs._value;
  _vecBuf = rhs._vecBuf;
  _vecBufDirty = rhs._vecBufDirty;
  _isArray = rhs._isArray;
  _ttl = rhs._ttl;
  return *this;
}


/// @brief Comparison with slice
bool Node::operator==(VPackSlice const& rhs) const {
  if (rhs.isObject()) {
    // build object recursively, take ttl into account
    return VPackNormalizedCompare::equals(toBuilder().slice(), rhs);
  } else {
    return VPackNormalizedCompare::equals(slice(), rhs);
  }
}


/// @brief Comparison with slice
bool Node::operator!=(VPackSlice const& rhs) const { return !(*this == rhs); }


/// @brief Remove this node from store
/// i.e. Remove this node as child of parent node if applicable
bool Node::remove() {
  if (_parent == nullptr) {
    return false;
  }
  Node& parent = *_parent;
  return parent.removeChild(_nodeName);
}


/// @brief Remove child by name
bool Node::removeChild(std::string const& key) {
  auto found = _children.find(key);
  if (found == _children.end()) {
    return false;
  }
  found->second->removeTimeToLive();
  _children.erase(found);
  return true;
}


/// @brief Node type
/// The check is if we are an array or a value. => LEAF. NODE else
NodeType Node::type() const { return (_isArray || _value.size()) ? LEAF : NODE; }


/// @brief lh-value at path vector
Node& Node::operator()(std::vector<std::string> const& pv) {
  if (!pv.empty()) {
    std::string const& key = pv.front();
    // Create new child
    // 1. Remove any values, 2. add new child
    if (_children.find(key) == _children.end()) {
      _isArray = false;
      if (!_value.empty()) {
        _value.clear();
      }
      _children[key] = std::make_shared<Node>(key, this);
    }
    auto pvc(pv);
    pvc.erase(pvc.begin());
    TRI_ASSERT(_children[key]->_parent == this);
    return (*_children[key])(pvc);
  } else {
    return *this;
  }
}


/// @brief rh-value at path vector. Check if TTL has expired.
Node const& Node::operator()(std::vector<std::string> const& pv) const {
  if (!pv.empty()) {
    auto const& key = pv.front();
    auto const it = _children.find(key);
    if (it == _children.end() ||
        (it->second->_ttl != std::chrono::system_clock::time_point() &&
         it->second->_ttl  < std::chrono::system_clock::now())) {
      throw StoreException(std::string("Node ") + key + " not found!");
    }
    auto const& child = *_children.at(key);
    TRI_ASSERT(child._parent == this);
    auto pvc(pv);
    pvc.erase(pvc.begin());
    return child(pvc);
  } else {
    return *this;
  }
}


/// @brief lh-value at path
Node& Node::operator()(std::string const& path) {
  return this->operator()(split(path, '/'));
}

/// @brief rh-value at path
Node const& Node::operator()(std::string const& path) const {
  return this->operator()(split(path, '/'));
}

// Get method which always throws when not found:
Node const& Node::get(std::string const& path) const {
  return this->operator()(path);
}

// lh-store
Node const& Node::root() const {
  Node* par = _parent;
  Node const* tmp = this;
  while (par != nullptr) {
    tmp = par;
    par = par->_parent;
  }
  return *tmp;
}

// rh-store
Node& Node::root() {
  Node* par = _parent;
  Node* tmp = this;
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
    _ttl = std::chrono::system_clock::time_point();
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
      long ttl = 1000l *
        ((ttl_v.isDouble())
         ? static_cast<long>(slice.get("ttl").getNumber<double>())
         : static_cast<long>(slice.get("ttl").getNumber<int>()));
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

  size_t inc = (slice.hasKey("step") && slice.get("step").isUInt()) ?
    slice.get("step").getUInt() : 1;

  Builder tmp;
  { VPackObjectBuilder t(&tmp);
    try {
      tmp.add("tmp", Value(this->slice().getInt() + inc));
    } catch (std::exception const&) {
      tmp.add("tmp", Value(1));
    }
  }
  *this = tmp.slice().get("tmp");
  return true;
}

/// Decrement integer value or set -1
template <>
bool Node::handle<DECREMENT>(VPackSlice const& slice) {
  Builder tmp;
  { VPackObjectBuilder t(&tmp);
    try {
      tmp.add("tmp", Value(this->slice().getInt() - 1));
    } catch (std::exception const&) {
      tmp.add("tmp", Value(-1));
    }
  }
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
  { VPackArrayBuilder t(&tmp);
    if (this->slice().isArray()) {
      for (auto const& old : VPackArrayIterator(this->slice())) tmp.add(old);
    }
    tmp.add(slice.get("new"));
  }
  *this = tmp.slice();
  return true;
}

/// Remove element from any place in array by value or position
template <> bool Node::handle<ERASE>(VPackSlice const& slice) {
  bool haveVal = slice.hasKey("val");
  bool havePos = slice.hasKey("pos");

  if (!haveVal && !havePos) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Operator erase without value or position to be erased is illegal: "
      << slice.toJson();
    return false;
  } else if (haveVal && havePos) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Operator erase with value and position to be erased is illegal: "
      << slice.toJson();
    return false;
  } else if (havePos &&
             (!slice.get("pos").isUInt() && !slice.get("pos").isSmallInt())) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Operator erase with non-positive integer position is illegal: "
      << slice.toJson();
  }

  Builder tmp;
  { VPackArrayBuilder t(&tmp);

    if (this->slice().isArray()) {
      if (haveVal) {
        VPackSlice valToErase = slice.get("val");
        for (auto const& old : VPackArrayIterator(this->slice())) {
          if (VelocyPackHelper::compare(old, valToErase, /*useUTF8*/true) != 0) {
            tmp.add(old);
          }
        }
      } else {
        size_t pos = slice.get("pos").getNumber<size_t>();
        if (pos >= this->slice().length()) {
          return false;
        }
        size_t n = 0;
        for (const auto& old : VPackArrayIterator(this->slice())) {
          if (n != pos) {
            tmp.add(old);
          }
          ++n;
        }
      }
    }

  }

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
  { VPackArrayBuilder t(&tmp);
    if (this->slice().isArray()) {
      VPackSlice valToRepl = slice.get("val");
      for (auto const& old : VPackArrayIterator(this->slice())) {
        if (VelocyPackHelper::compare(old, valToRepl, /*useUTF8*/true) == 0) {
          tmp.add(slice.get("new"));
        } else {
          tmp.add(old);
        }
      }
    }
  }
  *this = tmp.slice();
  return true;
}

/// Remove element from end of array.
template <>
bool Node::handle<POP>(VPackSlice const& slice) {
  Builder tmp;
  { VPackArrayBuilder t(&tmp);
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
  }
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
  { VPackArrayBuilder t(&tmp);
    tmp.add(slice.get("new"));
    if (this->slice().isArray()) {
      for (auto const& old : VPackArrayIterator(this->slice())) tmp.add(old);
    }
  }
  *this = tmp.slice();
  return true;
}

/// Remove element from front of array
template <>
bool Node::handle<SHIFT>(VPackSlice const& slice) {
  Builder tmp;
  { VPackArrayBuilder t(&tmp);
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
  }
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

}}

bool Node::applieOp(VPackSlice const& slice) {
  std::string oper = slice.get("op").copyString();

  if (oper == "delete") {
    if (_parent == nullptr) {  // root node
      _children.clear();
      _value.clear();
      return true;
    } else {
      return _parent->removeChild(_nodeName);
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
    handle<UNOBSERVE>(slice);
    if (_children.empty() && _value.empty()) {
      if (_parent == nullptr) {  // root node
        _children.clear();
        _value.clear();
        return true;
      } else {
        return _parent->removeChild(_nodeName);
      }
    }
    return true;
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

  clear();

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

  typedef std::chrono::system_clock clock;
  try {
    if (type() == NODE) {
      VPackObjectBuilder guard(&builder);
      for (auto const& child : _children) {
        auto const& cptr = child.second;
        if ((cptr->_ttl != clock::time_point() && cptr->_ttl < clock::now()) ||
            (child.first[0] == '.' && !showHidden )) {
          continue;
        }
        builder.add(VPackValue(child.first));
        cptr->toBuilder(builder);
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
  Builder builder;
  toBuilder(builder);
  o << builder.toJson();
  return o;
}

Node::Children& Node::children() { return _children; }

Node::Children const& Node::children() const { return _children; }

Builder Node::toBuilder() const {
  Builder builder;
  toBuilder(builder);
  return builder;
}

std::string Node::toJson() const {
  return toBuilder().toJson();
}

Node const* Node::parent() const { return _parent; }

std::vector<std::string> Node::exists(
  std::vector<std::string> const& rel) const {
  std::vector<std::string> result;
  Node const* cur = this;
  for (auto const& sub : rel) {
    auto it = cur->children().find(sub);
    if (it != cur->children().end() &&
        (it->second->_ttl == std::chrono::system_clock::time_point() ||
         it->second->_ttl >= std::chrono::system_clock::now())) {
      cur = it->second.get();
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

bool Node::has(std::vector<std::string> const& rel) const {
  return exists(rel).size() == rel.size();
}

bool Node::has(std::string const& rel) const {
  return has(split(rel, '/'));
}

int64_t Node::getInt() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to int");
  }
  return slice().getNumber<int64_t>();
}

uint64_t Node::getUInt() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to unsigned int");
  }
  return slice().getNumber<uint64_t>();
}

bool Node::getBool() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to bool");
  }
  return slice().getBool();
}

bool Node::isBool() const {
  if (type() == NODE) {
    return false;
  }
  return slice().isBool();
}

bool Node::isDouble() const {
  if (type() == NODE) {
    return false;
  }
  return slice().isDouble();
}

bool Node::isString() const {
  if (type() == NODE) {
    return false;
  }
  return slice().isString();
}

bool Node::isUInt() const {
  if (type() == NODE) {
    return false;
  }
  return slice().isUInt() || slice().isSmallInt();
}

bool Node::isInt() const {
  if (type() == NODE) {
    return false;
  }
  return slice().isInt() || slice().isSmallInt();
}

bool Node::isNumber() const {
  if (type() == NODE) {
    return false;
  }
  return slice().isNumber();
}

double Node::getDouble() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to double");
  }
  return slice().getNumber<double>();
}


std::pair<Node const&, bool> Node::hasAsNode(
  std::string const& url) const {

  // *this is bogus initializer
  std::pair<Node const&, bool> fail_pair = {*this, false};

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    std::pair<Node const&, bool> good_pair = {target, true};
    return good_pair;
  } catch (...) {
    // do nothing, fail_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsNode had exception processing " << url;
  } // catch

  return fail_pair;
} // hasAsNode


std::pair<Node&, bool> Node::hasAsWritableNode(
  std::string const& url) {

  // *this is bogus initializer
  std::pair<Node&, bool> fail_pair = {*this, false};

  // retrieve node, throws if does not exist
  try {
    Node & target(operator()(url));
    std::pair<Node&, bool> good_pair = {target, true};
    return good_pair;
  } catch (...) {
    // do nothing, fail_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsWritableNode had exception processing " << url;
  } // catch

  return fail_pair;
} // hasAsWritableNode


std::pair<NodeType, bool> Node::hasAsType(
  std::string const& url) const {

  std::pair<NodeType, bool> ret_pair = {NODE, false};

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    ret_pair.first = target.type();
    ret_pair.second = true;
  } catch (...) {
    // do nothing, fail_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsType had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsType


std::pair<Slice, bool> Node::hasAsSlice(
  std::string const& url) const {

  // *this is bogus initializer
  std::pair<Slice, bool> ret_pair =
    {arangodb::velocypack::Slice::emptyObjectSlice(), false};

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    ret_pair.first = target.slice();
    ret_pair.second = true;
  } catch (...) {
    // do nothing, ret_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsSlice had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsSlice


std::pair<uint64_t, bool> Node::hasAsUInt(
  std::string const& url) const {
  std::pair<uint64_t, bool> ret_pair(0, false);

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    if (target.isNumber()) {
      ret_pair.first = target.getUInt();
      ret_pair.second = true;
    }
  } catch (...) {
    // do nothing, ret_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsUInt had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsUInt


std::pair<bool, bool> Node::hasAsBool(
  std::string const& url) const {
  std::pair<bool, bool> ret_pair(false, false);

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    if (target.isBool()) {
      ret_pair.first = target.getBool();
      ret_pair.second = true;
    }
  } catch (...) {
    // do nothing, ret_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsBool had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsBool


std::pair<std::string, bool> Node::hasAsString(
  std::string const& url) const {
  std::pair<std::string, bool> ret_pair;

  ret_pair.second = false;

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    if (target.isString()) {
      ret_pair.first = target.getString();
      ret_pair.second = true;
    }
  } catch (...) {
    // do nothing, ret_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsString had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsString


std::pair<Node::Children, bool> Node::hasAsChildren(
  std::string const& url) const {
  std::pair<Children, bool> ret_pair;

  ret_pair.second = false;

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    ret_pair.first = target.children();
    ret_pair.second = true;
  } catch (...) {
    // do nothing, ret_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsChildren had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsChildren


std::pair<void*, bool> Node::hasAsBuilder(
  std::string const& url, Builder& builder, bool showHidden) const {
  std::pair<void*, bool> ret_pair(nullptr, false);

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    target.toBuilder(builder, showHidden);
    ret_pair.second = true;
  } catch (...) {
    // do nothing, ret_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsBuilder(1) had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsBuilder


std::pair<Builder, bool> Node::hasAsBuilder(
  std::string const& url) const {
  Builder builder;
  std::pair<Builder, bool> ret_pair(builder, false);

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    target.toBuilder(builder);
    ret_pair.first = builder;  // update
    ret_pair.second = true;
  } catch (...) {
    // do nothing, ret_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsBuilder(2) had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsBuilder


std::pair<Slice, bool> Node::hasAsArray(
  std::string const& url) const {

  // *this is bogus initializer
  std::pair<Slice, bool> ret_pair =
    {arangodb::velocypack::Slice::emptyObjectSlice(), false};

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    ret_pair.first = target.getArray();
    ret_pair.second = true;
  } catch (...) {
    // do nothing, ret_pair second already false
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "hasAsArray had exception processing " << url;
  } // catch

  return ret_pair;
} // hasAsArray


std::string Node::getString() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to string");
  }
  return slice().copyString();
}

Slice Node::getArray() const {
  if (type() == NODE) {
    throw StoreException("Must not convert NODE type to array");
  }
  if (!_isArray) {
    throw StoreException("Not an array type");
  }
  rebuildVecBuf();
  return Slice(_vecBuf.data());
}

void Node::clear() {
  _children.clear();
  _ttl = std::chrono::system_clock::time_point();
  _value.clear();
  _vecBuf.clear();
  _vecBufDirty = true;
  _isArray = false;
}
