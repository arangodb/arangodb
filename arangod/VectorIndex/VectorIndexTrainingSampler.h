////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace arangodb::vector {

// Reservoir sampler implementing Algorithm L (Li 1994) for uniform-without-
// replacement sampling over a stream of unknown-but-bounded length. Each
// slot holds `dimension` consecutive floats. With no capacity set, consume()
// just appends — useful when no upper bound on the stream size is known; a
// subsequent resize() call can still produce a uniform sample.
class VectorIndexTrainingSampler {
 public:
  VectorIndexTrainingSampler(std::size_t dimension,
                             std::optional<std::size_t> capacity,
                             std::uint64_t seed);

  void consume(std::vector<float> const& vector);

  // Uniformly subsample down to `newCapacity` slots via a partial
  // Fisher–Yates shuffle. No-op if the reservoir already fits.
  void resize(std::size_t newCapacity);

  std::vector<float> release() &&;
  std::size_t itemsSeen() const noexcept;

 private:
  double sampleOpenUnit();
  std::size_t skipCount(double w);
  void primeSampler();

  std::size_t _dimension;
  std::optional<std::size_t> _capacity;
  std::mt19937_64 _rng;
  std::size_t _itemsSeen{0};
  double _w{0.0};
  std::size_t _nextReplacement{0};
  std::vector<float> _data;
};

}  // namespace arangodb::vector
