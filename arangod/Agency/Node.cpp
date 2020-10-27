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

#include "Node.h"
#include "Store.h"

#include "AgencyStrings.h"
#include "Basics/StringUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <deque>

using namespace arangodb::consensus;
using namespace arangodb::basics;

const Node::Children Node::dummyChildren = Node::Children();
const Node Node::_dummyNode = Node("dumm-di-dumm");

/// @brief Construct with node name
Node::Node(std::string const& name)
    : _nodeName(name), _parent(nullptr), _store(nullptr), _vecBufDirty(true), _isArray(false) {}

/// @brief Construct with node name in tree structure
Node::Node(std::string const& name, Node* parent)
    : _nodeName(name), _parent(parent), _store(nullptr), _vecBufDirty(true), _isArray(false) {}

/// @brief Construct for store
Node::Node(std::string const& name, Store* store)
    : _nodeName(name), _parent(nullptr), _store(store), _vecBufDirty(true), _isArray(false) {}

/// @brief Default dtor
Node::~Node() = default;

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
    {
      VPackArrayBuilder t(&tmp);
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
Node::Node(Node&& other) noexcept
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
    copy->_parent = this;  // new children have us as _parent!
    _children.insert(std::make_pair(p.first, copy));
  }
}

/// @brief Assignment operator (slice)
/// 1. remove any existing time to live entry
/// 2. clear children map
/// 3. copy from rhs buffer to my buffer
/// @brief Must not copy _parent, _store, _ttl
Node& Node::operator=(VPackSlice const& slice) {
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
    _value.front().append(reinterpret_cast<char const*>(slice.begin()), slice.byteSize());
  }
  _vecBufDirty = true;
  return *this;
}

