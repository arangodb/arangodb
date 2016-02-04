////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "random.h"

#include "Basics/threads.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialized
////////////////////////////////////////////////////////////////////////////////

static bool Initialized = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a seed
////////////////////////////////////////////////////////////////////////////////

static unsigned long SeedRandom(void) {
  unsigned long seed;

#ifdef TRI_HAVE_GETTIMEOFDAY
  struct timeval tv;

  /* ignore result */ gettimeofday(&tv, 0);

  seed = static_cast<decltype(seed)>(tv.tv_sec);
  seed ^= static_cast<decltype(seed)>(tv.tv_usec);
#else
  seed = static_cast<decltype(seed)>(time(0));
#endif

  seed ^= static_cast<decltype(seed)>((uint32_t) TRI_CurrentProcessId() << 8);
  seed ^= static_cast<decltype(seed)>((uint32_t) TRI_CurrentProcessId() << 16);
  seed ^= static_cast<decltype(seed)>((uint32_t) TRI_CurrentProcessId() << 24);

#ifdef __APPLE__
  auto tid = reinterpret_cast<uintptr_t>(TRI_CurrentThreadId());
  seed ^= static_cast<decltype(seed)>(tid);
#else
  seed ^= static_cast<decltype(seed)>(TRI_CurrentThreadId());
#endif

  return seed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a 16 bit random unsigned integer
////////////////////////////////////////////////////////////////////////////////

uint16_t TRI_UInt16Random(void) {
#if RAND_MAX == 2147483647

  return TRI_random() & 0xFFFF;

#else

  uint32_t l1;
  uint32_t l2;

  l1 = TRI_random();
  l2 = TRI_random();

  return ((l1 & 0xFF) << 8) | (l2 & 0xFF);

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a 32 bit random unsigned integer
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_UInt32Random(void) {
#if RAND_MAX == 2147483647

  uint32_t l1;
  uint32_t l2;

  l1 = (uint32_t)TRI_random();
  l2 = (uint32_t)TRI_random();

  return ((l1 & 0xFFFF) << 16) | (l2 & 0xFFFF);

#else

#ifdef _WIN32
  unsigned int number;
  auto err = rand_s(&number);
  if (err == 0) {
    return number;
  }
#endif

  uint32_t l1;
  uint32_t l2;
  uint32_t l3;
  uint32_t l4;

  l1 = TRI_random();
  l2 = TRI_random();
  l3 = TRI_random();
  l4 = TRI_random();

  return ((l1 & 0xFF) << 24) | ((l2 & 0xFF) << 16) | ((l3 & 0xFF) << 8) |
         (l4 & 0xFF);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the random components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeRandom(void) {
  if (Initialized) {
    return;
  }

  TRI_srandom(SeedRandom());

  Initialized = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the random components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownRandom(void) {
  if (!Initialized) {
    return;
  }

  Initialized = false;
}
