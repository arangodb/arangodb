////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::Manager
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
/// @author Matthew Von-Maszewski
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include "Cluster/ClusterComm.h"
#include "VocBase/ticks.h"

using namespace arangodb;

class ClusterCommTester : public ClusterComm {
public:
  ClusterCommTester()
    : ClusterComm(false)
  {}



};// class ClusterCommTester


TEST_CASE("Test ClusterComm::wait", "[cluster][mev]") {

  SECTION("no responses") {
    ClusterCommTester testme;
    ClusterCommResult result;
    CoordTransactionID id = TRI_NewTickServer();

    result = testme.wait("", id, 42, "", 100);
    REQUIRE(CL_COMM_DROPPED == result.status);
    REQUIRE(42 == result.operationID);
  } // no responses

}
