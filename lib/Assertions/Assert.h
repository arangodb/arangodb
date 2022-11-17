///////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

// Ensures that the macro TRI_ASSERT is defined. There are three possible modes
//
// * USE_ARANGODB_LIGHT_ASSERTIONS -- a light version of assertions that does not
//   require ArangoDB's CrashHandler
//   The motivation for "light mode" is to resduce dependencies on ArangoDB code
//   for C++ unittests.
// * MAINTAINER_MODE -- in maintainer mode, when the condition in TRI_ASSERT is
//   false, the program crashes using ArangoDB's CrashHandler
// * Production mode: Assertions are compiled away. Except that the expression
//   in the assertion is actually evaluated, still, but the process does not
//   crash if the assertion fails.
//
#if defined(USE_ARANGODB_LIGHT_ASSERTIONS)

#include "AssertionLightLogger.h"

#define TRI_ASSERT(expr) /*GCOVR_EXCL_LINE*/                              \
  (ADB_LIKELY(expr))                                                      \
      ? (void)nullptr                                                     \
      : ::arangodb::debug::AssertionLightLogger{__FILE__, __LINE__,       \
                                                ARANGODB_PRETTY_FUNCTION, \
                                                #expr} &                  \
            ::arangodb::debug::AssertionLightLogger::assertionStringStream


#elif defined(ARANGODB_ENABLE_MAINTAINER_MODE)

#include "AssertionLogger.h"

#define TRI_ASSERT(expr) /*GCOVR_EXCL_LINE*/                                  \
  (ADB_LIKELY(expr))                                                          \
      ? (void)nullptr                                                         \
      : ::arangodb::debug::AssertionLogger{__FILE__, __LINE__,                \
                                           ARANGODB_PRETTY_FUNCTION, #expr} & \
            ::arangodb::debug::AssertionLogger::assertionStringStream



#else // Production

#include "AssertionNoOpLogger.h"

#define TRI_ASSERT(expr) /*GCOVR_EXCL_LINE*/          \
  (true) ? ((false) ? (void)(expr) : (void)nullptr)   \
         : ::arangodb::debug::AssertionNoOpLogger{} & \
               ::arangodb::debug::NoOpStream {}

#endif
