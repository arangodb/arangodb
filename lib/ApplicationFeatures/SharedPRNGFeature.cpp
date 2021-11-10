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

#include "SharedPRNGFeature.h"
#include "Basics/splitmix64.h"
#include "Basics/xoroshiro128plus.h"

#include <mutex>

using namespace arangodb::basics;

namespace {

/// @brief helper class for thread-safe creation of PRNG seed value
class PRNGSeeder {
 public: 
  PRNGSeeder()
      : _seeder(0xdeadbeefdeadbeefULL) {}

  uint64_t next() noexcept {
    std::lock_guard<std::mutex> guard(_mutex);
    return _seeder.next();
  }
 
 private:
  std::mutex _mutex;
  splitmix64 _seeder;
};

/// @brief global PRNG seeder, to seed thread-local PRNG objects
PRNGSeeder globalSeeder;

struct SeededPRNG {
  SeededPRNG() noexcept {
    uint64_t seed1 = globalSeeder.next();
    uint64_t seed2 = globalSeeder.next();
    prng.seed(seed1, seed2);
  }

  inline uint64_t next() noexcept { return prng.next(); }

  arangodb::basics::xoroshiro128plus prng;
};
  
static thread_local SeededPRNG threadLocalPRNG;

} // namespace


namespace arangodb {

SharedPRNGFeature::SharedPRNGFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "SharedPRNG") {
  setOptional(true);
}

SharedPRNGFeature::~SharedPRNGFeature() = default;

void SharedPRNGFeature::prepare() {
}

uint64_t SharedPRNGFeature::rand() noexcept {
  return ::threadLocalPRNG.next();
}
  
}  // namespace arangodb
