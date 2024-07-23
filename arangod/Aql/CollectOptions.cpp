////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
////////////////////////////////////////////////////////////////////////////////

#include "CollectOptions.h"
#include "Basics/Exceptions.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb::aql;

CollectOptions::CollectOptions() noexcept
    : method(CollectMethod::kUndefined), fixed(false) {}

/// @brief constructor
CollectOptions::CollectOptions(velocypack::Slice slice)
    : method(CollectMethod::kUndefined), fixed(true) {
  if (VPackSlice v = slice.get("collectOptions"); v.isObject()) {
    v = v.get("method");
    if (v.isString()) {
      method = methodFromString(v.stringView());
    }
  }
  if (VPackSlice v = slice.get("fixed"); v.isBoolean()) {
    fixed = v.isTrue();
  }

  TRI_ASSERT(method != CollectMethod::kUndefined || !fixed);
}

/// @brief whether or not the method has been fixed
bool CollectOptions::isFixed() const noexcept {
  TRI_ASSERT(!fixed || method != CollectMethod::kUndefined);
  return fixed;
}

/// @brief set method and fix it. note: some cluster optimizer rule
/// adjusts the methods after it has been initially fixed.
void CollectOptions::fixMethod(CollectMethod m) noexcept {
  TRI_ASSERT(m != CollectMethod::kUndefined);
  method = m;
  fixed = true;
}

/// @brief whether or not the method can be used
bool CollectOptions::canUseMethod(CollectMethod m) const noexcept {
  return (method == m || method == CollectMethod::kUndefined);
}

/// @brief whether or not the method should be used (i.e. is preferred)
bool CollectOptions::shouldUseMethod(CollectMethod m) const noexcept {
  TRI_ASSERT(m != CollectMethod::kUndefined);
  return method == m;
}

/// @brief convert the options to VelocyPack
void CollectOptions::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("method", VPackValue(methodToString(method)));
  builder.add("fixed", VPackValue(fixed));
}

/// @brief get the aggregation method from a string
CollectOptions::CollectMethod CollectOptions::methodFromString(
    std::string_view method) noexcept {
  if (method == "hash") {
    return CollectMethod::kHash;
  }
  if (method == "sorted") {
    return CollectMethod::kSorted;
  }
  if (method == "distinct") {
    return CollectMethod::kDistinct;
  }
  if (method == "count") {
    return CollectMethod::kCount;
  }

  return CollectMethod::kUndefined;
}

/// @brief stringify the aggregation method
std::string_view CollectOptions::methodToString(
    CollectOptions::CollectMethod method) {
  if (method == CollectMethod::kHash) {
    return "hash";
  }
  if (method == CollectMethod::kSorted) {
    return "sorted";
  }
  if (method == CollectMethod::kDistinct) {
    return "distinct";
  }
  if (method == CollectMethod::kCount) {
    return "count";
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "cannot stringify unknown aggregation method");
}
