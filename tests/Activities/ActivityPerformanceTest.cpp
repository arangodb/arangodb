#include "Activities/GenericActivity.h"
#include "Activities/RegistryGlobalVariable.h"
#include "Logger/LogMacros.h"

#include <gtest/gtest.h>
#include <chrono>
#include <stop_token>
#include <thread>
#include <latch>
#include <list>
#include <format>
#include <iostream>

using namespace arangodb;
using namespace arangodb::activities;
using namespace std::chrono_literals;

struct ActivityPerformanceTest : ::testing::Test {};

/**
   Before running this test you should
   - be sure to execute non-maintainer mode
   - fix CPU frequency

   What could also be interesting to look at:
   - execute gc when there are a lot of long-living activities
   - involve snapshot-taking as well: how long does snapshot-creating need, how
     does it influence gc and addition
 */
TEST_F(ActivityPerformanceTest,
       create_a_lot_of_very_short_activites_and_execute_gc_in_separate_thread) {
  auto const numberOfCreationThreads = 1;

  std::latch latch{numberOfCreationThreads + 1};
  std::stop_source stop;
  std::list<std::jthread> threads;

  for (int i = 0; i < numberOfCreationThreads; i++) {
    threads.emplace_back(
        [&latch](std::stop_token token) {
          latch.arrive_and_wait();
          uint64_t creation_counter = 0;
          std::chrono::steady_clock::duration creation_duration;
          auto last = std::chrono::steady_clock::now();
          while (not token.stop_requested()) {
            auto before_creation = std::chrono::steady_clock::now();
            // this activity is directly deleted again
            activities::make<activities::GenericActivity>(
                "activity", activities::GenericActivityData{});
            creation_duration +=
                std::chrono::steady_clock::now() - before_creation;
            creation_counter++;

            std::this_thread::sleep_for(0us);

            auto now = std::chrono::steady_clock::now();
            if (now - last >= 1s) {
              std::cerr
                  << std::format(
                         std::locale("en_US.UTF-8"),
                         "#creations: {:L} - time for creation: {:L} - time "
                         "per "
                         "creation: {:L}us",
                         creation_counter,
                         std::chrono::duration_cast<std::chrono::microseconds>(
                             creation_duration),
                         creation_duration.count() / creation_counter)
                  << std::endl;
              creation_counter = 0;
              creation_duration = std::chrono::steady_clock::duration{};
              last = now;
            }
          }
        },
        stop.get_token());
  }

  threads.emplace_back(
      [&latch](std::stop_token token) {
        latch.arrive_and_wait();
        while (not token.stop_requested()) {
          auto start = std::chrono::steady_clock::now();
          registry.run_external_cleanup();
          auto duration = std::chrono::steady_clock::now() - start;
          std::cerr
              << std::format(
                     std::locale("en_US.UTF-8"), "time for gc: {:L}",
                     std::chrono::duration_cast<std::chrono::microseconds>(
                         duration))
              << std::endl;

          std::this_thread::sleep_for(1s - duration);
        }
      },
      stop.get_token());
}
