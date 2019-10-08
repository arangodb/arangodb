////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

/// Google offers a way to run a death test against the system.
/// e.g. define  set of cade that causes the process to crash.
/// this could be used to validate that certain states are considered
/// invalid in production code and not accidentially removed on refactorings.
/// However every such death test will generate a core-dump, even if the test is successful
/// This is inconvenient as it unnecessarily bloats up HDD usage and hides releveant coredumps
/// So this thin macro wraps around the GTEST :: EXPECT_DEATH macro and disables coredumps
/// only within the expected forked process

#ifndef ARANGODB_TESTS_MOCKS_DEATH_TEST_CHANGER_H
#define ARANGODB_TESTS_MOCKS_DEATH_TEST_CHANGER_H 1

#ifndef _WIN32

#include <sys/resource.h>

// Enabled on Linux and Mac

#define EXPECT_DEATH_CORE_FREE(func, assertion) \
  EXPECT_DEATH(                                 \
      [&]() {                                   \
        rlimit core_limit;                      \
        core_limit.rlim_cur = 0;                \
        core_limit.rlim_max = 0;                \
        setrlimit(RLIMIT_CORE, &core_limit);    \
        func;                                   \
      }(),                                      \
      assertion)

#else

// Disabled on windows
// If anyone knows how to disable core creation of a forked process
// please feel free to fix it here.

#define EXPECT_DEATH_CORE_FREE(func, assertion) EXPECT_TRUE(true)

#endif

#endif