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

#include <Algorithm.h>

namespace pregel::algorithms::pagerank {

struct VertexProperties {
  uint64_t foo;
  float bla;
};

struct Msg1 {
  uint64_t something;
};

struct Msg2 {
  float something_else;
};

struct Message {
  std::variant<Msg1, Msg2> msg;
};

enum class Phase { Phase0, Phase1 };

struct Global {
  Phase phase;
};

struct PageRankData {
  using Settings = struct {};
  using VertexProperties = VertexProperties;
  using EdgeProperties = EmptyEdgeProperties;
  using Message = Message;

  using Global = struct {};

  using Aggregators = struct { float foo; };
};

struct PageRank : AlgorithmBase<PageRankData> {
  [[nodiscard]] constexpr auto name() const -> std::string_view override {
    return "PageRank";
  }

  auto readVertexDocument(VPackSlice const &doc) -> VertexProperties override {
    return VertexProperties{.val = 0.0};
  }
  auto readEdgeDocument(VPackSlice const &doc)
      -> PageRankData::EdgeProperties override {
    return PageRankData::EdgeProperties{};
  }
  auto writeVertexDocument(VPackBuilder &b) -> void override { return; }

  auto vertexStepPhase0() -> void{};

  auto vertexStep(Global const &global, VertexProperties &props)
      -> void override {
    switch (global.phase) {
    case Phase::Phase0:
      break;
    case Phase::Phase1:
      break;
    }
    props.foo = 15;

    return;
  }
  auto conductorStep(Global &state) -> void override {
    state.phase += 1;
    return;
  }
};

} // namespace pregel::algorithms::pagerank
