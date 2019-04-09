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

TEST_CASE("HotBackup", "[cluster][hotbackup]") {
  VPackBuilder pb = parseToBuilder(planFile);
  VPackSlice plan = pb.slice();
  

  SECTION("Test db server matching") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::unordered_map<ServerID,ServerID> matches;

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
    std::unordered_map<ServerID,ServerID> matches;

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
    std::unordered_map<ServerID,ServerID> matches;

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
    std::unordered_map<ServerID,ServerID> matches;

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
    std::unordered_map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }

    for (auto& i : dbServers) {
      i = std::string("PRMR_") + to_string(boost::uuids::random_generator()());
    }
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 3);
    REQUIRE(res.ok());
  }

  SECTION("Test one more local server than in backup") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::unordered_map<ServerID,ServerID> matches;

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
    std::unordered_map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }
    dbServers.pop_back();
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 0);
    REQUIRE(!res.ok());
  }

  SECTION("Test one less local server than in backup") {
    std::shared_ptr<VPackBuilder> props;

    std::vector<ServerID> dbServers;
    std::unordered_map<ServerID,ServerID> matches;

    for (auto const& i : VPackObjectIterator(plan.get("DBServers"))) {
      dbServers.push_back(i.key.copyString());
    }
    dbServers.front() = std::string("PRMR_")
      + to_string(boost::uuids::random_generator()());
    
    arangodb::Result res = matchBackupServers(plan, dbServers, matches);
    
    REQUIRE(matches.size() == 0);
    REQUIRE(!res.ok());
  }

}


#endif
