////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Graph/EdgeDocumentToken.h"

namespace arangodb::graph {

struct ExpansionInfo {
  EdgeDocumentToken eid;
  std::vector<uint8_t> edgeData;  // keeps allocation
  size_t cursorId;
  ExpansionInfo(EdgeDocumentToken eid, VPackSlice edge, size_t cursorId)
      : eid(eid), cursorId(cursorId) {
    edgeData.resize(edge.byteSize());
    memcpy(edgeData.data(), edge.start(), edge.byteSize());
  }
  ExpansionInfo(ExpansionInfo const& other) = delete;
  ExpansionInfo(ExpansionInfo&& other) = default;
  VPackSlice edge() const noexcept { return VPackSlice(edgeData.data()); }
  size_t size() const noexcept {
    return sizeof(ExpansionInfo) + edgeData.size();
  }
};
using NeighbourBatch = std::shared_ptr<std::vector<ExpansionInfo>>;

}  // namespace arangodb::graph