// Move operator
// cppcheck-suppress operatorEqVarError
Node& Node::operator=(Node&& rhs) noexcept {
  // 1. remove any existing time to live entry
  // 2. move children map over
  // 3. move value over
  // Must not move over rhs's _parent, _store
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

// Assignment operator
// cppcheck-suppress operatorEqVarError
Node& Node::operator=(Node const& rhs) {
  // 1. remove any existing time to live entry
  // 2. clear children map
  // 3. move from rhs to buffer pointer
  // Must not move rhs's _parent, _store
  removeTimeToLive();
  _nodeName = rhs._nodeName;
  _children.clear();
  for (auto const& p : rhs._children) {
    auto copy = std::make_shared<Node>(*p.second);
    copy->_parent = this;  // new child copy has us as _parent
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

/// @brief Remove child by name
arangodb::ResultT<std::shared_ptr<Node>> Node::removeChild(std::string const& key) {
  auto it = _children.find(key);
  if (it == _children.end()) {
    return arangodb::ResultT<std::shared_ptr<Node>>::error(TRI_ERROR_FAILED);
  }
  auto ret = it->second;
  ret->removeTimeToLive();
  _children.erase(it);
  return arangodb::ResultT<std::shared_ptr<Node>>::success(std::move(ret));
}

/// @brief Node type
/// The check is if we are an array or a value. => LEAF. NODE else
NodeType Node::type() const {
  return (_isArray || _value.size()) ? LEAF : NODE;
}


bool Node::lifetimeExpired() const {
  using clock = std::chrono::system_clock;
  return _ttl != clock::time_point() && _ttl < clock::now();
}

/// @brief lh-value at path vector
Node& Node::operator()(std::vector<std::string> const& pv) {
  Node* current = this;

  for (std::string const& key : pv) {

    // Remove TTL for any nodes on the way down, if and only if the noted TTL
    // is expired.
    if (current->lifetimeExpired()) {
      current->clear();
    }

    auto& children = current->_children;
    auto  child = children.find(key);

    if (child == children.end()) {

      current->_isArray = false;
      if (!current->_value.empty()) {
        current->_value.clear();
      }

      auto const& node = std::make_shared<Node>(key, current);
      children.insert(Children::value_type(key, node));

      current = node.get();
    } else {
      current = child->second.get();
    }
  }

  return *current;
}

/// @brief rh-value at path vector. Check if TTL has expired.
Node const& Node::operator()(std::vector<std::string> const& pv) const {

  Node const* current = this;

  for (std::string const& key : pv) {

    auto const& children = current->_children;
    auto const  child = children.find(key);

    if (child == children.end() || child->second->lifetimeExpired()) {
      throw StoreException(std::string("Node ") + uri() + "/" + key + " not found!");
    }  else {
      current = child->second.get();
    }

  }

  return *current;

}


/// @brief lh-value at path
Node& Node::operator()(std::string const& path) {
  return this->operator()(Store::split(path));
}

/// @brief rh-value at path
Node const& Node::operator()(std::string const& path) const {
  return this->operator()(Store::split(path));
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

Store* Node::getRootStore() const {
  Node const* par = _parent;
  Node const* tmp = this;
  while (par != nullptr) {
    tmp = par;
    par = par->_parent;
  }
  return tmp->_store; // Can be nullptr if we are not in a Node that belongs
                      // to a store.
}

// velocypack value type of this node
ValueType Node::valueType() const { return slice().type(); }

// file time to live entry for this node to now + millis
bool Node::addTimeToLive(std::chrono::time_point<std::chrono::system_clock> const& tkey) {
  Store& store = *(root()._store);
  store.timeTable().insert(std::pair<TimePoint, std::string>(tkey, uri()));
  _ttl = tkey;
  return true;
}

void Node::timeToLive(TimePoint const& ttl) {
  _ttl = ttl;
}

// remove time to live entry for this node
bool Node::removeTimeToLive() {

  Store* s = getRootStore();  // We could be in a Node that belongs to a store,
                              // or in one that doesn't.
  if (s != nullptr) {
    s->removeTTL(uri());
  }
  if (_ttl != std::chrono::system_clock::time_point()) {
    _ttl = std::chrono::system_clock::time_point();
  }
  return true;
}

namespace arangodb {
namespace consensus {

/// Set value
template <>
ResultT<std::shared_ptr<Node>> Node::handle<SET>(VPackSlice const& slice) {

  using namespace std::chrono;

  Slice val = slice.get("new");
  if (val.isNone()) {
    // key "new" not present
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string(
        "Operator set without new value: ") + slice.toJson());
  }

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

  VPackSlice ttl_v = slice.get("ttl");
  if (!ttl_v.isNone()) {
    if (ttl_v.isNumber()) {

      // ttl in millisconds
      long ttl =
          1000l * ((ttl_v.isDouble())
                   ? static_cast<long>(ttl_v.getNumber<double>())
                   : static_cast<long>(ttl_v.getNumber<int>()));

      // calclate expiry time
      auto const expires = slice.hasKey("epoch_millis") ?
        time_point<system_clock>(
          milliseconds(slice.get("epoch_millis").getNumber<uint64_t>() + ttl)) :
        system_clock::now() + milliseconds(ttl);

      // set ttl limit
      addTimeToLive(expires);

    } else {
      LOG_TOPIC("66da2", WARN, Logger::AGENCY)
          << "Non-number value assigned to ttl: " << ttl_v.toJson();
    }
  }

  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Increment integer value or set 1
template <>
ResultT<std::shared_ptr<Node>> Node::handle<INCREMENT>(VPackSlice const& slice) {
  auto inc = getIntWithDefault(slice, "step", 1);
  auto pre = getNumberUnlessExpiredWithDefault<int64_t>();

  Builder tmp;
  {
    VPackObjectBuilder t(&tmp);
    tmp.add("tmp", Value(pre + inc));
  }
  *this = tmp.slice().get("tmp");
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Decrement integer value or set -1
template <>
ResultT<std::shared_ptr<Node>> Node::handle<DECREMENT>(VPackSlice const& slice) {
  auto dec = getIntWithDefault(slice, "step", 1);
  auto pre = getNumberUnlessExpiredWithDefault<std::int64_t>();

  Builder tmp;
  {
    VPackObjectBuilder t(&tmp);
    tmp.add("tmp", Value(pre - dec));
  }
  *this = tmp.slice().get("tmp");
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Append element to array
template <>
ResultT<std::shared_ptr<Node>> Node::handle<PUSH>(VPackSlice const& slice) {
  VPackSlice v = slice.get("new");
  if (v.isNone()) {
    // key "new" not present
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string(
        "Operator push without new value: ") + slice.toJson());
  }
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
    if (this->slice().isArray() && !lifetimeExpired()) {
      for (auto const& old : VPackArrayIterator(this->slice())) tmp.add(old);
    }
    tmp.add(v);
  }
  *this = tmp.slice();
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Remove element from any place in array by value or position
template <>
ResultT<std::shared_ptr<Node>> Node::handle<ERASE>(VPackSlice const& slice) {
  bool haveVal = slice.hasKey("val");
  bool havePos = slice.hasKey("pos");

  if (!haveVal && !havePos) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string(
        "Erase without value or position to be erased is illegal: ") + slice.toJson());
  } else if (haveVal && havePos) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string(
        "Erase without value or position to be erased is illegal: ") + slice.toJson());
  } else if (havePos && (!slice.get("pos").isUInt() && !slice.get("pos").isSmallInt())) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string(
        "Erase with non-positive integer position is illegal: ") + slice.toJson());
  }

  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);

    if (this->slice().isArray() && !lifetimeExpired()) {
      if (haveVal) {
        VPackSlice valToErase = slice.get("val");
        for (auto const& old : VPackArrayIterator(this->slice())) {
          if (!VelocyPackHelper::equal(old, valToErase, /*useUTF8*/ true)) {
            tmp.add(old);
          }
        }
      } else {
        size_t pos = slice.get("pos").getNumber<size_t>();
        if (pos >= this->slice().length()) {
          return ResultT<std::shared_ptr<Node>>::error(
            TRI_ERROR_FAILED, std::string(
              "Erase with position out of range: ") + slice.toJson());
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
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Replace element from any place in array by new value
template <>
ResultT<std::shared_ptr<Node>> Node::handle<REPLACE>(VPackSlice const& slice) {
  if (!slice.hasKey("val")) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string("Operator erase without value to be erased: ")
      + slice.toJson());
  }
  if (!slice.hasKey("new")) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string("Operator replace without new value: ")
      + slice.toJson());
  }
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
    if (this->slice().isArray() && !lifetimeExpired()) {
      VPackSlice valToRepl = slice.get("val");
      for (auto const& old : VPackArrayIterator(this->slice())) {
        if (VelocyPackHelper::equal(old, valToRepl, /*useUTF8*/ true)) {
          tmp.add(slice.get("new"));
        } else {
          tmp.add(old);
        }
      }
    }
  }
  *this = tmp.slice();
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Remove element from end of array.
template <>
ResultT<std::shared_ptr<Node>> Node::handle<POP>(VPackSlice const& slice) {
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
    if (this->slice().isArray() && !lifetimeExpired()) {
      VPackArrayIterator it(this->slice());
      if (it.size() > 1) {
        size_t j = it.size() - 1;
        for (auto old : it) {
          tmp.add(old);
          if (--j == 0) {
            break;
          }
        }
      }
    }
  }
  *this = tmp.slice();
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Prepend element to array
template <>
ResultT<std::shared_ptr<Node>> Node::handle<PREPEND>(VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string("Operator prepend without new value: ")
      + slice.toJson());
  }
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
    tmp.add(slice.get("new"));
    if (this->slice().isArray() && !lifetimeExpired()) {
      for (auto const& old : VPackArrayIterator(this->slice())) tmp.add(old);
    }
  }
  *this = tmp.slice();
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Remove element from front of array
template <>
ResultT<std::shared_ptr<Node>> Node::handle<SHIFT>(VPackSlice const& slice) {
  Builder tmp;
  {    VPackArrayBuilder t(&tmp);
    if (this->slice().isArray() && !lifetimeExpired()) {  // If a
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
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

template<>
ResultT<std::shared_ptr<Node>> Node::handle<READ_LOCK>(VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string("Invalid read lock: ") + slice.toJson());
  }

  if (isReadLockable(user.stringRef())) {
    Builder newValue;
    {
      VPackArrayBuilder arr(&newValue);
      if (this->slice().isArray()) {
        newValue.add(VPackArrayIterator(this->slice()));
      }
      newValue.add(user);
    }
    this->operator=(newValue.slice());
    return ResultT<std::shared_ptr<Node>>::success(nullptr);
  }

  return ResultT<std::shared_ptr<Node>>::error(TRI_ERROR_LOCKED);
}

template <>
ResultT<std::shared_ptr<Node>> Node::handle<READ_UNLOCK>(VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string("Invalid read unlock: ") + slice.toJson());
  }

  if (isReadUnlockable(user.stringRef())) {
    Builder newValue;
    {
      // isReadUnlockable ensured that `this->slice()` is always an array of strings
      VPackArrayBuilder arr(&newValue);
      for (auto const& i : VPackArrayIterator(this->slice())) {
        if (!i.isEqualString(user.stringRef())) {
          newValue.add(i);
        }
      }
    }
    Slice newValueSlice = newValue.slice();
    if (newValueSlice.length() == 0) {
      return deleteMe();
    } else {
      this->operator=(newValue.slice());
    }
    return ResultT<std::shared_ptr<Node>>::success(nullptr);
  }

  return ResultT<std::shared_ptr<Node>>::error(
    TRI_ERROR_FAILED, "Read unlock failed");

}

