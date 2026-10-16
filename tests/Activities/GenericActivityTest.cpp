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

struct GenericActivityTest : ::testing::Test {
  GenericActivityTest() {
    Registry::setCurrentlyExecutingActivity(activities::Root);
  }
  void TearDown() override { registry.garbageCollect(); }
};

TEST_F(GenericActivityTest, metadata_is_set) {
  auto a = GenericActivity(1, nullptr, "TestActivity",          //
                           GenericActivityData{{"a", "one"},    //
                                               {"b", "two"}});  //

  auto t = a.copyData();
  ASSERT_EQ(t.at("a"), "one");
  ASSERT_EQ(t.at("b"), "two");
}

TEST_F(GenericActivityTest, metadata_can_be_changed) {
  auto a = GenericActivity(1, nullptr, "TestActivity",          //
                           GenericActivityData{{"a", "one"},    //
                                               {"b", "two"}});  //
  a.updateData([](auto&& m) { m["c"] = "three"; });

  auto t = a.copyData();

  ASSERT_EQ(t.at("a"), "one");
  ASSERT_EQ(t.at("b"), "two");
  ASSERT_EQ(t.at("c"), "three");
}

TEST_F(GenericActivityTest, metadata_mutator_returns_value) {
  auto a = GenericActivity(1, nullptr, "TestActivity",          //
                           GenericActivityData{{"a", "one"},    //
                                               {"b", "two"}});  //
  auto v = a.updateData([](auto& m) { return m["a"] = "three"; });
  auto w = a.getData([](auto&& m) { return m.at("a"); });

  ASSERT_EQ(v, "three");
  ASSERT_EQ(w, "three");
}
