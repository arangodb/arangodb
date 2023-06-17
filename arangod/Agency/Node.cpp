////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Agency/PathComponent.h"
#include "Basics/StringUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <deque>

using namespace arangodb::consensus;
using namespace arangodb::basics;
using namespace arangodb::velocypack;

class arangodb::consensus::SmallBuffer {
  uint8_t* _start;
  size_t _size;

 public:
  SmallBuffer() noexcept : _start(nullptr), _size(0) {}
  explicit SmallBuffer(size_t size) : SmallBuffer() {
    if (size > 0) {
      _start = new uint8_t[size];
      _size = size;
    }
  }
  explicit SmallBuffer(uint8_t const* data, size_t size) : SmallBuffer(size) {
    std::memcpy(_start, data, size);
  }
  SmallBuffer(SmallBuffer const& other) : SmallBuffer() {
    if (!other.empty()) {
      _start = new uint8_t[other._size];
      _size = other._size;
      std::memcpy(_start, other._start, other._size);
    }
  }
  SmallBuffer(SmallBuffer&& other) noexcept
      : _start(other._start), _size(other._size) {
    other._start = nullptr;
    other._size = 0;
  }
  SmallBuffer& operator=(SmallBuffer const& other) {
    if (this != &other) {
      if (!empty()) {
        delete[] _start;
      }
      if (other.empty()) {
        _start = nullptr;
        _size = 0;
      } else {
        _start = new uint8_t[other._size];
        _size = other._size;
        memcpy(_start, other._start, other._size);
      }
    }
    return *this;
  }
  SmallBuffer& operator=(SmallBuffer&& other) noexcept {
    if (this != &other) {
      if (!empty()) {
        delete[] _start;
      }
      _start = other._start;
      other._start = nullptr;
      _size = other._size;
      other._size = 0;
    }
    return *this;
  }
  ~SmallBuffer() { delete[] _start; }
  uint8_t* data() const { return _start; }
  size_t size() const { return _size; }
  bool empty() const { return _start == nullptr || _size == 0; }
};

const Node::Children Node::dummyChildren = Node::Children();
const Node Node::_dummyNode;

/// @brief Default dtor
Node::~Node() = default;

/// @brief Split strings by forward slashes, omitting empty strings,
/// and ignoring multiple subsequent forward slashes
std::vector<std::string> Node::split(std::string_view str) {
  std::vector<std::string> result;

  char const* p = str.data();
  char const* e = str.data() + str.size();

  // strip leading forward slashes
  while (p != e && *p == '/') {
    ++p;
  }

  // strip trailing forward slashes
  while (p != e && *(e - 1) == '/') {
    --e;
  }

  char const* start = nullptr;
  while (p != e) {
    if (*p == '/') {
      if (start != nullptr) {
        // had already found something
        result.emplace_back(start, p - start);
        start = nullptr;
      }
    } else {
      if (start == nullptr) {
        start = p;
      }
    }
    ++p;
  }
  if (start != nullptr) {
    result.emplace_back(start, p - start);
  }

  return result;
}

/// @brief Comparison with slice
bool Node::operator==(VPackSlice const& rhs) const {
  // build object recursively
  return VPackNormalizedCompare::equals(toBuilder().slice(), rhs);
}

/// @brief Comparison with slice
bool Node::operator!=(VPackSlice const& rhs) const { return !(*this == rhs); }

/// @brief Remove child by name
arangodb::ResultT<std::shared_ptr<Node>> Node::removeChild(
    std::string const& key) {
  if (auto children = std::get_if<Children>(&_value); children) {
    if (children->erase(key) == 1) {
      return {nullptr};
    }
  }

  return Result{TRI_ERROR_FAILED};
}

/// @brief rh-value at path vector. Check if TTL has expired.
Node const* Node::get(std::vector<std::string> const& pv) const {
  Node const* current = this;

  for (std::string const& key : pv) {
    auto children = std::get_if<Children>(&_value);

    if (children == nullptr) {
      return nullptr;
    }

    if (auto const child = children->find(key); child == children->end()) {
      return nullptr;
    } else {
      current = child->second.get();
    }
  }

  return current;
}

Node const* Node::get(
    std::shared_ptr<cluster::paths::Path const> const& path) const {
  return get(path->vec(cluster::paths::SkipComponents{1}));
}

