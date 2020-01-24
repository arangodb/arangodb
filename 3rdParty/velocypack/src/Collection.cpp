////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <unordered_map>

#include "velocypack/velocypack-common.h"
#include "velocypack/Collection.h"
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"

using namespace arangodb::velocypack;

// indicator for "element not found" in indexOf() method
ValueLength const Collection::NotFound = UINT64_MAX;
  
// fully append an array to the builder
Builder& Collection::appendArray(Builder& builder, Slice const& slice) {
  ArrayIterator it(slice);

  while (it.valid()) {
    builder.add(it.value());
    it.next();
  }

  return builder;
}

// convert a vector of strings into an unordered_set of strings
static inline std::unordered_set<std::string> makeSet(
    std::vector<std::string> const& keys) {
  std::unordered_set<std::string> s;
  for (auto const& it : keys) {
    s.emplace(it);
  }
  return s;
}

void Collection::forEach(Slice const& slice, Predicate const& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    if (!predicate(it.value(), index)) {
      // abort
      return;
    }
    it.next();
    ++index;
  }
}

Builder Collection::filter(Slice const& slice, Predicate const& predicate) {
  // construct a new Array
  Builder b;
  b.add(Value(ValueType::Array));

  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (predicate(s, index)) {
      b.add(s);
    }
    it.next();
    ++index;
  }
  b.close();
  return b;
}

Slice Collection::find(Slice const& slice, Predicate const& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (predicate(s, index)) {
      return s;
    }
    it.next();
    ++index;
  }

  return Slice();
}

bool Collection::contains(Slice const& slice, Predicate const& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (predicate(s, index)) {
      return true;
    }
    it.next();
    ++index;
  }

  return false;
}

bool Collection::contains(Slice const& slice, Slice const& other) {
  ArrayIterator it(slice);

  while (it.valid()) {
    if (it.value().binaryEquals(other)) {
      return true;
    }
    it.next();
  }

  return false;
}

ValueLength Collection::indexOf(Slice const& slice, Slice const& other) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    if (it.value().binaryEquals(other)) {
      return index;
    }
    it.next();
    ++index;
  }

  return Collection::NotFound;
}

bool Collection::all(Slice const& slice, Predicate const& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (!predicate(s, index)) {
      return false;
    }
    it.next();
    ++index;
  }

  return true;
}

bool Collection::any(Slice const& slice, Predicate const& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (predicate(s, index)) {
      return true;
    }
    it.next();
    ++index;
  }

  return false;
}

std::vector<std::string> Collection::keys(Slice const& slice) {
  std::vector<std::string> result;

  keys(slice, result);

  return result;
}

Builder Collection::concat(Slice const& slice1, Slice const& slice2) {
  Builder b;
  b.openArray();
  appendArray(b, slice1);
  appendArray(b, slice2);
  b.close();

  return b;
}

Builder Collection::extract(Slice const& slice, int64_t from, int64_t to) {
  Builder b;
  b.openArray();

  int64_t length = static_cast<int64_t>(slice.length());
  int64_t skip = from;
  int64_t limit = to;

  if (limit < 0) {
    limit = length + limit - skip;
  }
  if (limit > 0) {
    ArrayIterator it(slice);
    while (it.valid()) {
      if (skip > 0) {
        --skip;
      }
      else {
        b.add(it.value());
        if (--limit == 0) {
          break;
        }
      }
      it.next();
    }
  }
  b.close();

  return b;
}

