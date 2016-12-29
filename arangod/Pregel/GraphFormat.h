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

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <cstddef>
#include <type_traits>

#include "Basics/Common.h"

struct TRI_vocbase_t;
namespace arangodb {
namespace pregel {

template <typename V, typename E>
struct GraphFormat {
  virtual bool storesVertexData() const { return true; }
  virtual bool storesEdgeData() const { return true; }
  virtual size_t estimatedVertexSize() const { return sizeof(V); };
  virtual size_t estimatedEdgeSize() const { return sizeof(E); };

  virtual size_t copyVertexData(std::string const& documentId,
                                arangodb::velocypack::Slice document,
                                void* targetPtr, size_t maxSize) = 0;
  virtual size_t copyEdgeData(arangodb::velocypack::Slice edgeDocument,
                              void* targetPtr, size_t maxSize) = 0;

  virtual void buildVertexDocument(arangodb::velocypack::Builder& b,
                                   const void* targetPtr, size_t size) = 0;
  virtual void buildEdgeDocument(arangodb::velocypack::Builder& b,
                                 const void* targetPtr, size_t size) = 0;
};

class IntegerGraphFormat : public GraphFormat<int64_t, int64_t> {
  const std::string _sourceField, _resultField;
  const int64_t _vDefault, _eDefault;

 public:
  IntegerGraphFormat(std::string const& source, std::string const& result,
                     int64_t vertexNull, int64_t edgeNull)
      : _sourceField(source),
        _resultField(result),
        _vDefault(vertexNull),
        _eDefault(edgeNull) {}

  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document,
                        void* targetPtr, size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    *((int64_t*)targetPtr) = val.isInteger() ? val.getInt() : _vDefault;
    return sizeof(int64_t);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document,
                      void* targetPtr, size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    *((int64_t*)targetPtr) = val.isInteger() ? val.getInt() : _eDefault;
    return sizeof(int64_t);
  }

  void buildVertexDocument(arangodb::velocypack::Builder& b,
                           const void* targetPtr, size_t size) override {
    b.add(_resultField, VPackValue(*((int64_t*)targetPtr)));
  }

  void buildEdgeDocument(arangodb::velocypack::Builder& b,
                         const void* targetPtr, size_t size) override {
    b.add(_resultField, VPackValue(*((int64_t*)targetPtr)));
  }
};

class FloatGraphFormat : public GraphFormat<float, float> {
 protected:
  const std::string _sourceField, _resultField;
  const float _vDefault, _eDefault;

 public:
  FloatGraphFormat(std::string const& source, std::string const& result,
                   float vertexNull, float edgeNull)
      : _sourceField(source),
        _resultField(result),
        _vDefault(vertexNull),
        _eDefault(edgeNull) {}

  float readVertexData(const void* ptr) { return *((float*)ptr); }
  float readEdgeData(const void* ptr) { return *((float*)ptr); }

  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document,
                        void* targetPtr, size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    *((float*)targetPtr) = val.isDouble() ? (float)val.getDouble() : _vDefault;
    return sizeof(float);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document,
                      void* targetPtr, size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    *((float*)targetPtr) = val.isDouble() ? (float)val.getDouble() : _eDefault;
    return sizeof(float);
  }

  void buildVertexDocument(arangodb::velocypack::Builder& b,
                           const void* targetPtr, size_t size) override {
    b.add(_resultField, VPackValue(readVertexData(targetPtr)));
  }

  void buildEdgeDocument(arangodb::velocypack::Builder& b,
                         const void* targetPtr, size_t size) override {
    b.add(_resultField, VPackValue(readEdgeData(targetPtr)));
  }
};
/*
template <typename V, typename E>
class NumberGraphFormat : public GraphFormat<V, E> {
  static_assert(std::is_arithmetic<V>::value, "Vertex type must be numeric");
  static_assert(std::is_arithmetic<E>::value, "Edge type must be numeric");

protected:
  const std::string _sourceField, _resultField;
  const V _vDefault;
  const E _eDefault;

public:
  NumberGraphFormat(std::string const& source, std::string const& result,
                   V vertexNull, E edgeNull)
  : _sourceField(source),
  _resultField(result),
  _vDefault(vertexNull),
  _eDefault(edgeNull) {}

  V readVertexData(void* ptr) override { return *((V*)ptr); }
  E readEdgeData(void* ptr) override { return *((E*)ptr); }

  size_t copyVertexData(arangodb::velocypack::Slice document, void* targetPtr,
                        size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    if (std::is_integral<V>()) {
      if (std::is_signed<V>()) {
        *((V*)targetPtr) = val.isInteger() ? val.getInt() : _vDefault;
      } else {
        *((V*)targetPtr) = val.isInteger() ? val.getUInt() : _vDefault;
      }
    } else {
      *((V*)targetPtr) = val.isDouble() ? val.getDouble() : _vDefault;
    }
    return sizeof(V);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, void* targetPtr,
                      size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    if (std::is_integral<E>()) {
      if (std::is_signed<E>()) {
        *((E*)targetPtr) = val.isInteger() ? val.getInt() : _eDefault;
      } else {
        *((E*)targetPtr) = val.isInteger() ? val.getUInt() : _eDefault;
      }
    } else {
      *((E*)targetPtr) = val.isDouble() ? val.getDouble() : _eDefault;
    }
    return sizeof(E);
  }

  void buildVertexDocument(arangodb::velocypack::Builder& b, const void*
targetPtr,
                           size_t size) override {
    b.add(_resultField, VPackValue(readVertexData(targetPtr)));
  }

  void buildEdgeDocument(arangodb::velocypack::Builder& b, const void*
targetPtr,
                         size_t size) override {
    b.add(_resultField, VPackValue(readEdgeData(targetPtr)));
  }
};//*/
}
}
#endif
