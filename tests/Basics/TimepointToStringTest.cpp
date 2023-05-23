////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/Common.h"
#include "Basics/TimeString.h"

#include "fmt/core.h"
#include "fmt/chrono.h"

TEST(TimePointToString, converts_min) {
    auto const& tp = std::chrono::system_clock::time_point::min();
    time_t tt = std::chrono::system_clock::to_time_t(tp);
    fmt::print("tt: {}", (intmax_t)tt);

    auto s = timepointToString(tp);
    fmt::print("{:%Y-%m-%d %H:%M:%S}", tp);
    ASSERT_EQ(s, "-290308-12-21T19:59:");
}
