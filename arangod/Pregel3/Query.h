////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <variant>
#include "velocypack/Slice.h"
#include "Basics/debugging.h"
#include "velocypack/Iterator.h"
#include "Utils.h"
#include "GraphSpecification.h"
#include "Utils/DatabaseGuard.h"
#include "Graph02.h"
#include "AlgorithmSpecification.h"
#include "Containers/FlatHashMap.h"

namespace arangodb::pregel3 {

using VertexId = std::string;

// struct Vertex {
//   std::vector<size_t> neighbours;
// };
//
// struct Graph {
//   std::vector<Vertex> vertices;
// };

using QueryId = std::string;
struct Query : std::enable_shared_from_this<Query> {
  Query(TRI_vocbase_t& vocbase, QueryId id, GraphSpecification graphSpec,
        AlgorithmSpecification algSpec)
      : id{std::move(id)},
        _graphSpec{std::move(graphSpec)},
        _algSpec{std::move(algSpec)},
        _vocbase(vocbase){};

  void loadGraph();

  void getGraph(VPackBuilder& builder);

  auto graphIsLoaded() -> bool;

  enum class State { CREATED, LOADING, LOADED, RUNNING, STORING, ERROR, DONE };

  auto getState() const -> State { return _state; }
  void setState(State state) { _state = state; }
  std::string_view getStateName() {
    switch (_state) {
      case State::CREATED:
        return "created";
        break;
      case State::LOADING:
        return "loading";
        break;
      case State::LOADED:
        return "loaded";
        break;
      case State::RUNNING:
        return "running";
        break;
      case State::STORING:
        return "storing";
        break;
      case State::ERROR:
        return "error";
        break;
      case State::DONE:
        return "done";
        break;
    }
    return "NOT IMPLEMENTED STATE";
  }
  auto getGraphSpecification() const -> GraphSpecification {
    return _graphSpec;
  }

 private:
  QueryId id;
  GraphSpecification _graphSpec;
  AlgorithmSpecification _algSpec;
  std::shared_ptr<BaseGraph> _graph{nullptr};
  State _state = State::CREATED;
  TRI_vocbase_t& _vocbase;
  // needed only to load the graph; will be cleared after use
  // (must be a member variable because used in callback lambda functions and
  // we do not have any influence on the parameters of those functions:
  // addVertex, addSingleEdge)
  containers::FlatHashMap<std::string, size_t> _vertexIdToIdx;
  double _defaultCapacity = 0.0;
};
}  // namespace arangodb::pregel3
