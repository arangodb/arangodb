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

#include <Inspection/VPack.h>

#include <variant>
#include <string>
#include "Algorithm.h"

namespace arangodb::pregel::algorithms::pagerank {

struct Settings {
  double epsilon;
  double dampingFactor;

  std::string resultField;
};
template<typename Inspector>
auto inspect(Inspector& f, Settings& x) {
  return f.object(x).fields(f.field("epsilon", x.epsilon),
                            f.field("dampingFactor", x.dampingFactor),
                            f.field("resultField", x.resultField));
}

struct VertexProperties {
  double pageRank;
};
template<typename Inspector>
auto inspect(Inspector& f, VertexProperties& x) {
  return f.object(x).fields(f.field("pageRank", x.pageRank));
}

struct Global {};

struct Message {
  double pageRank;
};

struct Aggregators {
  //  MaxAggregator<double> difference;
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
  PageRank(Settings const& settings)
      : algorithm_sdk::AlgorithmBase<PageRankData>(settings) {}

  [[nodiscard]] auto readVertexDocument(VPackSlice const& doc) const
      -> VertexProperties override {
    // TODO: numberOfVertices needs to be in context
    return VertexProperties{};
  }
  [[nodiscard]] auto readEdgeDocument(VPackSlice const& doc) const
      -> PageRankData::EdgeProperties override {
    return PageRankData::EdgeProperties{};
  }
  // modify the whole document or just a pregel-defined sub-entry
  [[nodiscard]] auto writeVertexDocument(VertexProperties const& prop,
                                         VPackSlice const& doc)
      -> std::shared_ptr<VPackBuilder> override {
    auto builder = std::make_shared<VPackBuilder>();
    builder->add("page_rank", VPackValue(prop.pageRank));
    return builder;
  }

  virtual auto conductorSetup() -> Global override {
    fmt::print("Conductor setup\n");
    return Global{};
  }
  virtual auto conductorStep(Global const& state) -> void override {
    fmt::print("Conductor step\n");
  }
  virtual auto vertexStep(Global const& global, VertexProperties& props) const
      -> bool override {
    fmt::print("Vertex step\n");
    return false;
  }
};

}  // namespace arangodb::pregel::algorithms::pagerank
