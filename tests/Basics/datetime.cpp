////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Endpoint classes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017-2019 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <date/date.h>
#include <velocypack/StringRef.h>

#include "Basics/datetime.h"
#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::basics;

TEST(DateTimeTest, testing) {
  using namespace std::chrono;
  using namespace date;

  tp_sys_clock_ms tp;

  std::vector<std::string> dates{"2017", "2017-11", "2017-11-12"};
  std::vector<std::string> times{"",
                                 "T12:34",
                                 "T12:34+10:22",
                                 "T12:34-10:22",

                                 "T12:34:56",
                                 "T12:34:56+10:22",
                                 "T12:34:56-10:22",

                                 "T12:34:56.789",
                                 "T12:34:56.789+10:22",
                                 "T12:34:56.789-10:22"};

  std::vector<std::string> datesToTest{};

  for (auto const& d : dates) {
    for (auto const& t : times) {
      datesToTest.push_back(d + t);
    }
  }

  std::vector<std::string> datesToFail{"2017-01-01-12", "2017-01-01:12:34",
                                       "2017-01-01:12:34Z+10:20",
                                       "2017-01-01:12:34Z-10:20"};

  for (auto const& dateTime : datesToTest) {
    bool ret = parseDateTime(arangodb::velocypack::StringRef(dateTime), tp);
    ASSERT_TRUE(ret);
  }

  for (auto const& dateTime : datesToFail) {
    bool ret = parseDateTime(arangodb::velocypack::StringRef(dateTime), tp);
    ASSERT_FALSE(ret);
  }
}
