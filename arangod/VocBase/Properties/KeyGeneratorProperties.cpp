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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "KeyGeneratorProperties.h"
#include "Inspection/Status.h"

using namespace arangodb;

auto AutoIncrementGeneratorProperties::Invariants::isReasonableOffsetValue(
    uint64_t const& off) -> inspection::Status {
  if (off >= UINT64_MAX) {
    return {"offset value is too high"};
  }
  return {};
}

auto AutoIncrementGeneratorProperties::Invariants::isReasonableIncrementValue(
    uint64_t const& inc) -> inspection::Status {
  if (inc == 0 || inc >= (1ULL << 16)) {
    return {"increment value must be greater than zero and smaller than 65536"};
  }
  return {};
}