/// @brief rh-value at path
Node const* Node::get(std::string_view path) const { return get(split(path)); }

#if 0
using namespace arangodb;

/// Set value
template<>
ResultT<std::shared_ptr<Node>> Node::handle<SET>(VPackSlice const& slice) {
  using namespace std::chrono;

  Slice val = slice.get("new");
  if (val.isNone()) {
    // key "new" not present
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Operator set without new value: ") + slice.toJson());
  }

  if (val.isObject()) {
    if (val.hasKey("op")) {  // No longer a keyword but a regular key "op"
      if (_children == nullptr) {
        _children = std::make_unique<Children>();
      }
      if (_children->find("op") == _children->end()) {
        (*_children)["op"] = std::make_shared<Node>("op", this);
      }
      *((*_children)["op"]) = val.get("op");
    } else {  // Deeper down
      this->applies(val);
    }
  } else {
    *this = val;
  }
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Increment integer value or set 1
template<>
ResultT<std::shared_ptr<Node>> Node::handle<INCREMENT>(
    VPackSlice const& slice) {
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
template<>
ResultT<std::shared_ptr<Node>> Node::handle<DECREMENT>(
    VPackSlice const& slice) {
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
template<>
ResultT<std::shared_ptr<Node>> Node::handle<PUSH>(VPackSlice const& slice) {
  VPackSlice v = slice.get("new");
  if (v.isNone()) {
    // key "new" not present
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Operator push without new value: ") + slice.toJson());
  }
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
    if (this->slice().isArray()) {
      for (auto const& old : VPackArrayIterator(this->slice())) tmp.add(old);
    }
    tmp.add(v);
  }
  *this = tmp.slice();
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Remove element from any place in array by value or position
template<>
ResultT<std::shared_ptr<Node>> Node::handle<ERASE>(VPackSlice const& slice) {
  bool haveVal = slice.hasKey("val");
  bool havePos = slice.hasKey("pos");

  if (!haveVal && !havePos) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string(
            "Erase without value or position to be erased is illegal: ") +
            slice.toJson());
  } else if (haveVal && havePos) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string(
            "Erase without value or position to be erased is illegal: ") +
            slice.toJson());
  } else if (havePos &&
             (!slice.get("pos").isUInt() && !slice.get("pos").isSmallInt())) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Erase with non-positive integer position is illegal: ") +
            slice.toJson());
  }

  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);

    if (this->slice().isArray()) {
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
              TRI_ERROR_FAILED,
              std::string("Erase with position out of range: ") +
                  slice.toJson());
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

/// Push to end while keeping a specified length
template<>
ResultT<std::shared_ptr<Node>> Node::handle<PUSH_QUEUE>(
    VPackSlice const& slice) {
  VPackSlice v = slice.get("new");
  if (VPackSlice l = slice.get("len"); l.isInteger()) {
    if (v.isNone()) {
      // key "new" not present
      return ResultT<std::shared_ptr<Node>>::error(
          TRI_ERROR_FAILED,
          std::string("Operator push-queue without new value: ") +
              slice.toJson());
    }
    Builder tmp;
    {
      VPackArrayBuilder t(&tmp);
      auto ol = l.getNumber<int64_t>();
      if (this->slice().isArray()) {
        auto tl = this->slice().length();
        if (ol < 0) {
          return ResultT<std::shared_ptr<Node>>::error(
              TRI_ERROR_FAILED,
              std::string(
                  "Operator push-queue expects a len greater than 0: ") +
                  slice.toJson());
        }
        if (tl >= uint64_t(ol)) {
          for (size_t i = tl - ol + 1; i < tl; ++i) {
            tmp.add(this->slice()[i]);
          }
        } else {
          for (auto const& old : VPackArrayIterator(this->slice())) {
            tmp.add(old);
          }
        }
      }
      if (ol > 0) {
        tmp.add(v);
      }
    }
    *this = tmp.slice();
    return ResultT<std::shared_ptr<Node>>::success(nullptr);
  } else {
    // key "len" not present or not integer
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Operator push-queue without integer value len: ") +
            slice.toJson());
  }
}

