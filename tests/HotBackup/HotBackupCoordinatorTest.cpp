////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Cluster maintenance
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Cluster/ClusterMethods.h"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include <fstream>
#include <iterator>
#include <iostream>
#include <random>
#include <typeinfo>

using namespace arangodb;

#ifndef _WIN32

char const* planFile =
#include "../Maintenance/Plan.json"
;

VPackBuilder parseToBuilder(char const* c) {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  VPackBuilder builder;
  {
    VPackObjectBuilder o(&builder);
    builder.add(VPackValue("arango"));
    {
      VPackObjectBuilder o(&builder);
      builder.add("Plan", parser.steal()->slice());
    }
  }
  return builder;

}

std::string const PLAN_COL_PATH = "/Collections/";
std::string const PLAN_DB_PATH = "/Databases/";

int countSubstring(std::string const& str, std::string const& sub) {
  if (sub.length() == 0) return 0;
  int count = 0;
  for (size_t offset = str.find(sub); offset != std::string::npos;
       offset = str.find(sub, offset + sub.length())) {
    ++count;
  }
  return count;
}

std::vector<std::string> dbsPath {"arango","Plan","DBServers"};
std::vector<std::string> colPath {"arango","Plan","Collections"};

class HotBackupOnCoordinators : public ::testing::Test {
protected:
  HotBackupOnCoordinators () {
    pb = parseToBuilder(planFile);
    plan = pb.slice();
  }
  VPackBuilder pb;
  VPackSlice plan;
}


TEST_F(HotBackupOnCoordinators, test_dbserver_matching) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (auto const& i : VPackObjectIterator(plan.get(dbsPath))) {
    dbServers.push_back(i.key.copyString());
  }

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_EQ(matches.size(), 0);
  ASSERT_TRUE(res.ok());
}

TEST_F(HotBackupOnCoordinators, test_first_dbserver_new) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (auto const& i : VPackObjectIterator(plan.get(dbsPath))) {
    dbServers.push_back(i.key.copyString());
  }

  dbServers.front() = std::string("PRMR_")
    + to_string(boost::uuids::random_generator()());

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_EQ(matches.size(), 1);
  ASSERT_TRUE(res.ok());
}

TEST_F(HotBackupOnCoordinators, test_last_dbserver_new) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (auto const& i : VPackObjectIterator(plan.get(dbsPath))) {
    dbServers.push_back(i.key.copyString());
  }

  dbServers.back() = std::string("PRMR_")
    + to_string(boost::uuids::random_generator()());

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_EQ(matches.size(), 1);
  ASSERT_TRUE(res.ok());
}

TEST_F(HotBackupOnCoordinators, test_first_and_last_dbserver_new) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (auto const& i : VPackObjectIterator(plan.get(dbsPath))) {
    dbServers.push_back(i.key.copyString());
  }

  dbServers.front() = std::string("PRMR_")
    + to_string(boost::uuids::random_generator()());
  dbServers.back() = std::string("PRMR_")
    + to_string(boost::uuids::random_generator()());

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_EQ(matches.size(), 2);
  ASSERT_TRUE(res.ok());
}

TEST_F(HotBackupOnCoordinators, test_all_dbserver_new) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (size_t i = 0; i < plan.get(dbsPath).length(); ++i) {
    dbServers.push_back(std::string("PRMR_")
                        + to_string(boost::uuids::random_generator()()));
  }

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_EQ(matches.size(), 3);
  ASSERT_TRUE(res.ok());
}

TEST_F(HotBackupOnCoordinators, test_one_more_local_server_than_in_backup) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (auto const& i : VPackObjectIterator(plan.get(dbsPath))) {
    dbServers.push_back(i.key.copyString());
  }
  dbServers.push_back(std::string("PRMR_")
                      + to_string(boost::uuids::random_generator()()));

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_EQ(matches.size(), 0);
  ASSERT_TRUE(res.ok());
}

TEST_F(HotBackupOnCoordinators, test_one_less_local_server_than_in_backup) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (auto const& i : VPackObjectIterator(plan.get(dbsPath))) {
    dbServers.push_back(i.key.copyString());
  }
  dbServers.pop_back();

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_TRUE(!res.ok());
  ASSERT_EQ(matches.size(), 0);
}

