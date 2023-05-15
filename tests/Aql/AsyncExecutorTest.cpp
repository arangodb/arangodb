////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Aql/AqlExecutorTestCase.h"
#include "Mocks/FakeScheduler.h"

#include "Aql/AsyncExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;

struct AsyncExecutorTest : AqlExecutorTestCase<false> {
  AsyncExecutorTest()
      : AqlExecutorTestCase(&scheduler), scheduler(_server->server()) {}

  FakeScheduler scheduler;
};

TEST_F(AsyncExecutorTest, sleepingBeauty) {
}
