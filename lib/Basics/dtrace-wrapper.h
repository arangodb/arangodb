////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef USE_DTRACE
#define SDT_USE_VARIADIC 1
#include "Basics/sdt.h"

#define DTRACE_PROBE_NN(a, b, N, ...) DTRACE_PROBE##N(a, b, __VA_ARGS__)
#define DTRACE_PROBE_N(a, b, N, ...) DTRACE_PROBE_NN(a, b, N, __VA_ARGS__)
#define DTRACE_PROBE_(a, b, ...) \
  DTRACE_PROBE_N(a, b, _SDT_NARG(0, ##__VA_ARGS__), ##__VA_ARGS__)
#else

#define DTRACE_PROBE(a, b) \
  do {                     \
  } while (0);
#define DTRACE_PROBE1(a, b, c) \
  do {                         \
  } while (0);
#define DTRACE_PROBE2(a, b, c, d) \
  do {                            \
  } while (0);

#define DTRACE_PROBE_(a, b, ...)

#endif
