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

#include <velocypack/Slice.h>
#include <cstddef>

#include "Basics/Common.h"

struct TRI_vocbase_t;
namespace arangodb {
namespace pregel {

template <typename V, typename E>
struct GraphFormat {
  
  virtual size_t estimatedVertexSize() const { return sizeof(V); };
  virtual size_t estimatedEdgeSize() const { return sizeof(E); };
  
  virtual size_t copyVertexData(arangodb::velocypack::Slice document, void* targetPtr,
                                size_t maxSize) = 0;
  virtual size_t copyEdgeData(arangodb::velocypack::Slice edgeDocument, void* targetPtr,
                              size_t maxSize) = 0;
  virtual V readVertexData(void* ptr) = 0;
  virtual E readEdgeData(void* ptr) = 0;

  virtual bool storesVertexData() const { return true; }
  virtual bool storesEdgeData() const { return true; }
};

class IntegerGraphFormat : public GraphFormat<int64_t, int64_t> {
  const std::string _field;
  const int64_t _vDefault, _eDefault;

 public:
  IntegerGraphFormat(std::string const& field, int64_t vertexNull,
                     int64_t edgeNull)
      : _field(field), _vDefault(vertexNull), _eDefault(edgeNull) {}

  size_t copyVertexData(arangodb::velocypack::Slice document, void* targetPtr,
                        size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_field);
    *((int64_t*)targetPtr) = val.isInteger() ? val.getInt() : _vDefault;
    return sizeof(int64_t);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, void* targetPtr,
                      size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_field);
    *((int64_t*)targetPtr) = val.isInteger() ? val.getInt() : _eDefault;
    return sizeof(int64_t);
  }

  int64_t readVertexData(void* ptr) override { return *((int64_t*)ptr); }
  int64_t readEdgeData(void* ptr) override { return *((int64_t*)ptr); }
};

class FloatGraphFormat : public GraphFormat<float, float> {
  const std::string _field;
  const float _vDefault, _eDefault;

 public:
  
  FloatGraphFormat(std::string const& field, float vertexNull, float edgeNull)
      : _field(field), _vDefault(vertexNull), _eDefault(edgeNull) {}

  size_t copyVertexData(arangodb::velocypack::Slice document, void* targetPtr,
                        size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_field);
    *((float*)targetPtr) = val.isInteger() ? val.getInt() : _vDefault;
    return sizeof(float);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, void* targetPtr,
                      size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_field);
    *((float*)targetPtr) = val.isInteger() ? val.getInt() : _eDefault;
    return sizeof(float);
  }

  float readVertexData(void* ptr) override { return *((float*)ptr); }
  float readEdgeData(void* ptr) override { return *((float*)ptr); }
};
}
}
#endif
