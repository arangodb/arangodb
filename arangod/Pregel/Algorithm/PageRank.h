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

struct PageRankData {
  using Settings = struct {};
  using VertexProperties = struct { float val; };
  using EdgeProperties = EmptyEdgeProperties;
  using Message = float;
  using Global = struct {};
  using Aggregators = struct { float foo; };
};

struct PageRank : AlgorithmBase<PageRankData> {
  [[nodiscard]] constexpr auto name() const -> std::string_view override {
    return "PageRank";
  }

  auto readVertexDocument(VPackSlice const &doc)
      -> PageRankData::VertexProperties override {
    return PageRankData::VertexProperties{.val = 0.0};
  }
  auto readEdgeDocument(VPackSlice const &doc)
      -> PageRankData::EdgeProperties override {
    return PageRankData::EdgeProperties{};
  }
  auto writeVertexDocument(VPackBuilder &b) -> void override { return; }

  auto vertexStep() -> void override { return; }
  auto conductorStep() -> void override { return; }
};