TEST_F(HotBackupOnCoordinators, test_effect_of_first_dbserver_changed_on_new_plab) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (auto const& i : VPackObjectIterator(plan.get(dbsPath))) {
    dbServers.push_back(i.key.copyString());
  }
  dbServers.front() = std::string("PRMR_")
    + to_string(boost::uuids::random_generator()());

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_TRUE(res.ok());

  VPackBuilder newPlan;
  res = applyDBServerMatchesToPlan(plan, matches, newPlan);

  for (auto const& m : matches) {
    ASSERT_EQ(countSubstring(newPlan.slice().get(colPath).toJson(), m.first), 0);
    ASSERT_EQ(countSubstring(plan.get(colPath).toJson(), m.second), 0);
    ASSERT_EQ(countSubstring(newPlan.slice().get(colPath).toJson(), m.second),
      countSubstring(plan.get(colPath).toJson(), m.first));
  }

}

TEST_F(HotBackupOnCoordinators, test_effect_of_first_and_last_dbservers_changed_on_new_pan) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (auto const& i : VPackObjectIterator(plan.get(dbsPath))) {
    dbServers.push_back(i.key.copyString());
  }
  dbServers.front() = std::string("PRMR_")
    + to_string(boost::uuids::random_generator()());
  dbServers.back() = std::string("PRMR_")
    + to_string(boost::uuids::random_generator()());

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_TRUE(res.ok());

  VPackBuilder newPlan;
  res = applyDBServerMatchesToPlan(plan, matches, newPlan);

  VPackSlice const nplan = newPlan.slice();
  for (auto const& m : matches) {
    ASSERT_EQ(countSubstring(nplan.get(colPath).toJson(), m.first), 0);
    ASSERT_EQ(countSubstring(plan.get(colPath).toJson(), m.second), 0);
    ASSERT_EQ(countSubstring(nplan.get(colPath).toJson(), m.second),
      countSubstring(plan.get(colPath).toJson(), m.first));
  }

}

TEST_F(HotBackupOnCoordinators, test_effect_of_all_dbservers_changed_on_new_plan) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  for (size_t i = 0; i < plan.get(dbsPath).length(); ++i) {
    dbServers.push_back(std::string("PRMR_")
                        + to_string(boost::uuids::random_generator()()));
  }

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_TRUE(res.ok());

  VPackBuilder newPlan;
  res = applyDBServerMatchesToPlan(plan, matches, newPlan);

  VPackSlice const nplan = newPlan.slice();
  for (auto const& m : matches) {
    ASSERT_EQ(countSubstring(nplan.get(colPath).toJson(), m.first), 0);
    ASSERT_EQ(countSubstring(plan.get(colPath).toJson(), m.second), 0);
    ASSERT_EQ(countSubstring(nplan.get(colPath).toJson(), m.second),
      countSubstring(plan.get(colPath).toJson(), m.first));
  }

}

TEST_F(HotBackupOnCoordinators, test_irrelevance_of_string_size_for_dbserver_id) {
  std::shared_ptr<VPackBuilder> props;

  std::vector<ServerID> dbServers;
  std::map<ServerID,ServerID> matches;

  dbServers.push_back("PRMR_abcdefg");
  dbServers.push_back("1c0");
  dbServers.push_back("Hello");

  arangodb::Result res = matchBackupServers(plan, dbServers, matches);

  ASSERT_TRUE(res.ok());

  VPackBuilder newPlan;
  res = applyDBServerMatchesToPlan(plan, matches, newPlan);

  VPackSlice const nplan = newPlan.slice();
  for (auto const& m : matches) {
    ASSERT_EQ(countSubstring(nplan.get(colPath).toJson(), m.first), 0);
    ASSERT_EQ(countSubstring(plan.get(colPath).toJson(), m.second), 0);
    ASSERT_EQ(countSubstring(nplan.get(colPath).toJson(), m.second),
      countSubstring(plan.get(colPath).toJson(), m.first));
  }

}

#endif