template<>
ResultT<std::shared_ptr<Node>> Node::handle<WRITE_LOCK>(VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string("Invalid write unlock: ") + slice.toJson());
  }

  if (isWriteLockable(user.stringRef())) {
    this->operator=(user);
    return ResultT<std::shared_ptr<Node>>::success(nullptr);
  }
  return ResultT<std::shared_ptr<Node>>::error(TRI_ERROR_LOCKED);

}

template<>
ResultT<std::shared_ptr<Node>> Node::handle<WRITE_UNLOCK>(VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return ResultT<std::shared_ptr<Node>>::error(
      TRI_ERROR_FAILED, std::string("Invalid write unlock: ") + slice.toJson());
  }

  if (isWriteUnlockable(user.stringRef())) {
    return deleteMe();
  }

  return ResultT<std::shared_ptr<Node>>::error(
    TRI_ERROR_FAILED, "Write unlock failed");
}

bool Node::isReadLockable(const VPackStringRef& by) const {
  if (!_children.empty()) {
    return false;
  }
  Slice slice = this->slice();
  // the following states are counted as readLockable
  // array - when read locked or read lock released
  // empty object - when the node is created
  if (slice.isArray()) {
    // check if `by` is not in the array
    for (auto const& i : VPackArrayIterator(slice)) {
      if (!i.isString() || i.isEqualString(VPackStringRef(by.data(), by.length()))) {
        return false;
      }
    }
    return true;
  }

  return slice.isEmptyObject();
}