Builder Collection::values(Slice const& slice) {
  Builder b;
  b.add(Value(ValueType::Array));

  ObjectIterator it(slice);

  while (it.valid()) {
    b.add(it.value());
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::keep(Slice const& slice,
                         std::vector<std::string> const& keys) {
  // check if there are so many keys that we want to use the hash-based version
  // cut-off values are arbitrary...
  if (keys.size() >= 4 && slice.length() > 10) {
    return keep(slice, makeSet(keys));
  }

  Builder b;
  b.add(Value(ValueType::Object));

  ObjectIterator it(slice);

  while (it.valid()) {
    auto key = it.key(true).copyString();
    if (std::find(keys.begin(), keys.end(), key) != keys.end()) {
      b.add(key, it.value());
    }
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::keep(Slice const& slice,
                         std::unordered_set<std::string> const& keys) {
  Builder b;
  b.add(Value(ValueType::Object));

  ObjectIterator it(slice);

  while (it.valid()) {
    auto key = it.key(true).copyString();
    if (keys.find(key) != keys.end()) {
      b.add(key, it.value());
    }
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::remove(Slice const& slice,
                           std::vector<std::string> const& keys) {
  // check if there are so many keys that we want to use the hash-based version
  // cut-off values are arbitrary...
  if (keys.size() >= 4 && slice.length() > 10) {
    return remove(slice, makeSet(keys));
  }

  Builder b;
  b.add(Value(ValueType::Object));

  ObjectIterator it(slice);

  while (it.valid()) {
    auto key = it.key(true).copyString();
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
      b.add(key, it.value());
    }
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::remove(Slice const& slice,
                           std::unordered_set<std::string> const& keys) {
  Builder b;
  b.add(Value(ValueType::Object));

  ObjectIterator it(slice);

  while (it.valid()) {
    auto key = it.key(true).copyString();
    if (keys.find(key) == keys.end()) {
      b.add(key, it.value());
    }
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::merge(Slice const& left, Slice const& right,
                          bool mergeValues, bool nullMeansRemove) {
  if (!left.isObject() || !right.isObject()) {
    throw Exception(Exception::InvalidValueType, "Expecting type Object");
  }

  Builder b;
  Collection::merge(b, left, right, mergeValues, nullMeansRemove);
  return b;
}

Builder& Collection::merge(Builder& builder, Slice const& left, Slice const& right,
                           bool mergeValues, bool nullMeansRemove) {
  if (!left.isObject() || !right.isObject()) {
    throw Exception(Exception::InvalidValueType, "Expecting type Object");
  }

  builder.add(Value(ValueType::Object));

  std::unordered_map<StringRef, Slice> rightValues;
  {
    ObjectIterator it(right);
    while (it.valid()) {
      auto current = (*it);
      rightValues.emplace(current.key.stringRef(), current.value);
      it.next();
    }
  }

  {
    ObjectIterator it(left);

    while (it.valid()) {
      auto current = (*it);
      auto key = current.key.stringRef();
      auto found = rightValues.find(key);

      if (found == rightValues.end()) {
        // use left value
        builder.add(key, current.value);
      } else if (mergeValues && current.value.isObject() &&
                 (*found).second.isObject()) {
        // merge both values
        auto& value = (*found).second;
        if (!nullMeansRemove || (!value.isNone() && !value.isNull())) {
          builder.add(ValuePair(key, ValueType::String));
          Collection::merge(builder, current.value, value, true, nullMeansRemove);
        }
        // clear the value in the map so its not added again
        (*found).second = Slice();
      } else {
        // use right value
        auto& value = (*found).second;
        if (!nullMeansRemove || (!value.isNone() && !value.isNull())) {
          builder.add(key, value);
        }
        // clear the value in the map so its not added again
        (*found).second = Slice();
      }
      it.next();
    }
  }

  // add remaining values that were only in right
  for (auto& it : rightValues) {
    auto& s = it.second;
    if (s.isNone()) {
      continue;
    }
    if (nullMeansRemove && s.isNull()) {
      continue;
    }
    builder.add(std::move(it.first), s);
  }

  builder.close();
  return builder;
}

template <Collection::VisitationOrder order>
static bool doVisit(
    Slice const& slice,
    std::function<bool(Slice const& key, Slice const& value)> const& func);

template <Collection::VisitationOrder order>
static bool visitObject(
    Slice const& value,
    std::function<bool(Slice const& key, Slice const& value)> const& func) {
  ObjectIterator it(value);

  while (it.valid()) {
    auto current = (*it);
    // sub-object?
    Slice v = current.value;
    bool const isCompound = (v.isObject() || v.isArray());

    if (isCompound && order == Collection::PreOrder) {
      if (!doVisit<order>(v, func)) {
        return false;
      }
    }

    if (!func(current.key, v)) {
      return false;
    }

    if (isCompound && order == Collection::PostOrder) {
      if (!doVisit<order>(v, func)) {
        return false;
      }
    }

    it.next();
  }
  return true;
}

template <Collection::VisitationOrder order>
static bool visitArray(
    Slice const& value,
    std::function<bool(Slice const& key, Slice const& value)> const& func) {
  ArrayIterator it(value);

  while (it.valid()) {
    // sub-object?
    Slice v = it.value();
    bool const isCompound = (v.isObject() || v.isArray());

    if (isCompound && order == Collection::PreOrder) {
      if (!doVisit<order>(v, func)) {
        return false;
      }
    }

    if (!func(Slice(), v)) {
      return false;
    }

    if (isCompound && order == Collection::PostOrder) {
      if (!doVisit<order>(v, func)) {
        return false;
      }
    }

    it.next();
  }

  return true;
}

template <Collection::VisitationOrder order>
static bool doVisit(
    Slice const& slice,
    std::function<bool(Slice const& key, Slice const& value)> const& func) {
  if (slice.isObject()) {
    return visitObject<order>(slice, func);
  }
  if (slice.isArray()) {
    return visitArray<order>(slice, func);
  }

  throw Exception(Exception::InvalidValueType,
                  "Expecting type Object or Array");
}

void Collection::visitRecursive(
    Slice const& slice, Collection::VisitationOrder order,
    std::function<bool(Slice const&, Slice const&)> const& func) {
  if (order == Collection::PreOrder) {
    doVisit<Collection::PreOrder>(slice, func);
  } else {
    doVisit<Collection::PostOrder>(slice, func);
  }
}

Builder Collection::sort(
      Slice const& array,
      std::function<bool (Slice const&, Slice const&)> lessthan) {
  if (!array.isArray()) {
    throw Exception(Exception::InvalidValueType, "Expecting type Array");
  }
  std::vector<Slice> subValues;
  ValueLength len = array.length();
  subValues.reserve(checkOverflow(len));
  for (ValueLength i = 0; i < len; i++) {
    subValues.push_back(array[i]);
  }
  std::sort(subValues.begin(), subValues.end(), lessthan);
  Builder b;
  b.openArray();
  for (auto const&s : subValues) {
    b.add(s);
  }
  b.close();
  return b;
}

