////////////////////////////////////////////////////////////////////////////////
/// @brief mersenne twister implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_MERSENNE_H
#define TRIAGENS_BASICS_C_MERSENNE_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  MERSENNE TWISTER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Random
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the mersenne twister
/// this function needs to be called just once on startup
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseMersenneTwister (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly seed the mersenne twister
////////////////////////////////////////////////////////////////////////////////

void TRI_SeedMersenneTwister (uint32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a 31 bit random number
///
/// generates a random number on [0,0x7fffffff]-interval
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_Int31MersenneTwister (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a 32 bit random number
///
/// generates a random number on [0,0xffffffff]-interval
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_Int32MersenneTwister (void);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
