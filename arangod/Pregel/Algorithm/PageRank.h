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

#include <Algorithm/Algorithm.h>

#include <variant>

namespace arangodb::pregel::algorithms::pagerank {

struct Settings {
  double epsilon;
  double dampingFactor;

  std::string resultField;
};

struct VertexProperties {
  double pageRank;
};

struct Global {};

struct Message {
  double pageRank;
};

struct Aggregators {
  MaxAggregator<double> difference;
};

struct PageRankData {
  using Settings = Settings;
  using VertexProperties = VertexProperties;
  using EdgeProperties = algorithm_sdk::EmptyEdgeProperties;
  using Message = Message;

  using Global = Global;

  using Aggregators = Aggregators;
};

struct PageRank : arangodb::pregel::algorithm_sdk::AlgorithmBase<PageRankData> {
  [[nodiscard]] constexpr auto name() const -> std::string_view override {
    return "PageRank";
  }

  [[nodiscard]] auto readVertexDocument(VPackSlice const &doc) const
      -> VertexProperties override {
    // TODO: numberOfVertices needs to be in context
    return VertexProperties{.pageRank = 1.0 / numberOfVertices()};
  }
  [[nodiscard]] auto readEdgeDocument(VPackSlice const &doc) const
      -> PageRankData::EdgeProperties override {
    return PageRankData::EdgeProperties{};
  }
  // modify the whole document or just a pregel-defined sub-entry
  [[nodiscard]] auto writeVertexDocument(VertexProperties const &prop,
                                         VPackSlice const &doc)
      -> std::shared_ptr<VPackBuilder> override {
    auto builder = std::make_shared<VPackBuilder>();

    builder.add

        return builder;
  }

  virtual auto conductorSetup() -> void override {}
  virtual auto conductorStep(typename AlgorithmData::Global const &state)
      -> void {}
  virtual auto vertexStep(typename AlgorithmData::Global const &global,
                          typename AlgorithmData::VertexProperties &props) const
      -> void {

    if (gss == 0) {
    }
  }
};

} // namespace arangodb::pregel::algorithms::pagerank
