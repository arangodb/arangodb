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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_COLLECTION_H
#define VELOCYPACK_COLLECTION_H 1

#include <functional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "velocypack/velocypack-common.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {

class Collection {
 public:
  enum VisitationOrder { PreOrder = 1, PostOrder = 2 };

  // indicator for "element not found" in indexOf() method
  static ValueLength const NotFound;

  typedef std::function<bool(Slice const&, ValueLength)> Predicate;

  Collection() = delete;
  Collection(Collection const&) = delete;
  Collection& operator=(Collection const&) = delete;

  static Builder& appendArray(Builder& builder, Slice const& left);

  static void forEach(Slice const& slice, Predicate const& predicate);

  static void forEach(Slice const* slice, Predicate const& predicate) {
    return forEach(*slice, predicate);
  }

  static Builder filter(Slice const& slice, Predicate const& predicate);

  static Builder filter(Slice const* slice, Predicate const& predicate) {
    return filter(*slice, predicate);
  }

  static Slice find(Slice const& slice, Predicate const& predicate);

  static Slice find(Slice const* slice, Predicate const& predicate) {
    return find(*slice, predicate);
  }

  static bool contains(Slice const& slice, Predicate const& predicate);

  static bool contains(Slice const* slice, Predicate const& predicate) {
    return contains(*slice, predicate);
  }

  static bool contains(Slice const& slice, Slice const& other);

  static bool contains(Slice const* slice, Slice const& other) {
    return contains(*slice, other);
  }

  static ValueLength indexOf(Slice const& slice, Slice const& other);

  static ValueLength indexOf(Slice const* slice, Slice const& other) {
    return indexOf(*slice, other);
  }

  static bool all(Slice const& slice, Predicate const& predicate);

  static bool all(Slice const* slice, Predicate const& predicate) {
    return all(*slice, predicate);
  }

  static bool any(Slice const& slice, Predicate const& predicate);

  static bool any(Slice const* slice, Predicate const& predicate) {
    return any(*slice, predicate);
  }

  static std::vector<std::string> keys(Slice const& slice);

  static std::vector<std::string> keys(Slice const* slice) {
    return keys(*slice);
  }
  
  template<typename T>
  static void keys(Slice const& slice, T& result) {
    ObjectIterator it(slice);

    while (it.valid()) {
      ValueLength l;
      char const* p = it.key(true).getString(l);
      result.emplace(p, l);
      it.next();
    }
  }
  
  template<typename T>  
  static void keys(Slice const* slice, T& result) {
    return keys(*slice, result);
  }
  
  static void keys(Slice const& slice, std::vector<std::string>& result) {
    // pre-allocate result vector
    ObjectIterator it(slice);
    result.reserve(checkOverflow(it.size()));

    while (it.valid()) {
      ValueLength l;
      char const* p = it.key(true).getString(l);
      result.emplace_back(p, l);
      it.next();
    }
  }

  template<typename T>
  static void unorderedKeys(Slice const& slice, T& result) {
    ObjectIterator it(slice, true);

    while (it.valid()) {
      ValueLength l;
      char const* p = it.key(true).getString(l);
      result.emplace(p, l);
      it.next();
    }
  }
  
  template<typename T>  
  static void unorderedKeys(Slice const* slice, T& result) {
    return unorderedKeys(*slice, result);
  }

  static Builder extract(Slice const& slice, int64_t from, int64_t to = INT64_MAX);
  static Builder extract(Slice const* slice, int64_t from, int64_t to = INT64_MAX) {
    return extract(*slice, from, to);
  }

  static Builder concat(Slice const& slice1, Slice const& slice2);
  static Builder concat(Slice const* slice1, Slice const* slice2) { 
    return concat(*slice1, *slice2); 
  }

  static Builder values(Slice const& slice);
  static Builder values(Slice const* slice) { return values(*slice); }

  static Builder keep(Slice const& slice, std::vector<std::string> const& keys);
  static Builder keep(Slice const& slice,
                      std::unordered_set<std::string> const& keys);

  static Builder keep(Slice const* slice,
                      std::vector<std::string> const& keys) {
    return keep(*slice, keys);
  }

  static Builder keep(Slice const* slice,
                      std::unordered_set<std::string> const& keys) {
    return keep(*slice, keys);
  }

  static Builder remove(Slice const& slice,
                        std::vector<std::string> const& keys);

  static Builder remove(Slice const& slice,
                        std::unordered_set<std::string> const& keys);

  static Builder remove(Slice const* slice,
                        std::vector<std::string> const& keys) {
    return remove(*slice, keys);
  }

  static Builder remove(Slice const* slice,
                        std::unordered_set<std::string> const& keys) {
    return remove(*slice, keys);
  }

  static Builder merge(Slice const& left, Slice const& right, bool mergeValues, bool nullMeansRemove = false);

  static Builder merge(Slice const* left, Slice const* right,
                       bool mergeValues, bool nullMeansRemove = false) {
    return merge(*left, *right, mergeValues, nullMeansRemove);
  }
  static Builder& merge(Builder& builder, Slice const& left, Slice const& right, bool mergeValues, bool nullMeansRemove = false);

  static void visitRecursive(
      Slice const& slice, VisitationOrder order,
      std::function<bool(Slice const&, Slice const&)> const& func);

  static void visitRecursive(
      Slice const* slice, VisitationOrder order,
      std::function<bool(Slice const&, Slice const&)> const& func) {
    visitRecursive(*slice, order, func);
  }

  static Builder sort(
      Slice const& array,
      std::function<bool (Slice const&, Slice const&)> lessthan);
};

struct IsEqualPredicate {
  IsEqualPredicate(Slice const& value) : value(value) {}
  bool operator()(Slice const& current, ValueLength) {
    return value.binaryEquals(current);
  }
  // compare value
  Slice const value;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
