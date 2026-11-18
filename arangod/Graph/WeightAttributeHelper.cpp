////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Graph/WeightAttributeHelper.h"

#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/voc-errors.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

namespace arangodb::graph {

std::vector<std::string> parseWeightAttribute(velocypack::Slice obj) {
  velocypack::Slice weightAttribute = obj.get("weightAttribute");
  if (weightAttribute.isString()) {
    auto value = weightAttribute.stringView();
    if (!value.empty()) {
      return {std::string(value)};
    }
  } else if (weightAttribute.isArray()) {
    std::vector<std::string> path;
    path.reserve(weightAttribute.length());
    for (velocypack::Slice part : velocypack::ArrayIterator(weightAttribute)) {
      if (!part.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "The options require weightAttribute to be a string or array of "
            "strings");
      }
      auto value = part.stringView();
      path.emplace_back(value.data(), value.size());
    }
    return path;
  } else if (!weightAttribute.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The option require weightAttribute to be a string or array of "
        "strings");
  }
  return {};
}

void addWeightAttribute(velocypack::Builder& builder,
                        std::vector<std::string> const& weightAttribute) {
  velocypack::ArrayBuilder guard(&builder, "weightAttribute");
  for (auto const& part : weightAttribute) {
    builder.add(velocypack::Value(part));
  }
}

double getEdgeWeight(velocypack::Slice edge,
                     std::vector<std::string> const& weightAttribute,
                     double defaultWeight) {
  if (weightAttribute.empty()) {
    return defaultWeight;
  }

  return basics::VelocyPackHelper::getNumericValue<double>(
      edge.get(weightAttribute), defaultWeight);
}

}  // namespace arangodb::graph
