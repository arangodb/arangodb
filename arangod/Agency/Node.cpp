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
#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/finder.hpp>

using namespace arangodb::consensus;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb;

const Node::Children Node::dummyChildren = Node::Children();
const Node Node::_dummyNode;

Node::Node(VariantType value) noexcept : _value(std::move(value)) {}

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

/// @brief rh-value at path vector. Check if TTL has expired.
NodePtr Node::get(std::vector<std::string> const& pv) const {
  NodePtr current = shared_from_this();
  for (std::string const& key : pv) {
    auto children = std::get_if<Children>(&current->_value);

    if (children == nullptr) {
      return nullptr;
    }

    if (auto const child = children->find(key); child == children->end()) {
      return nullptr;
    } else {
      current = child->second;
    }
  }

  return current;
}

NodePtr Node::get(
    std::shared_ptr<cluster::paths::Path const> const& path) const {
  return get(path->vec(cluster::paths::SkipComponents{1}));
}

/// @brief rh-value at path
NodePtr Node::get(std::string_view path) const { return get(split(path)); }

/// Set value
template<>
ResultT<NodePtr> Node::handle<SET>(Node const* target,
                                   VPackSlice const& slice) {
  using namespace std::chrono;

  Slice val = slice.get("new");
  if (val.isNone()) {
    // key "new" not present
    return Result(
        TRI_ERROR_FAILED,
        std::string("Operator set without new value: ") + slice.toJson());
  }

  return Node::create(val);
}

/// Increment integer value or set 1
template<>
ResultT<NodePtr> Node::handle<INCREMENT>(Node const* target,
                                         VPackSlice const& slice) {
  auto inc = arangodb::basics::VelocyPackHelper::getNumericValue<std::int64_t>(
      slice, "step", 1);
  auto pre = target ? target->getInt().value_or(0) : 0;

  Builder tmp;
  {
    VPackObjectBuilder t(&tmp);
    tmp.add("tmp", Value(pre + inc));
  }
  return Node::create(tmp.slice().get("tmp"));
}

/// Decrement integer value or set -1
template<>
ResultT<NodePtr> Node::handle<DECREMENT>(Node const* target,
                                         VPackSlice const& slice) {
  auto dec = arangodb::basics::VelocyPackHelper::getNumericValue<std::int64_t>(
      slice, "step", 1);
  auto pre = target ? target->getInt().value_or(0) : 0;

  Builder tmp;
  {
    VPackObjectBuilder t(&tmp);
    tmp.add("tmp", Value(pre - dec));
  }
  return Node::create(tmp.slice().get("tmp"));
}

/// Append element to array
template<>
ResultT<NodePtr> Node::handle<PUSH>(Node const* target,
                                    VPackSlice const& slice) {
  VPackSlice v = slice.get("new");
  if (v.isNone()) {
    // key "new" not present
    return Result(
        TRI_ERROR_FAILED,
        std::string("Operator push without new value: ") + slice.toJson());
  }
  Node::Array array;
  if (target && target->isArray()) {
    array = *target->getArray();
  }

  array.emplace_back(v);
  return std::make_shared<Node>(std::move(array));
}

/// Remove element from any place in array by value or position
template<>
ResultT<NodePtr> Node::handle<ERASE>(Node const* target,
                                     VPackSlice const& slice) {
  VPackSlice valToErase = slice.get("val");
  Slice posSlice = slice.get("pos");
  bool havePos = !posSlice.isNone();
  bool haveVal = !valToErase.isNone();

  if (!haveVal && !havePos) {
    return Result(
        TRI_ERROR_FAILED,
        std::string(
            "Erase without value or position to be erased is illegal: ") +
            slice.toJson());
  } else if (haveVal && havePos) {
    return Result(
        TRI_ERROR_FAILED,
        std::string(
            "Erase without value or position to be erased is illegal: ") +
            slice.toJson());
  } else if (havePos &&
             (!slice.get("pos").isUInt() && !slice.get("pos").isSmallInt())) {
    return Result(
        TRI_ERROR_FAILED,
        std::string("Erase with non-positive integer position is illegal: ") +
            slice.toJson());
  }

  Node::Array array;
  if (target && target->isArray()) {
    array = *target->getArray();
  }

  if (haveVal) {
    array.erase(std::remove_if(array.begin(), array.end(),
                               [&](auto const& slice) {
                                 return VelocyPackHelper::equal(
                                     slice, valToErase,
                                     /*useUTF8*/ true);
                               }),
                array.end());
  } else {
    size_t pos = posSlice.getNumber<size_t>();
    if (pos < array.size()) {
      array.erase(array.begin() + pos);
    }
  }
  return std::make_shared<Node>(std::move(array));
}

