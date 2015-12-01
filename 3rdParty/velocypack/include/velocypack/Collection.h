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

#ifndef VELOCYPACK_COLLECTION_H
#define VELOCYPACK_COLLECTION_H 1

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "velocypack/velocypack-common.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {

class Collection {
 public:
  enum VisitationOrder { PreOrder = 1, PostOrder = 2 };

  Collection() = delete;
  Collection(Collection const&) = delete;
  Collection& operator=(Collection const&) = delete;

  static void forEach(Slice const& slice,
                      std::function<bool(Slice const&, ValueLength)> const& cb);

  static void forEach(
      Slice const* slice,
      std::function<bool(Slice const&, ValueLength)> const& cb) {
    return forEach(*slice, cb);
  }

  static Builder filter(
      Slice const& slice,
      std::function<bool(Slice const&, ValueLength)> const& cb);

  static Builder filter(
      Slice const* slice,
      std::function<bool(Slice const&, ValueLength)> const& cb) {
    return filter(*slice, cb);
  }

  static Slice find(Slice const& slice,
                    std::function<bool(Slice const&, ValueLength)> const& cb);

  static Slice find(Slice const* slice,
                    std::function<bool(Slice const&, ValueLength)> const& cb) {
    return find(*slice, cb);
  }

  static bool contains(
      Slice const& slice,
      std::function<bool(Slice const&, ValueLength)> const& cb);

  static bool contains(
      Slice const* slice,
      std::function<bool(Slice const&, ValueLength)> const& cb) {
    return contains(*slice, cb);
  }

  static bool all(Slice const& slice,
                  std::function<bool(Slice const&, ValueLength)> const& cb);

  static bool all(Slice const* slice,
                  std::function<bool(Slice const&, ValueLength)> const& cb) {
    return all(*slice, cb);
  }

  static bool any(Slice const& slice,
                  std::function<bool(Slice const&, ValueLength)> const& cb);

  static bool any(Slice const* slice,
                  std::function<bool(Slice const&, ValueLength)> const& cb) {
    return any(*slice, cb);
  }

  static std::vector<std::string> keys(Slice const& slice);

  static std::vector<std::string> keys(Slice const* slice) {
    return keys(*slice);
  }

  static void keys(Slice const& slice, std::vector<std::string>& result);

  static void keys(Slice const* slice, std::vector<std::string>& result) {
    return keys(*slice, result);
  }

  static void keys(Slice const& slice, std::unordered_set<std::string>& result);

  static void keys(Slice const* slice,
                   std::unordered_set<std::string>& result) {
    return keys(*slice, result);
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

  static Builder merge(Slice const& left, Slice const& right, bool mergeValues);

  static Builder merge(Slice const* left, Slice const* right,
                       bool mergeValues) {
    return merge(*left, *right, mergeValues);
  }

  static void visitRecursive(
      Slice const& slice, VisitationOrder order,
      std::function<bool(Slice const&, Slice const&)> const& func);

  static void visitRecursive(
      Slice const* slice, VisitationOrder order,
      std::function<bool(Slice const&, Slice const&)> const& func) {
    visitRecursive(*slice, order, func);
  }
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
