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

namespace arangodb::pregel3 {

using VertexId = std::string;

struct Vertex {
  std::vector<size_t> neighbours;
};

struct Graph {
  std::vector<Vertex> vertices;
};

using QueryId = std::string;
struct Query : std::enable_shared_from_this<Query> {
  Query(TRI_vocbase_t& vocbase, QueryId id, GraphSpecification graphSpec)
      : id{std::move(id)},
        _graphSpec{std::move(graphSpec)},
        _vocbase(vocbase){};

  void loadGraph();

  enum class State { CREATED, LOADING, RUNNING, STORING, ERROR, DONE };

  auto getState() const -> State { return _state; }
  void setState(State state) { _state = state; }
  auto getGraphSpecification() const -> GraphSpecification {
    return _graphSpec;
  }

 private:
  QueryId id;
  GraphSpecification _graphSpec;
  std::shared_ptr<Graph> graph{nullptr};
  State _state = State::CREATED;
  TRI_vocbase_t& _vocbase;
};
}  // namespace arangodb::pregel3