/// Push to end while keeping a specified length
template<>
ResultT<NodePtr> Node::handle<PUSH_QUEUE>(Node const* target,
                                          VPackSlice const& slice) {
  VPackSlice v = slice.get("new");
  VPackSlice l = slice.get("len");
  if (v.isNone()) {
    // key "new" not present
    return Result(TRI_ERROR_FAILED,
                  std::string("Operator push-queue without new value: ") +
                      slice.toJson());
  }
  if (l.isInteger()) {
    // key "len" not present or not integer
    return Result(
        TRI_ERROR_FAILED,
        std::string("Operator push-queue without integer value len: ") +
            slice.toJson());
  }

  auto ol = l.getNumber<uint64_t>();

  Node::Array array;
  if (ol == 0) {
    return std::make_shared<Node>(array);
  }
  array.reserve(ol);
  array.emplace_back(v);
  if (target && target->isArray()) {
    auto other = target->getArray();
    std::copy_n(other->begin(), std::min(ol - 1, other->size()),
                std::back_inserter(array));
  }
  return std::make_shared<Node>(std::move(array));
}

/// Replace element from any place in array by new value
template<>
ResultT<NodePtr> Node::handle<REPLACE>(Node const* target,
                                       VPackSlice const& slice) {
  if (!slice.hasKey("val")) {
    return Result(TRI_ERROR_FAILED,
                  std::string("Operator erase without value to be erased: ") +
                      slice.toJson());
  }
  if (!slice.hasKey("new")) {
    return Result(
        TRI_ERROR_FAILED,
        std::string("Operator replace without new value: ") + slice.toJson());
  }
  Array array;
  if (target && target->isArray()) {
    array = *target->getArray();
  }
  VPackSlice valToRepl = slice.get("val");
  std::replace_if(
      array.begin(), array.end(),
      [&](auto const& old) {
        return VelocyPackHelper::equal(old, valToRepl, /*useUTF8*/ true);
      },
      slice.get("new"));

  return std::make_shared<Node>(std::move(array));
}

/// Remove element from end of array.
template<>
ResultT<NodePtr> Node::handle<POP>(Node const* target,
                                   VPackSlice const& slice) {
  Array array;
  if (target && target->isArray()) {
    array = *target->getArray();
  }
  if (!array.empty()) {
    array.erase(array.rbegin().base());
  }
  return std::make_shared<Node>(std::move(array));
}

/// Prepend element to array
template<>
ResultT<NodePtr> Node::handle<PREPEND>(Node const* target,
                                       VPackSlice const& slice) {
  if (!slice.hasKey("new")) {
    return Result(
        TRI_ERROR_FAILED,
        std::string("Operator prepend without new value: ") + slice.toJson());
  }
  Array array;
  if (target && target->isArray()) {
    array = *target->getArray();
  }
  if (!array.empty()) {
    array.insert(array.begin(), slice.get("new"));
  }
  return std::make_shared<Node>(std::move(array));
}

/// Remove element from front of array
template<>
ResultT<NodePtr> Node::handle<SHIFT>(Node const* target,
                                     VPackSlice const& slice) {
  Array array;
  if (target && target->isArray()) {
    array = *target->getArray();
  }
  if (!array.empty()) {
    array.erase(array.begin());
  }
  return std::make_shared<Node>(std::move(array));
}

