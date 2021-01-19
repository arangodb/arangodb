////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_GRAPH_FORMAT_H
#define ARANGODB_PREGEL_GRAPH_FORMAT_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <cstddef>
#include <type_traits>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"
#include "Cluster/ClusterFeature.h"
#include "Pregel/Graph.h"
#include "Greenspun/EvalResult.h"

struct TRI_vocbase_t;
namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace pregel {

template <typename V, typename E>
struct GraphFormat {

  explicit GraphFormat(application_features::ApplicationServer& server)
      : _server(server) {}
  virtual ~GraphFormat() = default;

  virtual size_t estimatedVertexSize() const { return sizeof(V); }
  virtual size_t estimatedEdgeSize() const { return sizeof(E); }

  virtual void copyVertexData(arangodb::velocypack::Options const& vpackOptions,
                              std::string const& documentId,
                              arangodb::velocypack::Slice document,
                              V& targetPtr, uint64_t& vertexIdRange) = 0;


  // the default implementation is to do nothing. only few algorithms actually override
  // this with a more specific behavior
  virtual void copyEdgeData(arangodb::velocypack::Options const& vpackOptions,
                            arangodb::velocypack::Slice edgeDocument, E& targetPtr) {}

  virtual bool buildVertexDocument(arangodb::velocypack::Builder& b, V const* targetPtr) const = 0;

  virtual greenspun::EvalResult buildVertexDocumentWithResult(arangodb::velocypack::Builder& b,
                                                              V const* targetPtr) const {
    buildVertexDocument(b, targetPtr);
    return {};
  }
 private:
  application_features::ApplicationServer& _server;
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
  NumberGraphFormat(application_features::ApplicationServer& server,
                    std::string const& source, std::string const& result,
                    V vertexNull, E edgeNull)
      : GraphFormat<V, E>(server),
        _sourceField(source),
        _resultField(result),
        _vDefault(vertexNull),
        _eDefault(edgeNull) {}

  void copyVertexData(arangodb::velocypack::Options const&, std::string const& documentId,
                      arangodb::velocypack::Slice document, V& targetPtr,
                      uint64_t& /*vertexIdRange*/) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    if (std::is_integral<V>::value) {
      if (std::is_signed<V>::value) {
        targetPtr = val.isInteger() ? val.getInt() : _vDefault;
      } else {
        targetPtr = val.isInteger() ? val.getUInt() : _vDefault;
      }
    } else {
      targetPtr = val.isNumber() ? val.getNumber<V>() : _vDefault;
    }
  }

  void copyEdgeData(arangodb::velocypack::Options const&,
                    arangodb::velocypack::Slice document, E& targetPtr) override {
    arangodb::velocypack::Slice val = document.get(_sourceField);
    if (std::is_integral<E>::value) {
      if (std::is_signed<E>::value) {  // getNumber does range checks
        targetPtr = val.isInteger() ? val.getInt() : _eDefault;
      } else {
        targetPtr = val.isInteger() ? val.getUInt() : _eDefault;
      }
    } else {
      targetPtr = val.isNumber() ? val.getNumber<E>() : _eDefault;
    }
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b, V const* ptr) const override {
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
  InitGraphFormat(application_features::ApplicationServer& server,
                  std::string const& result, V vertexNull, E edgeNull)
      : GraphFormat<V, E>(server),
        _resultField(result),
        _vDefault(vertexNull),
        _eDefault(edgeNull) {}

  virtual void copyVertexData(arangodb::velocypack::Options const&,
                              std::string const& /*documentId*/,
                              arangodb::velocypack::Slice /*document*/,
                              V& targetPtr, uint64_t& /*vertexIdRange*/) override {
    targetPtr = _vDefault;
  }

  virtual void copyEdgeData(arangodb::velocypack::Options const&,
                            arangodb::velocypack::Slice /*document*/, E& targetPtr) override {
    targetPtr = _eDefault;
  }

  virtual bool buildVertexDocument(arangodb::velocypack::Builder& b, V const* ptr) const override {
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
  VertexGraphFormat(application_features::ApplicationServer& server,
                    std::string const& result, V vertexNull)
      : GraphFormat<V, E>(server), _resultField(result), _vDefault(vertexNull) {}

  size_t estimatedVertexSize() const override { return sizeof(V); }
  virtual size_t estimatedEdgeSize() const override { return 0; }

  void copyVertexData(arangodb::velocypack::Options const&, std::string const& /*documentId*/,
                      arangodb::velocypack::Slice /*document*/, V& targetPtr,
                      uint64_t& /*vertexIdRange*/) override {
    targetPtr = _vDefault;
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b, V const* ptr) const override {
    b.add(_resultField, arangodb::velocypack::Value(*ptr));
    return true;
  }
};
}  // namespace pregel
}  // namespace arangodb
#endif
