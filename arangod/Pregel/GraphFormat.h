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
#include "Pregel/Graph.h"

struct TRI_vocbase_t;
namespace arangodb {
namespace pregel {

template <typename V, typename E>
struct GraphFormat {
  virtual ~GraphFormat() {}

  virtual size_t estimatedVertexSize() const { return sizeof(V); };
  virtual size_t estimatedEdgeSize() const { return sizeof(E); };

  /// will load count number of vertex document, immidiately afterwards
  /// This must not be called again before not all docs were loaded
  virtual void willLoadVertices(uint64_t count) {}

  virtual size_t copyVertexData(std::string const& documentId,
                                arangodb::velocypack::Slice document,
                                V* targetPtr, size_t maxSize) = 0;
  virtual size_t copyEdgeData(arangodb::velocypack::Slice edgeDocument,
                              E* targetPtr, size_t maxSize) = 0;

  virtual bool buildVertexDocument(arangodb::velocypack::Builder& b,
                                   const V* targetPtr, size_t size) const = 0;
  virtual bool buildEdgeDocument(arangodb::velocypack::Builder& b,
                                 const E* targetPtr, size_t size) const = 0;
};

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

  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document, V* targetPtr,
                        size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    if (std::is_integral<V>::value) {
      if (std::is_signed<V>::value) {
        *targetPtr = val.isInteger() ? val.getInt() : _vDefault;
      } else {
        *targetPtr = val.isInteger() ? val.getUInt() : _vDefault;
      }
    } else {
      *targetPtr = val.isNumber() ? val.getNumber<V>() : _vDefault;
    }
    return sizeof(V);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, E* targetPtr,
                      size_t maxSize) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    if (std::is_integral<E>::value) {
      if (std::is_signed<E>::value) {  // getNumber does range checks
        *((E*)targetPtr) = val.isInteger() ? val.getInt() : _eDefault;
      } else {
        *targetPtr = val.isInteger() ? val.getUInt() : _eDefault;
      }
    } else {
      *targetPtr = val.isNumber() ? val.getNumber<E>() : _eDefault;
    }
    return sizeof(E);
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b, const V* ptr,
                           size_t size) const override {
    b.add(_resultField, arangodb::velocypack::Value(*ptr));
    return true;
  }

  bool buildEdgeDocument(arangodb::velocypack::Builder& b, const E* ptr,
                         size_t size) const override {
    b.add(_resultField, arangodb::velocypack::Value(*ptr));
    return true;
  }
};

template <typename V, typename E>
class InitGraphFormat : public GraphFormat<V, E> {
 protected:
  const std::string _resultField;
  const V _vDefault;
  const E _eDefault;

 public:
  InitGraphFormat(std::string const& result, V vertexNull, E edgeNull)
      : _resultField(result), _vDefault(vertexNull), _eDefault(edgeNull) {}

  virtual size_t copyVertexData(std::string const& documentId,
                                arangodb::velocypack::Slice document,
                                V* targetPtr, size_t maxSize) override {
    *(targetPtr) = _vDefault;
    return sizeof(V);
  }

  virtual size_t copyEdgeData(arangodb::velocypack::Slice document,
                              E* targetPtr, size_t maxSize) override {
    *targetPtr = _eDefault;
    return sizeof(E);
  }

  virtual bool buildVertexDocument(arangodb::velocypack::Builder& b,
                                   const V* ptr, size_t size) const override {
    b.add(_resultField, arangodb::velocypack::Value(*ptr));
    return true;
  }

  virtual bool buildEdgeDocument(arangodb::velocypack::Builder& b, const E* ptr,
                                 size_t size) const override {
    b.add(_resultField, arangodb::velocypack::Value(*ptr));
    return true;
  }
};

template <typename V, typename E>
class VertexGraphFormat : public GraphFormat<V, E> {
 protected:
  const std::string _resultField;
  const V _vDefault;

 public:
  VertexGraphFormat(std::string const& result, V vertexNull)
      : _resultField(result), _vDefault(vertexNull) {}

  size_t estimatedVertexSize() const override { return sizeof(V); };
  size_t estimatedEdgeSize() const override { return 0; };

  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document, V* targetPtr,
                        size_t maxSize) override {
    *targetPtr = _vDefault;
    return sizeof(V);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, V* targetPtr,
                      size_t maxSize) override {
    return 0;
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b, const V* ptr,
                           size_t size) const override {
    b.add(_resultField, arangodb::velocypack::Value(*ptr));
    return true;
  }

  bool buildEdgeDocument(arangodb::velocypack::Builder& b, const E* ptr,
                         size_t size) const override {
    return false;
  }
};
}
}
#endif