template<>
ResultT<NodePtr> Node::handle<READ_LOCK>(Node const* target,
                                         VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return Result(TRI_ERROR_FAILED,
                  std::string("Invalid read lock: ") + slice.toJson());
  }

  if (target && !target->isReadLockable(user.stringView())) {
    return {TRI_ERROR_LOCKED};
  }

  Node::Array array;
  if (target && target->isArray()) {
    array = *target->getArray();
  }
  array.emplace_back(user);
  return std::make_shared<Node>(std::move(array));
}

template<>
ResultT<NodePtr> Node::handle<READ_UNLOCK>(Node const* target,
                                           VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return Result(TRI_ERROR_FAILED,
                  std::string("Invalid read unlock: ") + slice.toJson());
  }

  if (target && target->isReadUnlockable(user.stringView())) {
    Node::Array array;
    {
      // isReadUnlockable ensured that `this->slice()` is always an array of
      // strings
      for (auto const& i : *target->getArray()) {
        if (!i.isEqualString(user.stringView())) {
          array.emplace_back(i);
        }
      }
    }
    if (array.empty()) {
      return nullptr;
    } else {
      return std::make_unique<Node>(std::move(array));
    }
  }

  return Result(TRI_ERROR_FAILED, "Read unlock failed");
}

template<>
ResultT<NodePtr> Node::handle<WRITE_LOCK>(Node const* target,
                                          VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return Result{TRI_ERROR_FAILED,
                  std::string("Invalid write unlock: ") + slice.toJson()};
  }

  if (!target || target->isWriteLockable(user.stringView())) {
    return Node::create(user);  // create a string node
  }
  return Result{TRI_ERROR_LOCKED};
}

template<>
ResultT<NodePtr> Node::handle<WRITE_UNLOCK>(Node const* target,
                                            VPackSlice const& slice) {
  Slice user = slice.get("by");
  if (!user.isString()) {
    return Result{TRI_ERROR_FAILED,
                  std::string("Invalid write unlock: ") + slice.toJson()};
  }

  if (!target || target->isWriteUnlockable(user.stringView())) {
    return nullptr;
  }

  return Result{TRI_ERROR_FAILED, "Write unlock failed"};
}

arangodb::ResultT<NodePtr> Node::applyOp(Node const* target, VPackSlice slice) {
  std::string_view oper = slice.get("op").stringView();

  if (oper == "delete") {
    return nullptr;
  } else if (oper == "set") {  // "op":"set"
    return handle<SET>(target, slice);
  } else if (oper == "increment") {  // "op":"increment"
    return handle<INCREMENT>(target, slice);
  } else if (oper == "decrement") {  // "op":"decrement"
    return handle<DECREMENT>(target, slice);
  } else if (oper == "push") {  // "op":"push"
    return handle<PUSH>(target, slice);
  } else if (oper == "push-queue") {  // "op":"push-queue"
    return handle<PUSH_QUEUE>(target, slice);
  } else if (oper == "pop") {  // "op":"pop"
    return handle<POP>(target, slice);
  } else if (oper == "prepend") {  // "op":"prepend"
    return handle<PREPEND>(target, slice);
  } else if (oper == "shift") {  // "op":"shift"
    return handle<SHIFT>(target, slice);
  } else if (oper == "erase") {  // "op":"erase"
    return handle<ERASE>(target, slice);
  } else if (oper == "replace") {  // "op":"replace"
    return handle<REPLACE>(target, slice);
  } else if (oper == OP_READ_LOCK) {
    return handle<READ_LOCK>(target, slice);
  } else if (oper == OP_READ_UNLOCK) {
    return handle<READ_UNLOCK>(target, slice);
  } else if (oper == OP_WRITE_LOCK) {
    return handle<WRITE_LOCK>(target, slice);
  } else if (oper == OP_WRITE_UNLOCK) {
    return handle<WRITE_UNLOCK>(target, slice);
  }

  return Result(TRI_ERROR_FAILED,
                std::string("Unknown operation '") + std::string(oper) + "'");
}

