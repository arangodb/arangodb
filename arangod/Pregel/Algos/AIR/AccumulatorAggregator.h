////////////////////////////////////////////////////////////////////////////////
///
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
///
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_ACCUMULATORAGGREGATOR_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_ACCUMULATORAGGERGATOR_H 1

#include <Pregel/Aggregator.h>

#include "AIR.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

struct VertexAccumulatorAggregator : IAggregator {
  VertexAccumulatorAggregator(AccumulatorOptions const& opts, bool persists);
  virtual ~VertexAccumulatorAggregator() = default;

  /// @brief Used when updating aggregator value locally
  void aggregate(void const* valuePtr) override;

  /// @brief Used when updating aggregator value from remote
  void parseAggregate(arangodb::velocypack::Slice const& slice) override;
  void const* getAggregatedValue() const override;

  /// @brief Value from superstep S-1 supplied by the conductor
  void setAggregatedValue(arangodb::velocypack::Slice const& slice) override;
  void serialize(std::string const& key, arangodb::velocypack::Builder& builder) const override;
  void reset() override;

  bool isConverging() const override;

  [[nodiscard]] AccumulatorBase& getAccumulator() const;

 private:
  VertexData fake;
  std::unique_ptr<AccumulatorBase> accumulator;
  bool permanent;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb

#endif
