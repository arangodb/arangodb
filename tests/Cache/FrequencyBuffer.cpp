////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::FrequencyBuffer
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Daniel H. Larkin
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include "Cache/FrequencyBuffer.h"

#include <stdint.h>

using namespace arangodb::cache;

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CCacheFrequencyBufferTest", "[cache]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test behavior with ints
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_uint8_t") {
  uint8_t zero = 0;
  uint8_t one = 1;
  uint8_t two = 2;

  // check that default construction is as expected
  CHECK(uint8_t() == zero);

  FrequencyBuffer<uint8_t> buffer(8);
  CHECK(buffer.memoryUsage() == sizeof(FrequencyBuffer<uint8_t>) + 8);

  for (size_t i = 0; i < 4; i++) {
    buffer.insertRecord(two);
  }
  for (size_t i = 0; i < 2; i++) {
    buffer.insertRecord(one);
  }

  auto frequencies = buffer.getFrequencies();
  CHECK(2ULL == frequencies->size());
  CHECK(one == (*frequencies)[0].first);
  CHECK(2ULL == (*frequencies)[0].second);
  CHECK(two == (*frequencies)[1].first);
  CHECK(4ULL == (*frequencies)[1].second);

  for (size_t i = 0; i < 8; i++) {
    buffer.insertRecord(one);
  }

  frequencies = buffer.getFrequencies();
  CHECK(1ULL == frequencies->size());
  CHECK(one == (*frequencies)[0].first);
  CHECK(8ULL == (*frequencies)[0].second);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test behavior with pointers
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_pointers") {
  uint8_t* zero = nullptr;
  uint8_t one = 1;
  uint8_t two = 2;

  // check that default construction is as expected
  typedef uint8_t* smallptr;
  CHECK(smallptr() == zero);

  FrequencyBuffer<uint8_t*> buffer(8);
  CHECK(buffer.memoryUsage() ==
                    sizeof(FrequencyBuffer<uint8_t*>) + (8 * sizeof(uint8_t*)));

  for (size_t i = 0; i < 4; i++) {
    buffer.insertRecord(&two);
  }
  for (size_t i = 0; i < 2; i++) {
    buffer.insertRecord(&one);
  }

  auto frequencies = buffer.getFrequencies();
  CHECK(2ULL == frequencies->size());
  CHECK(&one == (*frequencies)[0].first);
  CHECK(2ULL == (*frequencies)[0].second);
  CHECK(&two == (*frequencies)[1].first);
  CHECK(4ULL == (*frequencies)[1].second);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:
