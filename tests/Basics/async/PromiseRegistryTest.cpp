#include "Basics/async/promise.hpp"
#include "Basics/async/promise_registry.hpp"
#include "Basics/Result.h"

#include <gtest/gtest.h>
#include <source_location>
#include <thread>

using namespace arangodb::coroutine;

TEST(PromiseRegistryTest, adds_a_promise) {
  auto promise_registry = PromiseRegistryOnThread{};

  auto promise = PromiseInList(std::source_location::current());
  auto line_nr = promise._where.line();
  promise_registry.add(&promise);

  std::vector<uint> promise_lines;
  promise_registry.for_promise([&](PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });
  EXPECT_EQ(promise_lines, std::vector<uint>{line_nr});
}

TEST(PromiseRegistryTest, another_thread_cannot_add_a_promise) {
  auto promise_registry = PromiseRegistryOnThread{};

  std::jthread([&]() {
    auto promise = PromiseInList(std::source_location::current());
    EXPECT_TRUE(promise_registry.add(&promise).fail());
  });
}

TEST(PromiseRegistryTest, iterates_over_all_promises) {
  auto promise_registry = PromiseRegistryOnThread{};
  auto promise_1 = PromiseInList(std::source_location::current());
  auto line_nr_1 = promise_1._where.line();
  promise_registry.add(&promise_1);
  auto promise_2 = PromiseInList(std::source_location::current());
  auto line_nr_2 = promise_2._where.line();
  promise_registry.add(&promise_2);
  auto promise_3 = PromiseInList(std::source_location::current());
  auto line_nr_3 = promise_3._where.line();
  promise_registry.add(&promise_3);

  std::vector<uint> promise_lines;
  promise_registry.for_promise([&](PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });

  EXPECT_EQ(promise_lines,
            (std::vector<uint>{line_nr_3, line_nr_2, line_nr_1}));
}

TEST(PromiseRegistryTest, iterates_in_another_thread_over_all_promises) {
  auto promise_registry = PromiseRegistryOnThread{};
  auto promise_1 = PromiseInList(std::source_location::current());
  auto line_nr_1 = promise_1._where.line();
  promise_registry.add(&promise_1);
  auto promise_2 = PromiseInList(std::source_location::current());
  auto line_nr_2 = promise_2._where.line();
  promise_registry.add(&promise_2);
  auto promise_3 = PromiseInList(std::source_location::current());
  auto line_nr_3 = promise_3._where.line();
  promise_registry.add(&promise_3);

  std::jthread([&]() {
    std::vector<uint> promise_lines;
    promise_registry.for_promise([&](PromiseInList* promise) {
      promise_lines.push_back(promise->_where.line());
    });
    EXPECT_EQ(promise_lines,
              (std::vector<uint>{line_nr_3, line_nr_2, line_nr_1}));
  });
}

TEST(PromiseRegistryTest, erases_a_promise_at_the_start_of_the_list) {
  auto promise_registry = PromiseRegistryOnThread{};
  auto promise_1 = PromiseInList(std::source_location::current());
  promise_registry.add(&promise_1);
  auto promise_2 = PromiseInList(std::source_location::current());
  auto line_nr_2 = promise_2._where.line();
  promise_registry.add(&promise_2);

  promise_registry.erase(&promise_1);

  std::vector<uint> promise_lines;
  promise_registry.for_promise([&](PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });
  EXPECT_EQ(promise_lines, (std::vector<uint>{line_nr_2}));
}

TEST(PromiseRegistryTest, erases_a_promise_in_the_middle_of_the_list) {
  auto promise_registry = PromiseRegistryOnThread{};
  auto promise_1 = PromiseInList(std::source_location::current());
  auto line_nr_1 = promise_1._where.line();
  promise_registry.add(&promise_1);
  auto promise_2 = PromiseInList(std::source_location::current());
  promise_registry.add(&promise_2);
  auto promise_3 = PromiseInList(std::source_location::current());
  auto line_nr_3 = promise_3._where.line();
  promise_registry.add(&promise_3);

  promise_registry.erase(&promise_2);

  std::vector<uint> promise_lines;
  promise_registry.for_promise([&](PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });
  EXPECT_EQ(promise_lines, (std::vector<uint>{line_nr_3, line_nr_1}));
}

TEST(PromiseRegistryTest, erases_a_promise_at_the_end_of_the_list) {
  auto promise_registry = PromiseRegistryOnThread{};
  auto promise_1 = PromiseInList(std::source_location::current());
  auto line_nr_1 = promise_1._where.line();
  promise_registry.add(&promise_1);
  auto promise_2 = PromiseInList(std::source_location::current());
  promise_registry.add(&promise_2);

  promise_registry.erase(&promise_2);

  std::vector<uint> promise_lines;
  promise_registry.for_promise([&](PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });
  EXPECT_EQ(promise_lines, (std::vector<uint>{line_nr_1}));
}

TEST(PromiseRegistryTest, another_thread_cannot_erase_a_promise) {
  auto promise_registry = PromiseRegistryOnThread{};
  auto promise = PromiseInList(std::source_location::current());

  std::jthread([&]() { EXPECT_TRUE(promise_registry.erase(&promise).fail()); });
}

// TODO
TEST(PromiseRegistryTest, erases_a_existing_promise_although_not_in_list) {
  auto promise_registry = PromiseRegistryOnThread{};
  auto promise_in_list = PromiseInList(std::source_location::current());
  auto line_nr = promise_in_list._where.line();
  promise_registry.add(&promise_in_list);

  auto free_promise = PromiseInList(std::source_location::current());
  EXPECT_TRUE(promise_registry.erase(&free_promise).ok());

  std::vector<uint> promise_lines;
  promise_registry.for_promise([&](PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });
  EXPECT_EQ(promise_lines, (std::vector<uint>{line_nr}));
}

// TODO
TEST(PromiseRegistryTest, does_not_delete_while_iterating) {}