/// Replace element from any place in array by new value
template<>
ResultT<std::shared_ptr<Node>> Node::handle<REPLACE>(VPackSlice const& slice) {
  if (!slice.hasKey("val")) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Operator erase without value to be erased: ") +
            slice.toJson());
  }
  if (!slice.hasKey("new")) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Operator replace without new value: ") + slice.toJson());
  }
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
    if (this->slice().isArray()) {
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
template<>
ResultT<std::shared_ptr<Node>> Node::handle<POP>(VPackSlice const& slice) {
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
    if (this->slice().isArray()) {
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
template<>
ResultT<std::shared_ptr<Node>> Node::handle<PREPEND>(VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Operator prepend without new value: ") + slice.toJson());
  }
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
    tmp.add(slice.get("new"));
    if (this->slice().isArray()) {
      for (auto const& old : VPackArrayIterator(this->slice())) tmp.add(old);
    }
  }
  *this = tmp.slice();
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

/// Remove element from front of array
template<>
ResultT<std::shared_ptr<Node>> Node::handle<SHIFT>(VPackSlice const& slice) {
  Builder tmp;
  {
    VPackArrayBuilder t(&tmp);
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
  return ResultT<std::shared_ptr<Node>>::success(nullptr);
}

template<>
ResultT<std::shared_ptr<Node>> Node::handle<READ_LOCK>(
    VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED, std::string("Invalid read lock: ") + slice.toJson());
  }

  if (isReadLockable(user.stringView())) {
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

template<>
ResultT<std::shared_ptr<Node>> Node::handle<READ_UNLOCK>(
    VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Invalid read unlock: ") + slice.toJson());
  }

  if (isReadUnlockable(user.stringView())) {
    Builder newValue;
    {
      // isReadUnlockable ensured that `this->slice()` is always an array of
      // strings
      VPackArrayBuilder arr(&newValue);
      for (auto const& i : VPackArrayIterator(this->slice())) {
        if (!i.isEqualString(user.stringView())) {
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

  return ResultT<std::shared_ptr<Node>>::error(TRI_ERROR_FAILED,
                                               "Read unlock failed");
}

template<>
ResultT<std::shared_ptr<Node>> Node::handle<WRITE_LOCK>(
    VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Invalid write unlock: ") + slice.toJson());
  }

  if (isWriteLockable(user.stringView())) {
    this->operator=(user);
    return ResultT<std::shared_ptr<Node>>::success(nullptr);
  }
  return ResultT<std::shared_ptr<Node>>::error(TRI_ERROR_LOCKED);
}

template<>
ResultT<std::shared_ptr<Node>> Node::handle<WRITE_UNLOCK>(
    VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return ResultT<std::shared_ptr<Node>>::error(
        TRI_ERROR_FAILED,
        std::string("Invalid write unlock: ") + slice.toJson());
  }

  if (isWriteUnlockable(user.stringView())) {
    return deleteMe();
  }

  return ResultT<std::shared_ptr<Node>>::error(TRI_ERROR_FAILED,
                                               "Write unlock failed");
}



arangodb::ResultT<std::shared_ptr<Node>> Node::applyOp(VPackSlice slice) {
  std::string_view oper = slice.get("op").stringView();

  if (oper == "delete") {
    return deleteMe();
  } else if (oper == "set") {         // "op":"set"
    return handle<SET>(slice);
  } else if (oper == "increment") {   // "op":"increment"
    return handle<INCREMENT>(slice);
  } else if (oper == "decrement") {   // "op":"decrement"
    return handle<DECREMENT>(slice);
  } else if (oper == "push") {        // "op":"push"
    return handle<PUSH>(slice);
  } else if (oper == "push-queue") {  // "op":"push-queue"
    return handle<PUSH_QUEUE>(slice);
  } else if (oper == "pop") {         // "op":"pop"
    return handle<POP>(slice);
  } else if (oper == "prepend") {     // "op":"prepend"
    return handle<PREPEND>(slice);
  } else if (oper == "shift") {       // "op":"shift"
    return handle<SHIFT>(slice);
  } else if (oper == "erase") {       // "op":"erase"
    return handle<ERASE>(slice);
  } else if (oper == "replace") {     // "op":"replace"
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
      std::string("Unknown operation '") + std::string(oper) + "'");
}

// Apply slice to this node
bool Node::applies(VPackSlice slice) {
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
        getOrCreate(key).applies(i.value);
      } else {
        if (_children == nullptr) {
          _children = std::make_unique<Children>();
        }
        auto found = _children->find(key);
        if (found == _children->end()) {
          found =
              _children->emplace(key, std::make_shared<Node>(key, this)).first;
        }
        (*found).second->applies(i.value);
      }
    }
  } else {
    *this = slice;
  }

  return true;
}
#endif

bool Node::isReadLockable(std::string_view by) const {
  // the following states are counted as readLockable
  // array - when read locked or read lock released
  // empty object - when the node is created
  return std::visit(
      overload{[&](Children const& ch) { return ch.empty(); },
               [&](Array const& ar) {
                 return std::all_of(
                     ar.begin(), ar.end(), [&](auto const& slice) {
                       return slice.isString() && !slice.isEqualString(by);
                     });
               },
               [](auto const&) { return false; }},
      _value);
}

bool Node::isReadUnlockable(std::string_view by) const {
  // the following states are counted as readUnlockable
  // array of strings containing the value `by`
  return std::visit(
      overload{[&](Array const& ar) {
                 return std::any_of(
                     ar.begin(), ar.end(), [&](auto const& slice) {
                       return slice.isString() && slice.isEqualString(by);
                     });
               },
               [](auto const&) { return false; }},
      _value);
}

bool Node::isWriteLockable(std::string_view) const {
  return std::visit(overload{[&](Children const& ch) { return ch.empty(); },
                             [](auto const&) { return false; }},
                    _value);
}

bool Node::isWriteUnlockable(std::string_view by) const {
  return std::visit(overload{[&](VPackString const& slice) {
                               return slice.isString() &&
                                      slice.isEqualString(by);
                             },
                             [](auto const&) { return false; }},
                    _value);
}

void Node::toBuilder(Builder& builder, bool showHidden) const {
  std::visit(overload{[&](Children const& ch) {
                        VPackObjectBuilder ob(&builder);
                        for (auto const& [key, node] : ch) {
                          if (key[0] == '.' && !showHidden) {
                            continue;
                          }
                          builder.add(VPackValue(key));
                          node->toBuilder(builder, showHidden);
                        }
                      },
                      [&](Array const& ar) {
                        VPackArrayBuilder ab(&builder);
                        for (auto const& slice : ar) {
                          builder.add(slice);
                        }
                      },
                      [&](VPackString const& slice) { builder.add(slice); }},
             _value);
}

// Print internals to ostream
std::ostream& Node::print(std::ostream& o) const {
  Builder builder;
  toBuilder(builder);
  o << builder.toJson();
  return o;
}

Node::Children const& Node::children() const {
  if (auto children = std::get_if<Children>(&_value); children) {
    return *children;
  }
  return dummyChildren;
}

Builder Node::toBuilder() const {
  Builder builder;
  toBuilder(builder);
  return builder;
}

std::string Node::toJson() const { return toBuilder().toJson(); }

std::vector<std::string> Node::exists(
    std::vector<std::string> const& rel) const {
  std::vector<std::string> result;
  Node const* cur = this;
  for (auto const& sub : rel) {
    auto it = cur->children().find(sub);
    if (it != cur->children().end()) {
      cur = it->second.get();
      result.push_back(sub);
    } else {
      break;
    }
  }
  return result;
}

std::vector<std::string> Node::exists(std::string_view rel) const {
  // cppcheck-suppress returnDanglingLifetime
  return exists(split(rel));
}

bool Node::has(std::vector<std::string> const& rel) const {
  return exists(rel).size() == rel.size();
}

bool Node::has(std::string_view rel) const { return has(split(rel)); }

std::optional<int64_t> Node::getInt() const noexcept {
  if (auto slice = std::get_if<VPackString>(&_value); slice && slice->isInt()) {
    return slice->getInt();
  }
  return std::nullopt;
}

std::optional<uint64_t> Node::getUInt() const noexcept {
  if (auto slice = std::get_if<VPackString>(&_value);
      slice && slice->isUInt()) {
    return slice->getUInt();
  }
  return std::nullopt;
}

std::optional<bool> Node::getBool() const noexcept {
  if (auto slice = std::get_if<VPackString>(&_value);
      slice && slice->isBool()) {
    return slice->getBool();
  }
  return std::nullopt;
}

VPackSlice Node::slice() const noexcept {
  auto slice = std::get_if<VPackString>(&_value);
  return slice->slice();
}

bool Node::isBool() const {
  auto slice = std::get_if<VPackString>(&_value);
  return slice && slice->isBool();
}

bool Node::isDouble() const {
  auto slice = std::get_if<VPackString>(&_value);
  return slice && slice->isDouble();
}

bool Node::isString() const {
  auto slice = std::get_if<VPackString>(&_value);
  return slice && slice->isString();
}

bool Node::isArray() const { return std::holds_alternative<Array>(_value); }

bool Node::isObject() const { return std::holds_alternative<Children>(_value); }

bool Node::isUInt() const {
  auto slice = std::get_if<VPackString>(&_value);
  return slice && (slice->isUInt() || slice->isSmallInt());
}

bool Node::isInt() const {
  auto slice = std::get_if<VPackString>(&_value);
  return slice && (slice->isInt() || slice->isSmallInt());
}

bool Node::isNumber() const {
  auto slice = std::get_if<VPackString>(&_value);
  return slice && slice->isNumber();
}

std::optional<double> Node::getDouble() const noexcept {
  if (auto slice = std::get_if<VPackString>(&_value);
      slice && slice->isDouble()) {
    return slice->getDouble();
  }
  return std::nullopt;
}

Node const* Node::hasAsNode(std::string const& url) const noexcept {
  // retrieve node, throws if does not exist
  return get(url);
}  // hasAsNode

std::optional<Slice> Node::hasAsSlice(std::string const& url) const noexcept {
  if (auto slice = std::get_if<VPackString>(&_value); slice) {
    return *slice;
  }
  return std::nullopt;
}  // hasAsSlice

std::optional<uint64_t> Node::hasAsUInt(std::string const& url) const noexcept {
  if (auto node = get(url); node) {
    return node->getUInt();
  }

  return std::nullopt;
}  // hasAsUInt

std::optional<bool> Node::hasAsBool(std::string const& url) const noexcept {
  if (auto node = get(url); node) {
    return node->getBool();
  }

  return std::nullopt;
}  // hasAsBool

std::optional<std::string> Node::hasAsString(std::string const& url) const {
  if (auto node = get(url); node) {
    return node->getString();
  }

  return std::nullopt;
}  // hasAsString

Node::Children const* Node::hasAsChildren(std::string const& url) const {
  if (auto node = get(url); node) {
    return &node->children();
  }

  return nullptr;
}  // hasAsChildren

bool Node::hasAsBuilder(std::string const& url, Builder& builder,
                        bool showHidden) const {
  if (auto node = get(url); node) {
    node->toBuilder(builder, showHidden);
    return true;
  }

  return false;
}  // hasAsBuilder

std::optional<Builder> Node::hasAsBuilder(std::string const& url) const {
  if (auto node = get(url); node) {
    Builder builder;
    node->toBuilder(builder);
    return builder;
  }

  return std::nullopt;
}  // hasAsBuilder

Node::Array const* Node::hasAsArray(std::string const& url) const {
  if (auto node = get(url); node) {
    return node->getArray();
  }

  return nullptr;
}  // hasAsArray

std::optional<std::string> Node::getString() const {
  if (auto slice = std::get_if<VPackString>(&_value);
      slice && slice->isString()) {
    return slice->copyString();
  }
  return std::nullopt;
}

std::optional<std::string_view> Node::getStringView() const {
  if (auto slice = std::get_if<VPackString>(&_value);
      slice && slice->isString()) {
    return slice->stringView();
  }
  return std::nullopt;
}

Node::Array const* Node::getArray() const {
  return std::get_if<Array>(&_value);
}

void Node::clear() { _value.emplace<Children>(); }

std::vector<std::string> Node::keys() const {
  std::vector<std::string> result;
  std::visit(overload{[&result](Children const& c) {
                        result.reserve(c.size());
                        for (auto const& [key, value] : c) {
                          result.emplace_back(key);
                        }
                      },
                      [](auto const&) {}},
             _value);
  return result;
}

auto Node::getIntWithDefault(Slice slice, std::string_view key,
                             std::int64_t def) -> std::int64_t {
  return arangodb::basics::VelocyPackHelper::getNumericValue<std::int64_t>(
      slice, key, def);
}
