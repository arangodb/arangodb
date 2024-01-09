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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

// Ensures that the macro ADB_PROD_ASSERT is defined. There are two possible
// modes
//
// * MAINTAINER_MODE -- in maintainer mode, when the condition in
// ADB_PROD_ASSERT
//   is false, the program crashes using ArangoDB's CrashHandler and the string
//   expression that follows the assertion is always evaluated regardless of
//   whether the expression evaluates to false.
// * Production mode: If the assertion fails the program still crashes, but
//   the string that can follow ADB_ASSERT is not evaluated if the assertion
//   does not fail.

#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)

#include "AssertionConditionalLogger.h"

#include "Basics/system-compiler.h"

// Always evaluates expr, even if the assertion does not fail
#define ADB_PROD_ASSERT(expr) /*GCOVR_EXCL_LINE*/                          \
  ::arangodb::debug::AssertionConditionalLogger{                           \
      __FILE__, __LINE__, ARANGODB_PRETTY_FUNCTION, #expr} &               \
      ::arangodb::debug::AssertionConditionalLogger::assertionStringStream \
          .withCondition(expr)

#else  // Production

#include "AssertionLogger.h"
#include "Basics/system-compiler.h"

#include "Basics/system-compiler.h"

#define ADB_PROD_ASSERT(expr) /*GCOVR_EXCL_LINE*/                              \
  (ADB_LIKELY(expr))                                                           \
      ? (void)nullptr                                                          \
      : ::arangodb::debug::AssertionLogger{ADB_HERE, ARANGODB_PRETTY_FUNCTION, \
                                           #expr} &                            \
            ::arangodb::debug::AssertionLogger::assertionStringStream

#endif

#define ADB_PROD_CRASH()                                                       \
  ::arangodb::debug::AssertionLogger{ADB_HERE, ARANGODB_PRETTY_FUNCTION, ""} & \
      ::arangodb::debug::AssertionLogger::assertionStringStream
