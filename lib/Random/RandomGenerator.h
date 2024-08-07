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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <limits>
#include <memory>

namespace arangodb {

// -----------------------------------------------------------------------------
// RandomDevice
// -----------------------------------------------------------------------------

class RandomDevice {
  RandomDevice(RandomDevice const&) = delete;
  RandomDevice& operator=(RandomDevice const&) = delete;

 public:
  RandomDevice() {}
  virtual ~RandomDevice() = default;

 public:
  virtual uint32_t random() = 0;

 public:
  int32_t interval(int32_t left, int32_t right);
  uint32_t interval(uint32_t left, uint32_t right);

  static unsigned long seed();

  int32_t random(int32_t left, int32_t right);

 private:
  int32_t power2(int32_t left, uint32_t right);
  int32_t other(int32_t left, uint32_t right);
};

// -----------------------------------------------------------------------------
// RandomGenerator
// -----------------------------------------------------------------------------

class RandomGenerator {
  RandomGenerator(RandomGenerator const&) = delete;
  RandomGenerator& operator=(RandomGenerator const&) = delete;

 public:
  // as not really used, types from 2 to 5 are deprecated and will be further
  // removed, only Mersenne will stay
  enum class RandomType {
    MERSENNE = 1,
    RANDOM = 2,
    URANDOM = 3,
    COMBINED = 4,
  };

  // satisfies UniformRandomBitGenerator
  // Note: As the RandomGenerator::interval(a, b) functions all take signed
  //       integers only, we cannot use the unsigned T at its fullest.
  template<typename T, T min_value = 0,
           T max_value = std::numeric_limits<std::make_signed_t<T>>::max()>
  struct UniformRandomGenerator {
    using result_type = T;
    using internal_type = std::make_signed_t<T>;

    static_assert(std::is_unsigned_v<T>);
    static_assert(max_value <= std::numeric_limits<internal_type>::max());

    constexpr static auto min() noexcept -> result_type { return min_value; }
    constexpr static auto max() noexcept -> result_type { return max_value; }

    constexpr auto operator()() -> result_type {
      return RandomGenerator::interval(static_cast<internal_type>(min()),
                                       static_cast<internal_type>(max()));
    }
  };

 public:
  static void initialize(RandomType);
  static void shutdown();
  static void ensureDeviceIsInitialized();

  static void seed(uint64_t);

  static int16_t interval(int16_t, int16_t);
  static int32_t interval(int32_t, int32_t);
  static int64_t interval(int64_t, int64_t);

  // return a nonnegative number lower or equal to the argument
  static uint16_t interval(uint16_t);
  static uint32_t interval(uint32_t);
  static uint64_t interval(uint64_t);

  // exposed only for testing
#ifdef ARANGODB_USE_GOOGLE_TESTS
  static int32_t random(int32_t left, int32_t right);
#endif

 private:
  static RandomType _type;
  static thread_local std::unique_ptr<RandomDevice> _device;
};
}  // namespace arangodb
