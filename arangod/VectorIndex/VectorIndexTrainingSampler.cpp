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

#include "VectorIndex/VectorIndexTrainingSampler.h"

#include "Assertions/Assert.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace arangodb::vector {

VectorIndexTrainingSampler::VectorIndexTrainingSampler(std::size_t dimension,
                                                       std::size_t capacity,
                                                       std::uint64_t seed)
    : _dimension{dimension},
      _capacity{capacity},
      _rng{seed},
      _slotDist{0, capacity - 1} {
  TRI_ASSERT(_capacity > 0);
  TRI_ASSERT(_dimension > 0);
  _data.reserve(_capacity * _dimension);
}

bool VectorIndexTrainingSampler::wantsItem() const noexcept {
  return _itemsSeen < _capacity || _itemsSeen == _nextReplacement;
}

void VectorIndexTrainingSampler::consume(std::span<float const> vector) {
  TRI_ASSERT(wantsItem());
  TRI_ASSERT(vector.size() == _dimension);
  if (_itemsSeen < _capacity) {
    _data.insert(_data.end(), vector.begin(), vector.end());
    ++_itemsSeen;
    if (_itemsSeen == _capacity) {
      primeSampler();
    }
    return;
  }
  std::size_t const slot = _slotDist(_rng);
  std::ranges::copy(vector, _data.begin() + slot * _dimension);
  _w *= std::exp(std::log(sampleOpenUnit()) / static_cast<double>(_capacity));
  TRI_ASSERT(_w > 0.0 && _w < 1.0);
  _nextReplacement += 1 + skipCount(_w);
  ++_itemsSeen;
}

void VectorIndexTrainingSampler::skip() noexcept {
  TRI_ASSERT(!wantsItem());
  ++_itemsSeen;
}

void VectorIndexTrainingSampler::resize(std::size_t newCapacity) {
  std::size_t const current = _data.size() / _dimension;
  if (newCapacity >= current) {
    return;
  }
  using param_t = std::uniform_int_distribution<std::size_t>::param_type;
  for (std::size_t i = 0; i < newCapacity; ++i) {
    std::size_t const j = _slotDist(_rng, param_t{i, current - 1});
    if (i != j) {
      std::swap_ranges(_data.begin() + i * _dimension,
                       _data.begin() + (i + 1) * _dimension,
                       _data.begin() + j * _dimension);
    }
  }
  _data.resize(newCapacity * _dimension);
}

std::vector<float> VectorIndexTrainingSampler::release() && {
  return std::move(_data);
}

std::size_t VectorIndexTrainingSampler::itemsSeen() const noexcept {
  return _itemsSeen;
}

// Draws u ∈ (0, 1); rejects 0 so std::log(u) stays finite.
double VectorIndexTrainingSampler::sampleOpenUnit() {
  double u;
  do {
    u = _unitDist(_rng);
  } while (u <= 0.0);
  return u;
}

// Geometric skip count: floor(log(U) / log(1 - w)). Uses log1p for
// numerical stability when w is close to 0.
std::size_t VectorIndexTrainingSampler::skipCount(double w) {
  return static_cast<std::size_t>(
      std::floor(std::log(sampleOpenUnit()) / std::log1p(-w)));
}

void VectorIndexTrainingSampler::primeSampler() {
  _w = std::exp(std::log(sampleOpenUnit()) / static_cast<double>(_capacity));
  TRI_ASSERT(_w > 0.0 && _w < 1.0);
  _nextReplacement = _capacity + skipCount(_w);
}

}  // namespace arangodb::vector