namespace {
template<typename Iter, typename F>
ResultT<NodePtr> buildPathAndExecute(Node const* node, Iter begin, Iter end,
                                     F&& func) {
  // skip empty path segments
  while (begin != end && boost::copy_range<std::string_view>(*begin).empty()) {
    ++begin;
  }
  if (begin == end) {
    // we reached our final node
    return std::invoke(std::forward<F>(func), node);
  } else {
    // dig deeper. If `node` is nullptr we have to create a new node with a
    // single child, otherwise lookup the child.
    auto key = boost::copy_range<std::string_view>(*begin);

    Node const* child = nullptr;
    if (node && node->isObject()) {
      auto const& children = node->children();
      if (auto iter = node->children().find(key); iter != children.end()) {
        child = iter->second.get();
      }
    }

    auto updated =
        buildPathAndExecute(child, ++begin, end, std::forward<F>(func));
    if (updated.fail()) {
      return updated;
    }

    Node::Children newChildren;
    if (node && node->isObject()) {
      newChildren = node->children();
    }

    if (updated == nullptr) {
      newChildren.erase(key);
    } else {
      newChildren[key] = std::move(updated.get());
    }
    return std::make_shared<Node>(std::move(newChildren));
  }
}

template<typename F>
ResultT<NodePtr> buildPathAndExecute(Node const* node, std::string_view path,
                                     F&& func) {
  static_assert(std::is_invocable_r_v<ResultT<NodePtr>, F, Node const*>);
  auto segments = boost::make_split_iterator(path, boost::first_finder("/"));
  auto res = buildPathAndExecute(node, segments, decltype(segments){},
                                 std::forward<F>(func));
  TRI_ASSERT(res.ok());
  return std::move(res.get());
}
}  // namespace

NodePtr Node::applies(std::string_view path, VPackSlice slice) const {
  auto res = buildPathAndExecute(
      this, path,
      [&](Node const*) -> ResultT<NodePtr> { return Node::create(slice); });
  TRI_ASSERT(res.ok());
  return std::move(res.get());
}

arangodb::ResultT<NodePtr> Node::applyOp(std::string_view path,
                                         VPackSlice slice) const {
  return buildPathAndExecute(this, path,
                             [&](Node const* n) -> ResultT<NodePtr> {
                               return Node::applyOp(n, slice);
                             });
}

NodePtr Node::placeAt(std::string_view path, NodePtr node) const {
  auto res = buildPathAndExecute(
      this, path, [&](Node const*) -> ResultT<NodePtr> { return node; });
  TRI_ASSERT(res.ok());
  return std::move(res.get());
}

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
  if (auto slice = std::get_if<VPackString>(&_value);
      slice && slice->isNumber<int64_t>()) {
    return slice->getNumber<int64_t>();
  }
  return std::nullopt;
}

std::optional<uint64_t> Node::getUInt() const noexcept {
  if (auto slice = std::get_if<VPackString>(&_value);
      slice && slice->isNumber<uint64_t>()) {
    return slice->getNumber<uint64_t>();
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

VPackString Node::slice() const noexcept {
  if (isObject()) {
    return VPackSlice ::emptyObjectSlice();
  }
  VPackBuilder builder = toBuilder();
  return builder.slice();
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
  if (auto node = get(url).get(); node) {
    return node;
  }

  return &dummyNode();
}  // hasAsNode

std::optional<String> Node::hasAsSlice(std::string const& url) const noexcept {
  if (auto node = get(url); node) {
    return node->slice();
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

NodePtr Node::create(VPackSlice slice) {
  if (slice.isObject()) {
    Children c;
    c.reserve(slice.length());
    for (auto const& [key, sub] : VPackObjectIterator(slice)) {
      c.emplace(key.copyString(), Node::create(sub));
    }
    return std::make_shared<Node>(std::move(c));
  } else if (slice.isArray()) {
    Array a;
    a.reserve(slice.length());
    for (auto const& elem : VPackArrayIterator(slice)) {
      a.emplace_back(elem);
    }
    return std::make_shared<Node>(std::move(a));
  } else {
    return std::make_shared<Node>(VPackString(slice));
  }
}

NodePtr Node::create() { return std::make_shared<Node>(); }
