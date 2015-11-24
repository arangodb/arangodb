////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <chrono>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"

using namespace arangodb::velocypack;

#ifndef VELOCYPACK_64BIT
// check if the length is beyond the size of a SIZE_MAX on this platform
void checkValueLength(ValueLength length) {
  if (length > static_cast<ValueLength>(SIZE_MAX)) {
    throw Exception(Exception::NumberOutOfRange);
  }
}
#endif

int64_t arangodb::velocypack::currentUTCDateValue() {
  return static_cast<int64_t>(
      std::chrono::system_clock::now().time_since_epoch().count() /
      std::chrono::milliseconds(1).count());
}

static_assert(sizeof(arangodb::velocypack::ValueLength) >= sizeof(SIZE_MAX),
              "invalid value for SIZE_MAX");
