////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class IndexId;
class LogicalCollection;
class Result;
struct ClusterIndexMethods {
  // static only class, never initialize
  ClusterIndexMethods() = delete;
  ~ClusterIndexMethods() = delete;

  [[nodiscard]] static auto dropIndexCoordinator(LogicalCollection const& col,
                                                 IndexId iid, double endTime)
      -> Result;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ensure an index in coordinator.
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] static auto ensureIndexCoordinator(  // create index
      LogicalCollection const& collection, arangodb::velocypack::Slice slice,
      bool create, arangodb::velocypack::Builder& resultBuilder, double timeout)
      -> Result;
};

}  // namespace arangodb