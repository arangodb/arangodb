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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "CollectOptions.h"
#include "Basics/Exceptions.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief constructor
CollectOptions::CollectOptions(VPackSlice const& slice)
    : method(CollectMethod::UNDEFINED) {
  VPackSlice v = slice.get("collectOptions");
  if (v.isObject()) {
    v = v.get("method");
    if (v.isString()) {
      method = methodFromString(v.copyString());
    }
  }
}

/// @brief whether or not the method can be used
bool CollectOptions::canUseMethod(CollectMethod method) const {
  return (this->method == method || this->method == CollectMethod::UNDEFINED);
}

/// @brief whether or not the method should be used (i.e. is preferred)
bool CollectOptions::shouldUseMethod(CollectMethod method) const {
  return (this->method == method);
}

/// @brief convert the options to VelocyPack
void CollectOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("method", VPackValue(methodToString(method)));
}

/// @brief get the aggregation method from a string
CollectOptions::CollectMethod CollectOptions::methodFromString(std::string const& method) {
  if (method == "hash") {
    return CollectMethod::HASH;
  }
  if (method == "sorted") {
    return CollectMethod::SORTED;
  }
  if (method == "distinct") {
    return CollectMethod::DISTINCT;
  }
  if (method == "count") {
    return CollectMethod::COUNT;
  }

  return CollectMethod::UNDEFINED;
}

/// @brief stringify the aggregation method
std::string CollectOptions::methodToString(CollectOptions::CollectMethod method) {
  if (method == CollectMethod::HASH) {
    return std::string("hash");
  }
  if (method == CollectMethod::SORTED) {
    return std::string("sorted");
  }
  if (method == CollectMethod::DISTINCT) {
    return std::string("distinct");
  }
  if (method == CollectMethod::COUNT) {
    return std::string("count");
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "cannot stringify unknown aggregation method");
}

CollectOptions::CollectOptions() : method(CollectMethod::UNDEFINED) {}
