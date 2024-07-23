////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Basics/SourceLocation.h"

#include <filesystem>
#include <ranges>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace arangodb::basics;

TEST(SourceLocationTest, filename_prefix) {
  using namespace std::ranges::views;
  using namespace testing;
  namespace fs = std::filesystem;

  // This path should end in .../tests/Basics/SourceLocationTest.cpp
  auto const path = fs::path(__FILE__);

  auto pathFromContentRoot = std::vector<std::string>{};
  std::ranges::copy(path | reverse | take(3) | reverse,
                    std::back_inserter(pathFromContentRoot));

  // Just checking that the test is in its expected environment.
  ASSERT_THAT(pathFromContentRoot,
              ElementsAre("tests", "Basics", "SourceLocationTest.cpp"));

  {
    // now compare that with SourceLocation
    auto loc = SourceLocation::current();
    auto fn = loc.file_name();
    auto expected = fs::path("tests") / fs::path("Basics") /
                    fs::path("SourceLocationTest.cpp");
    ASSERT_STREQ(fn, expected.c_str());
  }
}

TEST(SourceLocationTest, line) {
  ASSERT_EQ(SourceLocation::current().line(), __LINE__);
}
