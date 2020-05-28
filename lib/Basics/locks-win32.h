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

#ifndef ARANGODB_BASICS_LOCKS__WIN32_H
#define ARANGODB_BASICS_LOCKS__WIN32_H 1

#include "Basics/debugging.h"
#include "Basics/operating-system.h"
#include "Basics/system-compiler.h"

#ifdef TRI_HAVE_WIN32_THREADS

/// @brief condition variable
struct TRI_condition_t {
  CRITICAL_SECTION _lockWaiters;
  CONDITION_VARIABLE _conditionVariable;
};

#endif

#endif
