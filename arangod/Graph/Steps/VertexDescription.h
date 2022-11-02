////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>

namespace arangodb {

namespace velocypack {
class Builder;
}  // namespace velocypack

namespace graph {
struct VertexDescription {
  std::string_view id;
  uint64_t depth;
  double weight;

  bool operator==(VertexDescription const& other) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, VertexDescription& description) {
  return f.object(description)
      .fields(f.field("vId", description.id),
              f.field("depth", description.depth),
              f.field("weight", description.weight));
}

}  // namespace graph
}  // namespace arangodb
