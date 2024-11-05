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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

// This tests for specific line numbers, so be aware that you have to adapt the
// lines numbers if you change the code inside the test.

TEST(AsyncTest, source_location_in_registry_is_co_await_line) {
  {
    NoWait wait;
    auto coro = [&]() -> async<void> {
      auto void_fn = []() {};
      co_await wait;
      void_fn();
      co_return;
    }();

    uint count = 0;
    arangodb::async_registry::registry.for_promise(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, 34);
        });
    EXPECT_EQ(count, 1);
  }
  arangodb::async_registry::get_thread_registry().garbage_collect();
  {
    WaitSlot wait;
    auto coro = [&]() -> async<void> {
      auto void_fn = []() {};
      co_await wait;
      void_fn();
      co_return;
    }();

    uint count = 0;
    arangodb::async_registry::registry.for_promise(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, 50);
        });
    EXPECT_EQ(count, 1);
    wait.resume();

    count = 0;
    arangodb::async_registry::registry.for_promise(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, 52);
        });
    EXPECT_EQ(count, 1);
  }
  arangodb::async_registry::get_thread_registry().garbage_collect();
  {
    ConcurrentNoWait wait;
    auto coro = [&]() -> async<void> {
      auto void_fn = []() {};
      co_await wait;
      void_fn();
      co_return;
    }();

    uint count = 0;
    arangodb::async_registry::registry.for_promise(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, 77);
        });
    EXPECT_EQ(count, 1);
    wait.await();

    count = 0;
    arangodb::async_registry::registry.for_promise(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          count++;
          EXPECT_EQ(promise.source_location.line, 79);
        });
    EXPECT_EQ(count, 1);
  }
  arangodb::async_registry::get_thread_registry().garbage_collect();
}
