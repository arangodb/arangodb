////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RandomMask.h"

#include "Maskings/Maskings.h"
#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace arangodb::maskings;

ParseResult<AttributeMasking> RandomMask::create(Path path, Maskings* maskings,
                                                 VPackSlice const&) {
  return ParseResult<AttributeMasking>(AttributeMasking(path, new RandomMask(maskings)));
}

VPackValue RandomMask::mask(bool value, std::string&) const {
  int64_t result =
      RandomGenerator::interval(static_cast<int64_t>(0), static_cast<int64_t>(1));

  return VPackValue(result % 2 == 0);
}

VPackValue RandomMask::mask(int64_t, std::string&) const {
  int64_t result = RandomGenerator::interval(static_cast<int64_t>(-1000),
                                             static_cast<int64_t>(1000));

  return VPackValue(result);
}

VPackValue RandomMask::mask(double, std::string&) const {
  int64_t result = RandomGenerator::interval(static_cast<int64_t>(-1000),
                                             static_cast<int64_t>(1000));

  return VPackValue(1.0 * result / 100);
}
