////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ClusterComm
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Agency/Job.h"
#include "Agency/Supervision.h"

#include "catch.hpp"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>
#include <typeinfo>

using namespace arangodb;
using namespace arangodb::consensus;

std::vector<std::string> servers {"XXX-XXX-XXX", "XXX-XXX-XXY"};

TEST_CASE("Supervision", "[agency][supervision]") {

  SECTION("Checking for the delete transaction 0 servers") {

    std::vector<std::string> todelete;
    auto const& transaction = removeTransactionBuilder(todelete);
    auto const& slice = transaction->slice();

    REQUIRE(slice.isArray());
    REQUIRE(slice.length() == 1);
    REQUIRE(slice[0].isArray());
    REQUIRE(slice[0].length() == 1);
    REQUIRE(slice[0][0].isObject());
    REQUIRE(slice[0][0].length() == 0);
    
  }

  SECTION("Checking for the delete transaction 1 server") {

    std::vector<std::string> todelete {servers[0]};
    auto const& transaction = removeTransactionBuilder(todelete);
    auto const& slice = transaction->slice();

    REQUIRE(slice.isArray());
    REQUIRE(slice.length() == 1);
    REQUIRE(slice[0].isArray());
    REQUIRE(slice[0].length() == 1);
    REQUIRE(slice[0][0].isObject());
    REQUIRE(slice[0][0].length() == 1);
    for (size_t i = 0; i < slice[0][0].length(); ++i) {
      REQUIRE(
        slice[0][0].keyAt(i).copyString() ==
        Supervision::agencyPrefix() + arangodb::consensus::healthPrefix + servers[i]);
      REQUIRE(slice[0][0].valueAt(i).isObject());
      REQUIRE(slice[0][0].valueAt(i).keyAt(0).copyString() == "op");
      REQUIRE(slice[0][0].valueAt(i).valueAt(0).copyString() == "delete");
    }
    
    
  }
  
  SECTION("Checking for the delete transaction 2 server") {
    
    std::vector<std::string> todelete = servers;
    auto const& transaction = removeTransactionBuilder(todelete);
    auto const& slice = transaction->slice();
    
    REQUIRE(slice.isArray());
    REQUIRE(slice.length() == 1);
    REQUIRE(slice[0].isArray());
    REQUIRE(slice[0].length() == 1);
    REQUIRE(slice[0][0].isObject());
    REQUIRE(slice[0][0].length() == 2);
    for (size_t i = 0; i < slice[0][0].length(); ++i) {
      REQUIRE(
        slice[0][0].keyAt(i).copyString() ==
        Supervision::agencyPrefix() + arangodb::consensus::healthPrefix + servers[i]);
      REQUIRE(slice[0][0].valueAt(i).isObject());
      REQUIRE(slice[0][0].valueAt(i).keyAt(0).copyString() == "op");
      REQUIRE(slice[0][0].valueAt(i).valueAt(0).copyString() == "delete");
    }
    
  }

}





