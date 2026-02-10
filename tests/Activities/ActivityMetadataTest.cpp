////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "Activities/registry.h"
#include "Async/async.h"
#include "Containers/Concurrent/thread.h"
#include "Activities/activity.h"
#include "Activities/activity_registry_variable.h"
#include "Inspection/JsonPrintInspector.h"
#include <gtest/gtest.h>

#include <coroutine>
#include <thread>

using namespace arangodb;
using namespace arangodb::activities;

struct ActivityMetadataTest : ::testing::Test {
  ActivityMetadataTest() {
    Registry::setCurrentlyExecutingActivity(ActivityRoot);
  }
  void TearDown() override { get_thread_registry().garbage_collect(); }
};

TEST_F(ActivityMetadataTest, metadata_is_set) {
  auto a = Activity("TestActivity",   //
                    {{"a", "one"},    //
                     {"b", "two"}});  //

  a.metadata().doUnderLock([](auto&& m) {
    ASSERT_EQ(m["a"], "one");
    ASSERT_EQ(m["b"], "two");
  });
}

TEST_F(ActivityMetadataTest, metadata_can_be_changed) {
  auto a = Activity("TestActivity",   //
                    {{"a", "one"},    //
                     {"b", "two"}});  //
  a.metadata().doUnderLock([](auto&& m) { m["c"] = "three"; });

  a.metadata().doUnderLock([](auto&& m) {
    ASSERT_EQ(m["a"], "one");
    ASSERT_EQ(m["b"], "two");
    ASSERT_EQ(m["c"], "three");
  });
}
