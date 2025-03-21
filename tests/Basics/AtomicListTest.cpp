////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Containers/BoundedList.h"
#include <thread>

#include "gtest/gtest.h"

using namespace arangodb;

TEST(AtomicListTests, testBasicOperation) {
  AtomicList<int> list;
  list.prepend(1);
  list.prepend(2);
  list.prepend(3);
  AtomicList<int>::Node* p = list.getSnapshot();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data, 3);
  p = p->next();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data, 2);
  p = p->next();
  ASSERT_TRUE(p != nullptr);
  ASSERT_EQ(p->_data, 1);
  p = p->next();
  ASSERT_TRUE(p == nullptr);
}

struct Entry {
  int a;
  Entry(int a) : a(a) {}
  size_t memoryUsage() const noexcept { return sizeof(Entry); }
};

TEST(BoundedListTests, testBasicOperation) {
  BoundedList<Entry> list(1024 * 1024, 3);
  list.prepend(Entry{1});
  list.prepend(Entry{2});
  list.prepend(Entry{3});

  std::vector<int> values;
  list.forItems([&values](const Entry& entry) { values.push_back(entry.a); });

  ASSERT_EQ(values.size(), 3);
  ASSERT_EQ(values[0], 3);
  ASSERT_EQ(values[1], 2);
  ASSERT_EQ(values[2], 1);
}

TEST(AtomicListTests, testConcurrentOperation) {
  AtomicList<int> list;
  std::atomic<bool> keep_running{true};
  std::atomic<size_t> total_count{0};

  std::atomic<int64_t> dummy{0};

  // Create a thread that continuously takes snapshots
  std::thread reader([&list, &keep_running, &dummy]() {
    while (keep_running.load()) {
      auto* snapshot = list.getSnapshot();
      int64_t sum = 0;
      while (snapshot != nullptr) {
        // Just traverse the list and sum the entries
        sum += snapshot->_data;
        snapshot = snapshot->next();
      }
      dummy.fetch_add(sum);
    }
  });

  // Create 10 writer threads
  std::vector<std::thread> writers;
  for (int t = 0; t < 10; ++t) {
    writers.emplace_back(
        [&list, &total_count](int thread_id) {
          for (int i = 0; i < 100000; ++i) {
            list.prepend(thread_id * 100000 + i);
            total_count.fetch_add(1, std::memory_order_relaxed);
          }
        },
        t);
  }

  // Wait for all writers to finish
  for (auto& t : writers) {
    t.join();
  }

  // Stop the reader thread
  keep_running.store(false);
  reader.join();

  // Verify the total number of elements
  size_t count = 0;
  auto* p = list.getSnapshot();
  while (p != nullptr) {
    ++count;
    p = p->next();
  }

  ASSERT_EQ(total_count.load(), 1000000);
  ASSERT_EQ(count, 1000000);
  ASSERT_NE(dummy, 0);
}

TEST(BoundedListTests, testConcurrentOperation) {
  // Use a relatively small memory threshold to force rotations
  const size_t memoryThreshold = 1024 * 100;  // 100KB
  const size_t maxHistory = 3;
  BoundedList<Entry> list(memoryThreshold, maxHistory);
  std::atomic<bool> keep_running{true};
  std::atomic<size_t> total_prepended{0};

  std::atomic<int64_t> dummy{0};

  // Create a thread that continuously reads items
  std::thread reader([&list, &keep_running, &dummy]() {
    while (keep_running.load()) {
      int64_t sum = 0;
      list.forItems([&sum](const Entry& entry) { sum += entry.a; });
      dummy.fetch_add(sum);
    }
  });

  // Create 10 writer threads
  std::vector<std::thread> writers;
  for (int t = 0; t < 10; ++t) {
    writers.emplace_back(
        [&list, &total_prepended](int thread_id) {
          for (int i = 0; i < 1000000; ++i) {
            list.prepend(Entry{thread_id * 100000 + i});
            total_prepended.fetch_add(1, std::memory_order_relaxed);
          }
        },
        t);
  }

  // Wait for all writers to finish
  for (auto& t : writers) {
    t.join();
  }

  // Stop the reader thread
  keep_running.store(false);
  reader.join();

  // Count total elements
  size_t total_count = 0;
  size_t current_memory = 0;
  list.forItems([&](const Entry& entry) {
    ++total_count;
    current_memory += entry.memoryUsage();
  });

  // We used to have a memory overshooting test here, but it was flaky.
  // The trouble is that with very few cores it can happen that when
  // many threads write to the list that some of the lists overshoot their
  // memory usage considerably. This is, because the thread which happens
  // to rotate the lists can be suspended when not enough cores are present.
  // Since this is a problem which is not going to be relevant in practice,
  // we ignore it here. This has been sacrified on the alter of performance.

  // Verify we have some elements and the reader did something
  ASSERT_GT(total_count, 0);
  ASSERT_NE(dummy, 0);
}

TEST(BoundedListTests, testOrderPreservation) {
  // Calculate memory threshold needed for all entries
  const size_t numEntries = 1000000;
  const size_t entrySize = sizeof(Entry);
  const size_t memoryThreshold = numEntries * entrySize / 2;

  BoundedList<Entry> list(memoryThreshold, 3);

  // Insert entries in ascending order
  for (size_t i = 0; i < numEntries; ++i) {
    list.prepend(Entry{static_cast<int>(i)});
  }

  // Verify reverse order
  size_t count = 0;
  size_t expected = numEntries - 1;  // Start with highest number
  list.forItems([&](const Entry& entry) {
    ASSERT_EQ(entry.a, expected);
    expected--;
    count++;
  });

  ASSERT_EQ(count, numEntries);
  ASSERT_EQ(expected, size_t(-1));  // We should have counted down to -1
}

TEST(BoundedListTests, testTrashCollection) {
  // Calculate memory threshold to force rotations
  const size_t entrySize = sizeof(Entry);
  const size_t entriesPerBatch = 1000;
  const size_t memoryThreshold = entrySize * entriesPerBatch;
  const size_t maxHistory = 3;

  BoundedList<Entry> list(memoryThreshold, maxHistory);

  // Fill multiple batches to force rotations
  const size_t totalBatches =
      maxHistory + 2;  // Create more than maxHistory to force trash
  const size_t totalEntries = entriesPerBatch * totalBatches + 17;
  // plus 17 to have some in the current list!

  for (size_t i = 0; i < totalEntries; ++i) {
    list.prepend(Entry{static_cast<int>(i)});
  }

  // Get trash and verify
  size_t count = list.clearTrash();
  ASSERT_GT(count, 0);

  // Verify trash is cleared after getting it
  count = list.clearTrash();
  ASSERT_EQ(count, 0);
}
