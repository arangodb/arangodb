////////////////////////////////////////////////////////////////////////////////
///
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

#include <velocypack/vpack.h>

#include <memory>
#include <string_view>

#include <fmt/core.h>

namespace arangodb::pregel::algorithm_sdk {
// These structs can be used by an algorithm
// implementor to signal that the respective
// data is empty, so we do not allocate *any*
// space for them.
//
// This becomes really quite important when
// we are talking about a billion vertices/edges
struct EmptyVertexProperties {};
struct EmptyEdgeProperties {};
struct EmptyMessage {};

template<typename AlgorithmData>
struct AlgorithmBase {
  [[nodiscard]] virtual constexpr auto name() const -> std::string_view = 0;

  [[nodiscard]] virtual auto readVertexDocument(VPackSlice const& doc) const ->
      typename AlgorithmData::VertexProperties = 0;

  [[nodiscard]] virtual auto readEdgeDocument(VPackSlice const& doc) const ->
      typename AlgorithmData::EdgeProperties = 0;

  virtual auto writeVertexDocument(
      typename AlgorithmData::VertexProperties const& prop,
      VPackSlice const& doc) -> std::shared_ptr<VPackBuilder>;

  virtual auto conductorSetup() -> void = 0;
  virtual auto conductorStep(typename AlgorithmData::Global const& state)
      -> void = 0;

  virtual auto vertexStep(typename AlgorithmData::Global const& global,
                          typename AlgorithmData::VertexProperties& props) const
      -> void = 0;

  virtual ~AlgorithmBase() = default;
};

}  // namespace arangodb::pregel::algorithm_sdk
