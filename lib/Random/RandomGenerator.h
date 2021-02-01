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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_RANDOM_RANDOM_GENERATOR_H
#define ARANGODB_RANDOM_RANDOM_GENERATOR_H 1

#include <cstdint>
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

 public:
  static unsigned long seed();

 private:
  int32_t random(int32_t left, int32_t right);
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
  enum class RandomType {
    MERSENNE = 1,
    RANDOM = 2,
    URANDOM = 3,
    COMBINED = 4,
    WINDOWS_CRYPT = 5  // uses the built in cryptographic services offered and
                       // recommended by microsoft (e.g. CryptGenKey(...) )
  };

 public:
  static void initialize(RandomType);
  static void shutdown();
  static void ensureDeviceIsInitialized();

  static void seed(uint64_t);

  static int16_t interval(int16_t, int16_t);
  static int32_t interval(int32_t, int32_t);
  static int64_t interval(int64_t, int64_t);

  static uint16_t interval(uint16_t);
  static uint32_t interval(uint32_t);
  static uint64_t interval(uint64_t);

 private:
  static RandomType _type;
  static thread_local std::unique_ptr<RandomDevice> _device;
};
}  // namespace arangodb

#endif
