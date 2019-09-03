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

#include "Cluster/AgencyPaths.h"

using namespace arangodb;
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
  Root root;
};

static Root root{};

TEST_F(AgencyPathsTest, test_path_string) {
  std::string actual;
  actual = root.arango().pathStr();
  EXPECT_EQ("/arango", actual);
  actual = root.arango().plan().pathStr();
  EXPECT_EQ("/arango/Plan", actual);
  actual = root.arango().plan().databases().pathStr();
  EXPECT_EQ("/arango/Plan/Databases", actual);
  actual = root.arango().plan().databases()(DatabaseID{"_system"}).pathStr();
  EXPECT_EQ("/arango/Plan/Databases/_system", actual);
  actual = root.arango().current().pathStr();
  EXPECT_EQ("/arango/Current", actual);
  actual = root.arango().current().serversRegistered().pathStr();
  EXPECT_EQ("/arango/Current/ServersRegistered", actual);
}

TEST_F(AgencyPathsTest, test_path_pathvec) {
  using Res = std::vector<std::string>;
  Res actual;
  actual = root.arango().pathVec();
  EXPECT_EQ((Res{"arango"}), actual);
  actual = root.arango().plan().pathVec();
  EXPECT_EQ((Res{"arango", "Plan"}), actual);
  actual = root.arango().plan().databases().pathVec();
  EXPECT_EQ((Res{"arango", "Plan", "Databases"}), actual);
  actual = root.arango().plan().databases()(DatabaseID{"_system"}).pathVec();
  EXPECT_EQ((Res{"arango", "Plan", "Databases", "_system"}), actual);
  actual = root.arango().current().pathVec();
  EXPECT_EQ((Res{"arango", "Current"}), actual);
  actual = root.arango().current().serversRegistered().pathVec();
  EXPECT_EQ((Res{"arango", "Current", "ServersRegistered"}), actual);
}

TEST_F(AgencyPathsTest, test_path_stringstream) {
  std::stringstream actual;

  actual << root.arango();
  EXPECT_EQ("/arango", actual.str());
  actual.str(std::string{});

  actual << root.arango().plan();
  EXPECT_EQ("/arango/Plan", actual.str());
  actual.str(std::string{});

  actual << root.arango().plan().databases();
  EXPECT_EQ("/arango/Plan/Databases", actual.str());
  actual.str(std::string{});

  actual << root.arango().plan().databases()(DatabaseID{"_system"});
  EXPECT_EQ("/arango/Plan/Databases/_system", actual.str());
  actual.str(std::string{});

  actual << root.arango().current();
  EXPECT_EQ("/arango/Current", actual.str());
  actual.str(std::string{});

  actual << root.arango().current().serversRegistered();
  EXPECT_EQ("/arango/Current/ServersRegistered", actual.str());
  actual.str(std::string{});
}

TEST_F(AgencyPathsTest, test_path_pathtostream) {
  std::stringstream actual;

  root.pathToStream(actual);
  EXPECT_EQ("", actual.str());
  actual.str(std::string{});

  root.arango().pathToStream(actual);
  EXPECT_EQ("/arango", actual.str());
  actual.str(std::string{});

  root.arango().plan().pathToStream(actual);
  EXPECT_EQ("/arango/Plan", actual.str());
  actual.str(std::string{});

  root.arango().plan().databases().pathToStream(actual);
  EXPECT_EQ("/arango/Plan/Databases", actual.str());
  actual.str(std::string{});

  root.arango().plan().databases()(DatabaseID{"_system"}).pathToStream(actual);
  EXPECT_EQ("/arango/Plan/Databases/_system", actual.str());
  actual.str(std::string{});

  root.arango().current().pathToStream(actual);
  EXPECT_EQ("/arango/Current", actual.str());
  actual.str(std::string{});

  root.arango().current().serversRegistered().pathToStream(actual);
  EXPECT_EQ("/arango/Current/ServersRegistered", actual.str());
  actual.str(std::string{});
}
