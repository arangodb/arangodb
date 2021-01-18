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

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include <numeric>
#include <unordered_set>
#include <vector>

namespace arangodb {
namespace tests {
namespace graph {

class MockGraph {
 public:
  struct EdgeDef {
    EdgeDef(std::string from, std::string to, double weight)
        : _from(from), _to(to), _weight(weight){};
    std::string _from;
    std::string _to;
    double _weight;

    std::string toString() const {
      return "<EdgeDef>(_from: " + _from + ", to: " + _to + ")";
    }

    void addToBuilder(arangodb::velocypack::Builder& builder) const {
      std::string fromId = _from;
      std::string toId = _to;
      std::string keyId = _from.substr(2) + "-" + _to.substr(2);

      builder.openObject();
      builder.add(StaticStrings::IdString, VPackValue("e/" + keyId));
      builder.add(StaticStrings::KeyString, VPackValue(keyId));
      builder.add(StaticStrings::FromString, VPackValue(fromId));
      builder.add(StaticStrings::ToString, VPackValue(toId));
      builder.add("weight", VPackValue(_weight));
      builder.close();
    }
  };

  struct VertexDef {
    VertexDef(std::string id) : _id(std::move(id)) {}

    void addToBuilder(arangodb::velocypack::Builder& builder) const {
      builder.openObject();
      builder.add(StaticStrings::KeyString, VPackValue(_id.substr(2)));
      builder.add(StaticStrings::IdString, VPackValue(_id));
      builder.close();
    }

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

  void addEdge(std::string from, std::string to, double weight = 1.0) {
    _edges.emplace_back(EdgeDef{from, to, weight});
    _vertices.emplace(std::move(from));
    _vertices.emplace(std::move(to));
  }

  void addEdge(size_t from, size_t to, double weight = 1.0) {
    addEdge("v/" + basics::StringUtils::itoa(from),
            "v/" + basics::StringUtils::itoa(to), weight);
  }

  auto edges() const -> std::vector<EdgeDef> const& { return _edges; }
  auto vertices() const -> std::unordered_set<VertexDef, hashVertexDef> const& {
    return _vertices;
  }

 private:
  std::vector<EdgeDef> _edges;
  std::unordered_set<VertexDef, hashVertexDef> _vertices;
};
}  // namespace graph
}  // namespace tests
}  // namespace arangodb

#endif
