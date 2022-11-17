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

// Ensures that the macro ADB_PROD_ASSERT is defined. There are three possible modes
//
// * USE_ARANGODB_LIGHT_ASSERTIONS -- a light version of assertions that does not
//   require ArangoDB's CrashHandler
//   The motivation for "light mode" is to resduce dependencies on ArangoDB code
//   for C++ unittests.
// * MAINTAINER_MODE -- in maintainer mode, when the condition in ADB_PROD_ASSERT
//   is false, the program crashes using ArangoDB's CrashHandler and the string
//   expression that follows the assertion is always evaluated regardless of
//   whether the expression evaluates to false.
// * Production mode: If the assertion fails the program still crashes, but
//   the string that can follow ADB_ASSERT is not evaluated if the assertion
//   does not fail.

#if defined(USE_ARANGODB_LIGHT_ASSERTIONS)

#define ADB_PROD_ASSERT(expr) /*GCOVR_EXCL_LINE*/                            \
  ::arangodb::debug::AssertionLightLogger{__FILE__, __LINE__,                \
                                          ARANGODB_PRETTY_FUNCTION, #expr} & \
      ::arangodb::debug::AssertionLightLogger::assertionStringStream         \
          .withCondition(expr)

#elif defined(ARANGODB_ENABLE_MAINTAINER_MODE)

#include "AssertionConditionalLogger.h"

// Always evaluates expr, even if the assertion does not fail
#define ADB_PROD_ASSERT(expr) /*GCOVR_EXCL_LINE*/                          \
  ::arangodb::debug::AssertionConditionalLogger{                           \
      __FILE__, __LINE__, ARANGODB_PRETTY_FUNCTION, #expr} &               \
      ::arangodb::debug::AssertionConditionalLogger::assertionStringStream \
          .withCondition(expr)


#else // Production

#include "AssertionLogger.h"

#define ADB_PROD_ASSERT(expr) /*GCOVR_EXCL_LINE*/                             \
  (ADB_LIKELY(expr))                                                          \
      ? (void)nullptr                                                         \
      : ::arangodb::debug::AssertionLogger{__FILE__, __LINE__,                \
                                           ARANGODB_PRETTY_FUNCTION, #expr} & \
            ::arangodb::debug::AssertionLogger::assertionStringStream

#endif
