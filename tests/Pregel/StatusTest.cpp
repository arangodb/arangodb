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

TEST(PregelConductorStatus, accumulates_worker_status) {
  auto workers = std::unordered_map<arangodb::ServerID, Status>{
      {"worker_with_later_status",
       Status{.timeStamp = date::sys_days{date::March / 7 / 2020}}},
      {"worker_with_earlier_status",
       Status{.timeStamp = date::sys_days{date::March / 4 / 2020}}}};
  auto conductorStatus = ConductorStatus{.workers = workers};

  ASSERT_EQ(conductorStatus.accumulate(),
            (AccumulatedConductorStatus{
                .status = workers.at("worker_with_later_status"),
                .workers = workers}));
}
