////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gtest/gtest.h>

#include "Basics/ErrorCode.h"
#include "Basics/error.h"

namespace arangodb::tests {

// gtest predicate: succeeds when `res.ok()`. On failure it renders the full
// error information the Result carries -- number, symbolic name and message --
// which a bare ASSERT_TRUE(res.ok()) would discard. Works with any Result-like
// type (Result, OperationResult, ...) exposing ok()/errorNumber()/
// errorMessage().
template<typename R>
::testing::AssertionResult IsOk(R const& res) {
  if (res.ok()) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
         << "expected ok, but failed with " << res.errorNumber() << " ("
         << TRI_errno_string(res.errorNumber()) << "): " << res.errorMessage();
}

// gtest predicate: succeeds when `res` failed with exactly `expected`. Reports
// whether the result was unexpectedly ok or carried a different error, in the
// latter case including the actual error's name and message.
template<typename R>
::testing::AssertionResult IsError(R const& res, ErrorCode expected) {
  if (res.ok()) {
    return ::testing::AssertionFailure()
           << "expected error " << expected << " ("
           << TRI_errno_string(expected) << "), but result was ok";
  }
  if (res.errorNumber() != expected) {
    return ::testing::AssertionFailure()
           << "expected error " << expected << " ("
           << TRI_errno_string(expected) << "), but failed with "
           << res.errorNumber() << " (" << TRI_errno_string(res.errorNumber())
           << "): " << res.errorMessage();
  }
  return ::testing::AssertionSuccess();
}

}  // namespace arangodb::tests