bool Node::isReadUnlockable(const VPackStringRef& by) const {
  if (!_children.empty()) {
    return false;
  }
  Slice slice = this->slice();
  // the following states are counted as readUnlockable
  // array of strings containing the value `by`
  if (slice.isArray()) {
    bool valid = false;
    for (auto const& i : VPackArrayIterator(slice)) {
      if (!i.isString()) {
        valid = false;
        break;
      }
      if (i.isEqualString(VPackStringRef(by.data(), by.length()))) {
        valid = true;
      }
    }
    return valid;
  }
  return false;
}

bool Node::isWriteLockable(const VPackStringRef& by) const {
  if (!_children.empty()) {
    return false;
  }
  Slice slice = this->slice();
  // the following states are counted as writeLockable
  //  empty object - when the node is create
  return slice.isEmptyObject();
}

bool Node::isWriteUnlockable(const VPackStringRef& by) const {
  if (!_children.empty()) {
    return false;
  }
  Slice slice = this->slice();
  // the following states are counted as writeLockable
  //  string - when write lock was obtained
  return slice.isString() && slice.isEqualString(VPackStringRef(by.data(), by.length()));
}

}  // namespace consensus
}  // namespace arangodb

arangodb::ResultT<std::shared_ptr<Node>> Node::applyOp(VPackSlice const& slice) {
  std::string oper = slice.get("op").copyString();

  if (oper == "delete") {
    return deleteMe();
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
  } else if (oper == "erase") {  // "op":"erase"
    return handle<ERASE>(slice);
  } else if (oper == "replace") {  // "op":"replace"
    return handle<REPLACE>(slice);
  } else if (oper == OP_READ_LOCK) {
    return handle<READ_LOCK>(slice);
  } else if (oper == OP_READ_UNLOCK) {
    return handle<READ_UNLOCK>(slice);
  } else if (oper == OP_WRITE_LOCK) {
    return handle<WRITE_LOCK>(slice);
  } else if (oper == OP_WRITE_UNLOCK) {
    return handle<WRITE_UNLOCK>(slice);
  }

  return ResultT<std::shared_ptr<Node>>::error(
    TRI_ERROR_FAILED,
    std::string("Unknown operation '") + oper + "'");

}

