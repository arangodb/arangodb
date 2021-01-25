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
#include "Basics/Thread.h"
#include "Basics/debugging.h"
#include "Basics/fasthash.h"
#include "Basics/splitmix64.h"

using namespace arangodb::basics;

namespace arangodb {

SharedPRNGFeature::SharedPRNGFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "SharedPRNG"),
      _prngs(nullptr) {
  setOptional(true);
}

SharedPRNGFeature::~SharedPRNGFeature() = default;

void SharedPRNGFeature::prepare() {
  // allocate memory for all stripes.
  _prngs = std::make_unique<PaddedPRNG[]>(stripes);

  PaddedPRNG* prngs = _prngs.get();
  TRI_ASSERT((reinterpret_cast<uintptr_t>(prngs) & (sizeof(xoroshiro128plus) - 1)) == 0);
  
  splitmix64 seeder(0xdeadbeefdeadbeefULL);

  // initialize all stripes
  for (std::size_t i = 0; i < stripes; ++i) {
    uint64_t seed1 = seeder.next();
    uint64_t seed2 = seeder.next();
    TRI_ASSERT((reinterpret_cast<uintptr_t>(&prngs[i]) & (sizeof(xoroshiro128plus) - 1)) == 0);
    prngs[i].seed(seed1, seed2);
  }
}
  
uint64_t SharedPRNGFeature::id() const noexcept {
  return fasthash64_uint64(Thread::currentThreadNumber(), 0xdeadbeefdeadbeefULL);
}

uint64_t SharedPRNGFeature::rand() noexcept { 
  return _prngs[id() & (stripes - 1)].next(); 
}

}  // namespace arangodb
