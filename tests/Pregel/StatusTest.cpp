////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include <date/date.h>
#include <chrono>
#include <optional>
#include "Pregel/Common.h"
#include "Pregel/Status/ConductorStatus.h"
#include "Pregel/Status/Status.h"

#include "Pregel/Status/ConductorStatus.h"
#include "gtest/gtest.h"

using namespace arangodb::pregel;

TEST(PregelStatus,
     adding_two_status_gives_a_status_with_the_most_recent_timestamp) {
  auto earlierStatus =
      Status{.timeStamp = date::sys_days{date::March / 4 / 2020}};
  auto laterStatus =
      Status{.timeStamp = date::sys_days{date::March / 7 / 2020}};

  ASSERT_EQ(earlierStatus + laterStatus,
            Status{.timeStamp = laterStatus.timeStamp});
}

TEST(PregelGraphStoreStatus, adding_two_status_adds_measurements) {
  auto earlierStatus = GraphStoreStatus{.verticesLoaded = 2,
                                        .edgesLoaded = 119,
                                        .memoryBytesUsed = 92228,
                                        .verticesStored = 1};
  auto laterStatus = GraphStoreStatus{.verticesLoaded = 987,
                                      .edgesLoaded = 1,
                                      .memoryBytesUsed = 322,
                                      .verticesStored = 0};

  ASSERT_EQ(earlierStatus + laterStatus,
            (GraphStoreStatus{.verticesLoaded = 989,
                              .edgesLoaded = 120,
                              .memoryBytesUsed = 92550,
                              .verticesStored = 1}));
}

TEST(PregelGraphStoreStatus,
     empty_option_measurements_are_discarded_when_adding_two_status) {
  auto earlierStatus = GraphStoreStatus{.verticesLoaded = std::nullopt,
                                        .edgesLoaded = std::nullopt,
                                        .memoryBytesUsed = 92228,
                                        .verticesStored = 1};
  auto laterStatus = GraphStoreStatus{.verticesLoaded = std::nullopt,
                                      .edgesLoaded = 1,
                                      .memoryBytesUsed = std::nullopt,
                                      .verticesStored = 4};

  ASSERT_EQ(earlierStatus + laterStatus,
            (GraphStoreStatus{.verticesLoaded = std::nullopt,
                              .edgesLoaded = 1,
                              .memoryBytesUsed = 92228,
                              .verticesStored = 5}));
}

TEST(PregelGssGssStatus, adding_two_status_adds_mutual_gss_status) {
  auto statusWith2Gss =
      AllGssStatus{.gss = {GssStatus{.verticesProcessed = 1},
                           GssStatus{.verticesProcessed = 10}}};
  auto statusWith1Gss =
      AllGssStatus{.gss = {GssStatus{.verticesProcessed = 2}}};

  ASSERT_EQ(statusWith2Gss + statusWith1Gss,
            (AllGssStatus{.gss = {GssStatus{.verticesProcessed = 3}}}));
  ASSERT_EQ(statusWith1Gss + statusWith2Gss,
            (AllGssStatus{.gss = {GssStatus{.verticesProcessed = 3}}}));
}

TEST(PregelConductorStatus, accumulates_worker_status) {
  auto workers = std::unordered_map<arangodb::ServerID, Status>{
      {"worker_with_later_status",
       Status{.timeStamp = date::sys_days{date::March / 7 / 2020},
              .graphStoreStatus = GraphStoreStatus{.verticesLoaded = 2,
                                                   .edgesLoaded = 119,
                                                   .memoryBytesUsed = 92228,
                                                   .verticesStored = 1},
              .allGssStatus = std::nullopt}},
      {"worker_with_earlier_status",
       Status{.timeStamp = date::sys_days{date::March / 4 / 2020},
              .graphStoreStatus = GraphStoreStatus{.verticesLoaded = 987,
                                                   .edgesLoaded = 1,
                                                   .memoryBytesUsed = 322,
                                                   .verticesStored = 0},
              .allGssStatus =
                  AllGssStatus{.gss = {GssStatus{.verticesProcessed = 3}}}}}};
  auto conductorStatus = ConductorStatus{.workers = workers};

  ASSERT_EQ(
      conductorStatus.accumulate(),
      (AccumulatedConductorStatus{
          .status =
              Status{
                  .timeStamp = date::sys_days{date::March / 7 / 2020},
                  .graphStoreStatus = GraphStoreStatus{.verticesLoaded = 989,
                                                       .edgesLoaded = 120,
                                                       .memoryBytesUsed = 92550,
                                                       .verticesStored = 1},
                  .allGssStatus =
                      AllGssStatus{.gss = {GssStatus{.verticesProcessed = 3}}}},
          .workers = workers}));
}
