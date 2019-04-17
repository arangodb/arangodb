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

#include "catch.hpp"

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
  builder.add(parser.steal()->slice());
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

TEST_CASE("HotBackup on coordinators", "[cluster][hotbackup]") {
  VPackBuilder pb = parseToBuilder(planFile);
  VPackSlice plan = pb.slice();
  

  SECTION("Test db server matching") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 0);
    REQUIRE(res.ok());
  }

  SECTION("Test first db server new") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }

    dbServers.front() = std::string("PRMR_")
      + to_string(boost::uuids::random_generator()());
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 1);
    REQUIRE(res.ok());
  }

  SECTION("Test last db server new") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }

    dbServers.back() = std::string("PRMR_")
      + to_string(boost::uuids::random_generator()());
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 1);
    REQUIRE(res.ok());
  }

  SECTION("Test first and last db server new") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }

    dbServers.front() = std::string("PRMR_")
      + to_string(boost::uuids::random_generator()());
    dbServers.back() = std::string("PRMR_")
      + to_string(boost::uuids::random_generator()());
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 2);
    REQUIRE(res.ok());
  }

  SECTION("Test all db server new") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (size_t i = 0; i < plan.get("DBServers").length(); ++i) {
      dbServers.push_back(std::string("PRMR_")
                          + to_string(boost::uuids::random_generator()()));
    }
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 3);
    REQUIRE(res.ok());
  }

  SECTION("Test one more local server than in backup") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }
    dbServers.push_back(std::string("PRMR_")
                        + to_string(boost::uuids::random_generator()()));
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 0);
    REQUIRE(res.ok());
  }

  SECTION("Test one less local server than in backup") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }
    dbServers.pop_back();
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(!res.ok());
    REQUIRE(matches.size() == 0);
  }

  SECTION("Test effect of first db server changed on new plan") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }
    dbServers.front() = std::string("PRMR_")
      + to_string(boost::uuids::random_generator()());
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(res.ok());

    VPackBuilder newPlan;
    res = applyDBServerMatchesToPlan(plan, matches, newPlan);

    for (auto const& m : matches) {
      REQUIRE(countSubstring(newPlan.slice().get("Collections").toJson(), m.first) == 0);
      REQUIRE(countSubstring(plan.get("Collections").toJson(), m.second) == 0);
      REQUIRE(countSubstring(newPlan.slice().get("Collections").toJson(), m.second) ==
        countSubstring(plan.get("Collections").toJson(), m.first));
    }
    
  }

  SECTION("Test effect of first and last db servers changed on new plan") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }
    dbServers.front() = std::string("PRMR_")
      + to_string(boost::uuids::random_generator()());
    dbServers.back() = std::string("PRMR_")
      + to_string(boost::uuids::random_generator()());
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(res.ok());

    VPackBuilder newPlan;
    res = applyDBServerMatchesToPlan(plan, matches, newPlan);

    VPackSlice const nplan = newPlan.slice();
    for (auto const& m : matches) {
      REQUIRE(countSubstring(nplan.get("Collections").toJson(), m.first) == 0);
      REQUIRE(countSubstring(plan.get("Collections").toJson(), m.second) == 0);
      REQUIRE(countSubstring(nplan.get("Collections").toJson(), m.second) ==
        countSubstring(plan.get("Collections").toJson(), m.first));
    }
    
  }

  SECTION("Test effect of all db servers changed on new plan") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    for (size_t i = 0; i < plan.get("DBServers").length(); ++i) {
      dbServers.push_back(std::string("PRMR_")
                          + to_string(boost::uuids::random_generator()()));
    }
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(res.ok());

    VPackBuilder newPlan;
    res = applyDBServerMatchesToPlan(plan, matches, newPlan);

    VPackSlice const nplan = newPlan.slice();
    for (auto const& m : matches) {
      REQUIRE(countSubstring(nplan.get("Collections").toJson(), m.first) == 0);
      REQUIRE(countSubstring(plan.get("Collections").toJson(), m.second) == 0);
      REQUIRE(countSubstring(nplan.get("Collections").toJson(), m.second) ==
        countSubstring(plan.get("Collections").toJson(), m.first));
    }
    
  }

  SECTION("Test irrelevance of string size for db server id") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::map<ServerID,ServerID> matches;

    dbServers.push_back("PRMR_abcdefg");
    dbServers.push_back("1c0");
    dbServers.push_back("Hello");
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(res.ok());

    VPackBuilder newPlan;
    res = applyDBServerMatchesToPlan(plan, matches, newPlan);

    VPackSlice const nplan = newPlan.slice();
    for (auto const& m : matches) {
      REQUIRE(countSubstring(nplan.get("Collections").toJson(), m.first) == 0);
      REQUIRE(countSubstring(plan.get("Collections").toJson(), m.second) == 0);
      REQUIRE(countSubstring(nplan.get("Collections").toJson(), m.second) ==
        countSubstring(plan.get("Collections").toJson(), m.first));
    }
    
  }

}


#endif
