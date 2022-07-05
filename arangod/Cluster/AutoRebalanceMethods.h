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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <string>
#include <vector>
#include <cstdint>

#include <Futures/Future.h>
#include <Basics/ResultT.h>

namespace arangodb::cluster {

struct MoveShardOperation {
  std::string shardId, from, to;
};

struct AutoRebalanceProblem;

struct RebalanceMethods {
  virtual ~RebalanceMethods() = default;

  struct OptimizeOptions {
    std::uint64_t version;
    std::size_t maximumNumberOfMoves;
    bool moveLeaders;
    bool moveFollowers;
    std::vector<std::string> databasesExcluded;
    double piFactor;
  };

  using MoveShardOperationList = std::vector<MoveShardOperation>;

  [[nodiscard]] virtual auto gatherInformation() const
      -> futures::Future<ResultT<AutoRebalanceProblem>> = 0;

  [[nodiscard]] virtual auto optimize(OptimizeOptions options) const
      -> futures::Future<ResultT<MoveShardOperationList>> = 0;

};

}  // namespace arangodb::cluster
