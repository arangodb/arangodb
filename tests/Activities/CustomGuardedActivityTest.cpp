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

#include "Activities/RegistryGlobalVariable.h"
#include "Async/async.h"
#include "Containers/Concurrent/thread.h"
#include "Activities/GenericActivity.h"
#include "Inspection/JsonPrintInspector.h"
#include <gtest/gtest.h>

#include <coroutine>
#include <thread>

using namespace arangodb;
using namespace arangodb::activities;

namespace {
struct CustomData {
  int a;
  std::string b;
};
template<typename Inspector>
auto inspect(Inspector& f, CustomData& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}
struct CustomActivity : GuardedActivity<CustomActivity, CustomData> {
  CustomActivity(ActivityId id, ActivityHandle parent, ActivityType type,
                 CustomData data)
      : GuardedActivity<CustomActivity, CustomData>(
            std::move(id), std::move(parent), std::move(type),
            std::move(data)) {}
};

}  // namespace

struct CustomGuardedActivityTest : ::testing::Test {
  CustomGuardedActivityTest() {
    Registry::setCurrentlyExecutingActivity(activities::Root);
  }
  void TearDown() override { registry.garbageCollect(); }
};

TEST_F(CustomGuardedActivityTest, metadata_is_set) {
  auto a = CustomActivity(1, nullptr, "TestActivity",       //
                          CustomData{.a = 4, .b = "one"});  //

  auto t = a.copyData();
  ASSERT_EQ(t.a, 4);
  ASSERT_EQ(t.b, "one");
}

TEST_F(CustomGuardedActivityTest, metadata_can_be_changed) {
  auto a = CustomActivity(1, nullptr, "TestActivity",       //
                          CustomData{.a = 4, .b = "one"});  //
  a.updateData([](auto&& m) { m.a = 7; });

  auto t = a.copyData();
  ASSERT_EQ(t.a, 7);
  ASSERT_EQ(t.b, "one");
}

TEST_F(CustomGuardedActivityTest, metadata_mutator_returns_value) {
  auto a = CustomActivity(1, nullptr, "TestActivity",       //
                          CustomData{.a = 4, .b = "one"});  //
  auto w = a.getData([](auto&& m) { return m.a; });

  ASSERT_EQ(w, 4);
}
