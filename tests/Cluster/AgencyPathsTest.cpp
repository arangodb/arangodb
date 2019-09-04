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
#define CONSTRUCTIBLE_MESSAGE "This class should not be publicly constructible!"
// clang-format off

// First, default constructors
static_assert(!std::is_default_constructible<Root>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Databases>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Databases::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::State>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::State::Timestamp>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::State::Mode>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Shards>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::DbServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::SyncTime>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::Timestamp>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::SyncStatus>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::LastAckedTime>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::Host>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::Engine>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::Version>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::Status>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::ShortName>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::DbServer::Endpoint>::value, CONSTRUCTIBLE_MESSAGE);

// Exclude these on windows, because the constructor is made public there.
#ifndef _WIN32

// Second, constructors with parent
static_assert(!std::is_constructible<Root::Arango, Root>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Databases, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Databases::Database, Root::Arango::Plan::Databases>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::State, Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::State::Timestamp, Root::Arango::Supervision::State>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::State::Mode, Root::Arango::Supervision::State>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Shards, Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::DbServers, Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health, Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer, Root::Arango::Supervision::Health>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::SyncTime, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::Timestamp, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::SyncStatus, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::LastAckedTime, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::Host, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::Engine, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::Version, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::Status, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::ShortName, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer::Endpoint, Root::Arango::Supervision::Health::DbServer>::value, CONSTRUCTIBLE_MESSAGE);


// Third, constructors with parent and an additional string where applicable
static_assert(!std::is_constructible<Root::Arango::Plan::Databases::Database, Root::Arango::Plan::Databases, DatabaseID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::DbServer, Root::Arango::Supervision::Health, ServerID>::value, CONSTRUCTIBLE_MESSAGE);

#endif
// clang-format on
#undef CONSTRUCTIBLE_MESSAGE

class AgencyPathsTest : public ::testing::Test {
 protected:
  // Vector of {expected, actual} pairs.
  std::vector<std::pair<std::vector<std::string> const, std::shared_ptr<Path const> const>> const ioPairs{
      // clang-format off
      {{"arango"}, root()->arango()},
      {{"arango", "Plan"}, root()->arango()->plan()},
      {{"arango", "Plan", "Databases"}, root()->arango()->plan()->databases()},
      {{"arango", "Plan", "Databases", "_system"}, root()->arango()->plan()->databases()->database(DatabaseID{"_system"})},
      {{"arango", "Plan", "Databases", "someCol"}, root()->arango()->plan()->databases()->database(DatabaseID{"someCol"})},
      {{"arango", "Current"}, root()->arango()->current()},
      {{"arango", "Current", "ServersRegistered"}, root()->arango()->current()->serversRegistered()},
      {{"arango", "Supervision"}, root()->arango()->supervision()},
      {{"arango", "Supervision", "State"}, root()->arango()->supervision()->state()},
      {{"arango", "Supervision", "State", "Timestamp"}, root()->arango()->supervision()->state()->timestamp()},
      {{"arango", "Supervision", "State", "Mode"}, root()->arango()->supervision()->state()->mode()},
      {{"arango", "Supervision", "Shards"}, root()->arango()->supervision()->shards()},
      {{"arango", "Supervision", "DBServers"}, root()->arango()->supervision()->dbServers()},
      {{"arango", "Supervision", "Health"}, root()->arango()->supervision()->health()},
      {{"arango", "Supervision", "Health", "CRDN-1234"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")},
      {{"arango", "Supervision", "Health", "PRMR-5678", "SyncTime"}, root()->arango()->supervision()->health()->dbServer("PRMR-5678")->syncTime()},
      {{"arango", "Supervision", "Health", "PRMR-5678", "Timestamp"}, root()->arango()->supervision()->health()->dbServer("PRMR-5678")->timestamp()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "SyncStatus"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")->syncStatus()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "LastAckedTime"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")->lastAckedTime()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Host"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")->host()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Engine"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")->engine()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Version"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")->version()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Status"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")->status()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "ShortName"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")->shortName()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Endpoint"}, root()->arango()->supervision()->health()->dbServer("CRDN-1234")->endpoint()},
      // clang-format on
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
