////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/Slice.h>
#include "Pregel/Algorithm.h"

namespace arangodb::pregel::algos {

using V = float;  // need to simulate MaxAggregator
using E = uint8_t;

struct ReadWrite : public SimpleAlgorithm<V, E, V> {
  explicit ReadWrite(application_features::ApplicationServer& server,
                     arangodb::velocypack::Slice const& params);

  [[nodiscard]] GraphFormat<V, E>* inputFormat() const override;

  [[nodiscard]] MessageFormat<V>* messageFormat() const override {
    return new NumberMessageFormat<V>();
  }

  [[nodiscard]] MessageCombiner<V>* messageCombiner() const override {
    return new SumCombiner<V>();
  }

  VertexComputation<V, E, V>* createComputation(
      WorkerConfig const*) const override;

  [[nodiscard]] WorkerContext* workerContext(
      VPackSlice userParams) const override;

  [[nodiscard]] MasterContext* masterContext(
      VPackSlice userParams) const override;

  [[nodiscard]] IAggregator* aggregator(std::string const& name) const override;
};
}  // namespace arangodb::pregel::algos
