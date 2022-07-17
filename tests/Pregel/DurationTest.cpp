////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include <gtest/gtest.h>
#include <variant>

#include <fmt/core.h>

#include <Pregel/Status/ExecutionStatus.h>

using namespace arangodb::pregel;

TEST(DurationTest, test_duration) {
  auto d1 = Duration{};

  ASSERT_FALSE(d1.hasStarted());
  ASSERT_FALSE(d1.hasFinished());

  d1.start();
  ASSERT_TRUE(d1.hasStarted());
  ASSERT_FALSE(d1.hasFinished());

  auto elapsed = d1.elapsedSeconds();
  ASSERT_GT(elapsed.count(), 0);

  auto moreElapsed = d1.elapsedSeconds();
  ASSERT_GT(moreElapsed, elapsed);

  d1.finish();
  ASSERT_TRUE(d1.hasStarted());
  ASSERT_TRUE(d1.hasFinished());

  auto evenMoreElapsed = d1.elapsedSeconds();

  ASSERT_GT(evenMoreElapsed, moreElapsed);

  auto notMoreElapsed = d1.elapsedSeconds();

  ASSERT_EQ(evenMoreElapsed, notMoreElapsed);
}