// Apply slice to this node
bool Node::applies(VPackSlice const& slice) {
  clear();

  if (slice.isObject()) {
    for (auto const& i : VPackObjectIterator(slice)) {
      // note: no need to remove duplicate forward slashes here...
      //  if i.key contains duplicate forward slashes, then we will go
      //  into the  key.find('/')  case, and will be calling  operator()
      //  on the tainted key. And  operator()  calls  Store::split(),
      //  which will remove all duplicate forward slashes.
      std::string key = i.key.copyString();
      if (key.find('/') != std::string::npos) {
        (*this)(key).applies(i.value);
      } else {
        auto found = _children.find(key);
        if (found == _children.end()) {
          found = _children.emplace(key, std::make_shared<Node>(key, this)).first;
        }
        (*found).second->applies(i.value);
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
            (child.first[0] == '.' && !showHidden)) {
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
    LOG_TOPIC("44d99", ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
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

std::string Node::toJson() const { return toBuilder().toJson(); }

Node const* Node::parent() const { return _parent; }

std::vector<std::string> Node::exists(std::vector<std::string> const& rel) const {
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
  return exists(Store::split(rel));
}

bool Node::has(std::vector<std::string> const& rel) const {
  return exists(rel).size() == rel.size();
}

bool Node::has(std::string const& rel) const { return has(Store::split(rel)); }

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

std::pair<Node const&, bool> Node::hasAsNode(std::string const& url) const {
  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    return {target, true};
  } catch (...) {
    // *this is bogus initializer
    return {*this, false};
  }
}  // hasAsNode

std::pair<Node&, bool> Node::hasAsWritableNode(std::string const& url) {
  // retrieve node, throws if does not exist
  try {
    Node& target(operator()(url));
    return {target, true};
  } catch (...) {
    // *this is bogus initializer
    return {*this, false};
  }
}  // hasAsWritableNode

std::pair<NodeType, bool> Node::hasAsType(std::string const& url) const {
  std::pair<NodeType, bool> ret_pair = {NODE, false};

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    ret_pair.first = target.type();
    ret_pair.second = true;
  } catch (...) {
    // do nothing, fail_pair second already false
  }  // catch

  return ret_pair;
}  // hasAsType

std::pair<Slice, bool> Node::hasAsSlice(std::string const& url) const {
  // *this is bogus initializer
  std::pair<Slice, bool> ret_pair = {arangodb::velocypack::Slice::emptyObjectSlice(), false};

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    ret_pair.first = target.slice();
    ret_pair.second = true;
  } catch (...) {
    // do nothing, ret_pair second already false
  }  // catch

  return ret_pair;
}  // hasAsSlice

std::pair<uint64_t, bool> Node::hasAsUInt(std::string const& url) const {
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
  }  // catch

  return ret_pair;
}  // hasAsUInt

std::pair<bool, bool> Node::hasAsBool(std::string const& url) const {
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
  }  // catch

  return ret_pair;
}  // hasAsBool

std::pair<std::string, bool> Node::hasAsString(std::string const& url) const {
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
  }  // catch

  return ret_pair;
}  // hasAsString

std::pair<Node::Children const&, bool> Node::hasAsChildren(std::string const& url) const {

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    return std::pair<Children const&, bool> {target.children(), true};
  } catch (...) {
  }  // catch

  return std::pair<Children const&, bool> {dummyChildren, false};

}  // hasAsChildren

std::pair<void*, bool> Node::hasAsBuilder(std::string const& url, Builder& builder,
                                          bool showHidden) const {
  std::pair<void*, bool> ret_pair(nullptr, false);

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    target.toBuilder(builder, showHidden);
    ret_pair.second = true;
  } catch (...) {
    // do nothing, ret_pair second already false
  }  // catch

  return ret_pair;
}  // hasAsBuilder

std::pair<Builder, bool> Node::hasAsBuilder(std::string const& url) const {
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
  }  // catch

  return ret_pair;
}  // hasAsBuilder

std::pair<Slice, bool> Node::hasAsArray(std::string const& url) const {
  // *this is bogus initializer
  std::pair<Slice, bool> ret_pair = {arangodb::velocypack::Slice::emptyObjectSlice(), false};

  // retrieve node, throws if does not exist
  try {
    Node const& target(operator()(url));
    ret_pair.first = target.getArray();
    ret_pair.second = true;
  } catch (...) {
    // do nothing, ret_pair second already false
  }  // catch

  return ret_pair;
}  // hasAsArray

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
  removeTimeToLive();
  _value.clear();
  _vecBuf.clear();
  _vecBufDirty = true;
  _isArray = false;
}

[[nodiscard]] arangodb::ResultT<std::shared_ptr<Node>> Node::deleteMe() {
  if (_parent == nullptr) {  // root node
    _children.clear();
    _value.clear();
    _vecBufDirty = true;  // just in case there was an array
    return arangodb::ResultT<std::shared_ptr<Node>>::success(nullptr);
  } else {
    return _parent->removeChild(_nodeName);
  }
}

std::vector<std::string> Node::keys() const {
  std::vector<std::string> result;
  if (!_isArray) {
    result.reserve(_children.size());
    for (auto const& i : _children) {
      result.emplace_back(i.first);
    }
  }
  return result;
}

auto Node::getIntWithDefault(Slice slice, std::string_view key, std::int64_t def)
    -> std::int64_t {
  return arangodb::basics::VelocyPackHelper::getNumericValue<std::int64_t>(
      slice, VPackStringRef(key.data(), key.size()), def);
}
