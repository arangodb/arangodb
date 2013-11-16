////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for mersenne.c
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <boost/test/unit_test.hpp>

#include "BasicsC/mersenne.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CMersenneSetup {
  CMersenneSetup () {
    BOOST_TEST_MESSAGE("setup mersenne");

    TRI_InitialiseMersenneTwister();
  }

  ~CMersenneSetup () {
    BOOST_TEST_MESSAGE("tear-down mersenne");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CMersenneTest, CMersenneSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test mersenne int32
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_mersenne_int32) {
  for (size_t i = 0; i < 100; ++i) {
    int64_t value = (int64_t) TRI_Int32MersenneTwister();
  
    BOOST_CHECK_EQUAL(true, value >= 0);
    BOOST_CHECK_EQUAL(true, value <= UINT32_MAX);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test mersenne int31
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_mersenne_int31) {
  for (size_t i = 0; i < 100; ++i) {
    int64_t value = (int64_t) TRI_Int31MersenneTwister();
  
    BOOST_CHECK_EQUAL(true, value >= 0);
    BOOST_CHECK_EQUAL(true, value <= INT32_MAX);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test after explicit seed
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_mersenne_seed) {
  TRI_SeedMersenneTwister((uint32_t) 0UL);
  BOOST_CHECK_EQUAL((uint32_t) 2357136044UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 2546248239UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 3071714933UL, TRI_Int32MersenneTwister());

  TRI_SeedMersenneTwister((uint32_t) 1UL);
  BOOST_CHECK_EQUAL((uint32_t) 1791095845UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 4282876139UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 3093770124UL, TRI_Int32MersenneTwister());

  TRI_SeedMersenneTwister((uint32_t) 2UL);
  BOOST_CHECK_EQUAL((uint32_t) 1872583848UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 794921487UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 111352301UL, TRI_Int32MersenneTwister());

  TRI_SeedMersenneTwister((uint32_t) 23UL);
  BOOST_CHECK_EQUAL((uint32_t) 2221777491UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 2873750246UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 4067173416UL, TRI_Int32MersenneTwister());

  TRI_SeedMersenneTwister((uint32_t) 42UL);
  BOOST_CHECK_EQUAL((uint32_t) 1608637542UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 3421126067UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 4083286876UL, TRI_Int32MersenneTwister());

  TRI_SeedMersenneTwister((uint32_t) 458735UL);
  BOOST_CHECK_EQUAL((uint32_t) 1537542272UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 4131475792UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 2280116031UL, TRI_Int32MersenneTwister());

  TRI_SeedMersenneTwister((uint32_t) 395568682893UL);
  BOOST_CHECK_EQUAL((uint32_t) 2297195664UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 2381406737UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 4184846092UL, TRI_Int32MersenneTwister());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test after explicit seed, repeated calls 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_mersenne_reseed) {
  TRI_SeedMersenneTwister((uint32_t) 23UL);
  BOOST_CHECK_EQUAL((uint32_t) 2221777491UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 2873750246UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 4067173416UL, TRI_Int32MersenneTwister());
  
  // re-seed with same value and compare
  TRI_SeedMersenneTwister((uint32_t) 23UL);
  BOOST_CHECK_EQUAL((uint32_t) 2221777491UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 2873750246UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 4067173416UL, TRI_Int32MersenneTwister());
 
  // seed with different value 
  TRI_SeedMersenneTwister((uint32_t) 458735UL);
  BOOST_CHECK_EQUAL((uint32_t) 1537542272UL, TRI_Int32MersenneTwister());
  
  // re-seed with original value and compare
  TRI_SeedMersenneTwister((uint32_t) 23UL);
  BOOST_CHECK_EQUAL((uint32_t) 2221777491UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 2873750246UL, TRI_Int32MersenneTwister());
  BOOST_CHECK_EQUAL((uint32_t) 4067173416UL, TRI_Int32MersenneTwister());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
