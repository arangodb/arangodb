////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "Basics/StringUtils.h"
#include "Cluster/AgencyPaths.h"

#include <memory>
#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::cluster;
using namespace arangodb::cluster::paths;

// We don't want any class in the hierarchy to be publicly constructible.
// However, we can only check for specific constructors.
// TODO complete the list
#define CONSTRUCTIBLE_MESSAGE "This class should not be publicly constructible!"
static_assert(!std::is_default_constructible<Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Databases>::value, CONSTRUCTIBLE_MESSAGE);

// Exclude these on windows, because the constructor is made public there.
#ifndef _WIN32
static_assert(!std::is_constructible<Root::Arango, Root>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Databases, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
#endif
#undef CONSTRUCTIBLE_MESSAGE

class AgencyPathsTest : public ::testing::Test {
 protected:
  // Vector of {expected, actual} pairs.
  std::vector<std::pair<std::vector<std::string>, std::shared_ptr<Path const>>> ioPairs {
      {{"arango"}, Root().arango()},
      {{"arango", "Plan"}, Root().arango()->plan()},
      {{"arango", "Plan", "Databases"}, Root().arango()->plan()->databases()},
      {{"arango", "Plan", "Databases", "_system"}, Root().arango()->plan()->databases()->database(DatabaseID{"_system"})},
      {{"arango", "Plan", "Databases", "someCol"}, Root().arango()->plan()->databases()->database(DatabaseID{"someCol"})},
      {{"arango", "Current"}, Root().arango()->current()},
      {{"arango", "Current", "ServersRegistered"}, Root().arango()->current()->serversRegistered()},
  };
};

TEST_F(AgencyPathsTest, test_path_string) {
  for (auto const& it : ioPairs) {
    auto expected = std::string{"/"} + StringUtils::join(it.first, "/");
    auto actual = it.second->str();
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(AgencyPathsTest, test_path_pathvec) {
  for (auto const& it : ioPairs) {
    auto& expected = it.first;
    auto actual = it.second->vec();
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(AgencyPathsTest, test_path_stringstream) {
  for (auto const& it : ioPairs) {
    auto expected = std::string{"/"} + StringUtils::join(it.first, "/");
    std::stringstream stream;
    stream << *it.second;
    EXPECT_EQ(expected, stream.str());
  }
}

TEST_F(AgencyPathsTest, test_path_pathtostream) {
  for (auto const& it : ioPairs) {
    auto expected = std::string{"/"} + StringUtils::join(it.first, "/");
    std::stringstream stream;
    it.second->toStream(stream);
    EXPECT_EQ(expected, stream.str());
  }
}
