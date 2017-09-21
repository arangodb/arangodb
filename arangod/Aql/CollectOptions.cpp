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

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief constructor
CollectOptions::CollectOptions(VPackSlice const& slice) 
    : method(COLLECT_METHOD_UNDEFINED) {
  VPackSlice v = slice.get("collectOptions");
  if (v.isObject()) {
    v = v.get("method");
    if (v.isString()) {
      method = methodFromString(v.copyString());
    }
  }
}

/// @brief whether or not the hash method can be used
bool CollectOptions::canUseHashMethod() const {
  if (method == CollectMethod::COLLECT_METHOD_SORTED) {
    return false;
  }

  return true;
}

/// @brief convert the options to VelocyPack
void CollectOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("method", VPackValue(methodToString(method)));
}

/// @brief get the aggregation method from a string
CollectOptions::CollectMethod CollectOptions::methodFromString(
    std::string const& method) {
  if (method == "hash") {
    return CollectMethod::COLLECT_METHOD_HASH;
  }
  if (method == "sorted") {
    return CollectMethod::COLLECT_METHOD_SORTED;
  }

  return CollectMethod::COLLECT_METHOD_UNDEFINED;
}

/// @brief stringify the aggregation method
std::string CollectOptions::methodToString(
    CollectOptions::CollectMethod method) {
  if (method == CollectMethod::COLLECT_METHOD_HASH) {
    return std::string("hash");
  }
  if (method == CollectMethod::COLLECT_METHOD_SORTED) {
    return std::string("sorted");
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "cannot stringify unknown aggregation method");
}
