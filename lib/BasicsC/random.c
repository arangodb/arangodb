////////////////////////////////////////////////////////////////////////////////
/// @brief random functions
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "random.h"

#include "BasicsC/threads.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Random
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialised
////////////////////////////////////////////////////////////////////////////////

static bool Initialised = false;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Random
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a seed
////////////////////////////////////////////////////////////////////////////////

static unsigned long SeedRandom (void) {
  unsigned long seed;

#ifdef TRI_HAVE_GETTIMEOFDAY
  struct timeval tv;
  int result;
#endif


  seed = (unsigned long) time(0);

#ifdef TRI_HAVE_GETTIMEOFDAY
  result = gettimeofday(&tv, 0);

  seed ^= (unsigned long)(tv.tv_sec);
  seed ^= (unsigned long)(tv.tv_usec);
  seed ^= (unsigned long)(result);
#endif

  seed ^= (unsigned long)(TRI_CurrentProcessId());
  seed ^= (unsigned long)(TRI_CurrentThreadId());

  return seed;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Random
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a 16 bit random unsigned integer
////////////////////////////////////////////////////////////////////////////////

uint16_t TRI_UInt16Random (void) {
#if RAND_MAX == 2147483647

  return rand() & 0xFFFF;

#else

  uint32_t l1;
  uint32_t l2;

  l1 = rand();
  l2 = rand();

  return ((l1 & 0xFF) << 8) | (l2 & 0xFF);

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a 32 bit random unsigned integer
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_UInt32Random (void) {
#if RAND_MAX == 2147483647

  uint32_t l1;
  uint32_t l2;

  l1 = (uint32_t) rand();
  l2 = (uint32_t) rand();

  return ((l1 & 0xFFFF) << 16) | (l2 & 0xFFFF);

#else

  uint32_t l1;
  uint32_t l2;
  uint32_t l3;
  uint32_t l4;

  l1 = rand();
  l2 = rand();
  l3 = rand();
  l4 = rand();

  return ((l1 & 0xFF) << 24) | ((l2 & 0xFF) << 16) | ((l3 & 0xFF) << 8) | (l4 & 0xFF);

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Random
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the random components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseRandom (void) {
  if (Initialised) {
    return;
  }

  srandom(SeedRandom());

  Initialised = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the logging components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownRandom (void) {
  if (! Initialised) {
    return;
  }

  Initialised = false;
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
