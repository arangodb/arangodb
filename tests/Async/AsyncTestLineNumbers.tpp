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
///
////////////////////////////////////////////////////////////////////////////////

TEST(AsyncTest, source_location_in_registry_is_co_await_line) {
  {
    async_tests::NoWait wait;
    constexpr int kCoReturnLine = __LINE__ + 5;
    auto fn = [&]() -> async<void> {
      auto void_fn = []() {};
      co_await wait;
      void_fn();
      co_return;
    };
    std::ignore = fn();

    uint count = 0;
    arangodb::async_registry::registry.for_node(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, kCoReturnLine);
        });
    EXPECT_EQ(count, 1);
  }
  arangodb::async_registry::get_thread_registry().garbage_collect();
  {
    async_tests::WaitSlot wait;
    constexpr int kCoAwaitLine = __LINE__ + 4;
    constexpr int kCoReturnLine = __LINE__ + 5;
    auto fn = [&]() -> async<void> {
      auto void_fn = []() {};
      co_await wait;
      void_fn();
      co_return;
    };
    std::ignore = fn();

    uint count = 0;
    arangodb::async_registry::registry.for_node(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, kCoAwaitLine);
        });
    EXPECT_EQ(count, 1);
    wait.resume();

    count = 0;
    arangodb::async_registry::registry.for_node(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, kCoReturnLine);
        });
    EXPECT_EQ(count, 1);
  }
  arangodb::async_registry::get_thread_registry().garbage_collect();
  {
    async_tests::ConcurrentNoWait wait;
    constexpr int kCoAwaitLine = __LINE__ + 4;
    constexpr int kCoReturnLine = __LINE__ + 5;
    auto fn = [&]() -> async<void> {
      auto void_fn = []() {};
      co_await wait;
      void_fn();
      co_return;
    };
    std::ignore = fn();

    uint count = 0;
    arangodb::async_registry::registry.for_node(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, kCoAwaitLine);
        });
    EXPECT_EQ(count, 1);
    wait.await();

    count = 0;
    arangodb::async_registry::registry.for_node(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, kCoReturnLine);
        });
    EXPECT_EQ(count, 1);
  }
  arangodb::async_registry::get_thread_registry().garbage_collect();
}