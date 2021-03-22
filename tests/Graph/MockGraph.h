////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef TESTS_MOCK_GRAPH_H
#define TESTS_MOCK_GRAPH_H

#include "Basics/operating-system.h"

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"

#include "Graph/BaseOptions.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include <numeric>
#include <unordered_set>
#include <vector>

struct TRI_vocbase_t;

namespace arangodb {

namespace tests {

class PreparedRequestResponse;

namespace graph {

class MockGraph {
 public:
  struct EdgeDef {
    EdgeDef() : _from(""), _to(""), _weight(0.0), _eCol(""){};

    EdgeDef(std::string from, std::string to, double weight, std::string eCol)
        : _from(from), _to(to), _weight(weight), _eCol(eCol){};
    std::string _from;
    std::string _to;
    double _weight;
    std::string _eCol;

    std::string toString() const {
      return "<EdgeDef>(_from: " + _from + ", to: " + _to + ")";
    }

    void addToBuilder(arangodb::velocypack::Builder& builder) const;
  };

  struct VertexDef {
    VertexDef(std::string id) : _id(std::move(id)) {}

    void addToBuilder(arangodb::velocypack::Builder& builder) const;

    bool operator==(VertexDef const& other) const { return _id == other._id; }

    std::string _id;
  };

  struct hashVertexDef {
    std::size_t operator()(VertexDef const& value) const {
      return std::hash<std::string>{}(value._id);
    }
  };

 public:
  MockGraph() {}
  ~MockGraph() {}

  void addEdge(std::string from, std::string to, double weight = 1.0);
  void addEdge(size_t from, size_t to, double weight = 1.0);

  auto edges() const -> std::vector<EdgeDef> const& { return _edges; }
  auto vertices() const -> std::unordered_set<VertexDef, hashVertexDef> const& {
    return _vertices;
  }

  std::string const& getVertexCollectionName() const {
    return _vertexCollectionName;
  }

  std::string const& getEdgeCollectionName() const {
    return _edgeCollectionName;
  }

  std::string const vertexToId(size_t vertex) const {
    return _vertexCollectionName + "/" + std::to_string(vertex);
  }

  std::string const edgeToId(size_t edge) const {
    return _edgeCollectionName + "/" + std::to_string(edge);
  }

  template <class ServerType>
  void prepareServer(ServerType& server) const;

  template <class ServerType>
  std::pair<std::vector<arangodb::tests::PreparedRequestResponse>, uint64_t> simulateApi(
      ServerType& server,
      std::unordered_map<size_t, std::vector<std::pair<size_t, size_t>>> const& expectedVerticesEdgesBundleToFetch,
      arangodb::graph::BaseOptions& opts) const;

  void storeData(TRI_vocbase_t& vocbase, std::string const& vertexCollectionName,
                 std::string const& edgeCollectionName) const;

 private:
  std::vector<std::pair<std::string, std::string>> const& getVertexShardNameServerPairs() const {
    return _vertexShards;
  }
  std::vector<std::pair<std::string, std::string>> const& getEdgeShardNameServerPairs() const {
    return _edgeShards;
  }

 private:
  std::vector<EdgeDef> _edges;
  std::unordered_set<VertexDef, hashVertexDef> _vertices;

  std::string _vertexCollectionName{"v"};
  std::string _edgeCollectionName{"e"};

  std::vector<std::pair<std::string, std::string>> _vertexShards{
      {"s9870", "PRMR_0001"}};
  std::vector<std::pair<std::string, std::string>> _edgeShards{
      {"s9880", "PRMR_0001"}};
};
}  // namespace graph
}  // namespace tests
}  // namespace arangodb

#endif
