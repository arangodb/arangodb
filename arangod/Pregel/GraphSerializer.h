////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_GRAPH_SERIALIZER_H
#define ARANGODB_PREGEL_GRAPH_SERIALIZER_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <cstddef>

#include "Basics/Common.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/Graph.h"

namespace arangodb {
namespace pregel {

template <typename V>
struct VertexSerializer {
  virtual ~VertexSerializer() = default;

  virtual void serialize(VPackBuilder const& builder, V const* targetPtr) const = 0;

  virtual bool deserialize(VPackSlice const& data, V* targetPtr) const = 0;
};

template <>
struct VertexSerializer<int64_t> {
  virtual void serialize(VPackBuilder const& builder, int64_t const* targetPtr) const {
    builder.add(VPackValue(*targetPtr));
  };
  virtual bool deserialize(VPackSlice const& data, int64_t* targetPtr) const {
    *targetPtr = data.getInt();
  };
};
}  // namespace pregel
}  // namespace arangodb
#endif
