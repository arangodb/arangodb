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

#ifndef ARANGODB_PREGEL_GRAPH_FORMAT_H
#define ARANGODB_PREGEL_GRAPH_FORMAT_H 1

#include <cstddef>
#include "Basics/Common.h"

#include <velocypack/velocypack-aliases.h>
#include <velocypack/vpack.h>

namespace arangodb {
namespace pregel {

template <typename V, typename M>
struct GraphFormat {
  virtual size_t copyVertexData(VPackSlice document, void* targetPtr,
                                size_t maxSize) const = 0;
  virtual size_t copyEdgeData(VPackSlice edgeDocument, void* targetPtr,
                              size_t maxSize) const = 0;
  virtual V readVertexData(void* ptr) const = 0;
  virtual M readEdgeData(void* ptr) const = 0;
};

class IntegerGraphFormat : public GraphFormat<int64_t, int64_t> {
  const std::string _field;
  const int64_t _vDefault, _eDefault;

 public:
  IntegerGraphFormat(std::string const& field, int64_t vertexNull,
                     int64_t edgeNull)
      : _field(field), _vDefault(vertexNull), _eDefault(edgeNull) {}

  size_t copyVertexData(VPackSlice document, void* targetPtr,
                        size_t maxSize) const override {
    VPackSlice val = document.get(_field);
    *((int64_t*)targetPtr) = val.isInteger() ? val.getInt() : _vDefault;
    return sizeof(int64_t);
  }

  size_t copyEdgeData(VPackSlice document, void* targetPtr,
                      size_t maxSize) const override {
    VPackSlice val = document.get(_field);
    *((int64_t*)targetPtr) = val.isInteger() ? val.getInt() : _eDefault;
    return sizeof(int64_t);
  }

  int64_t readVertexData(void* ptr) const override { return *((int64_t*)ptr); }
  int64_t readEdgeData(void* ptr) const override { return *((int64_t*)ptr); }
};
}
}
#endif
