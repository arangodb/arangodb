////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_APPLICATION_FEATURES_SHARED_PRNG_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_SHARED_PRNG_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/xoroshiro128plus.h"

#include <memory>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

/// @brief a striped pseudo-random number generator, split into multiple stripes
/// for multi-threaded access without locking. each stripe is aligned to a cache
/// line and contains a single PRNG.
class SharedPRNGFeature final : public application_features::ApplicationFeature {
 public:
  explicit SharedPRNGFeature(application_features::ApplicationServer& server);
  ~SharedPRNGFeature();

  void prepare() override final;
  
  uint64_t rand() noexcept;

 private:
  uint64_t id() const noexcept;

  static constexpr uint64_t stripes = (1 << 16);
  static constexpr std::size_t alignment = 64;
  
  struct alignas(alignment) PaddedPRNG {
    basics::xoroshiro128plus prng;

    PaddedPRNG(uint64_t seed1, uint64_t seed2) {
      prng.seed(seed1, seed2);
    }

    inline uint64_t next() noexcept { return prng.next(); }
  };
  
  static_assert(sizeof(PaddedPRNG) == alignment);

  /// @brief contiguous memory which was used to allocate the PRNG buckets (with
  /// slight overallocation).
  std::unique_ptr<char[]> _memory;

  /// @brief pointer to PRNGs, bumped to cache line boundaries
  PaddedPRNG* _prngs;
};

}  // namespace arangodb

#endif
