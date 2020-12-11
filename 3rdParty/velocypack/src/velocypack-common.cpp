////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <chrono>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"

using namespace arangodb::velocypack;

static bool AssemblerFunctionsDisabled = false;

// disable hand-coded SSE4_2 functions for JSON parsing
// this must be called before the JSON parser is used 
void arangodb::velocypack::disableAssemblerFunctions() {
  AssemblerFunctionsDisabled = true;
}

bool arangodb::velocypack::assemblerFunctionsEnabled() { return !AssemblerFunctionsDisabled; }
bool arangodb::velocypack::assemblerFunctionsDisabled() { return AssemblerFunctionsDisabled; }

#ifndef VELOCYPACK_64BIT
// check if the length is beyond the size of a SIZE_MAX on this platform
std::size_t arangodb::velocypack::checkOverflow(ValueLength length) {
  if (length > static_cast<ValueLength>(SIZE_MAX)) {
    throw Exception(Exception::NumberOutOfRange);
  }
  return static_cast<std::size_t>(length);
}
#else
// 64 bit platform
static_assert(sizeof(std::size_t) == 8, "unexpected size_t size");
static_assert(sizeof(std::size_t) == sizeof(uint64_t),
              "unexpected size_t size");
#endif

int64_t arangodb::velocypack::currentUTCDateValue() {
  return static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

static_assert(sizeof(arangodb::velocypack::ValueLength) >= sizeof(SIZE_MAX),
              "invalid value for SIZE_MAX");
