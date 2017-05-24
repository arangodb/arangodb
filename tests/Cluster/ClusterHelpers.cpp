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
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include "Cluster/ClusterHelpers.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

TEST_CASE("comparing server lists", "[cluster][helpers]") {

SECTION("comparing non array slices will return false") {
  VPackBuilder a;
  VPackBuilder b;

  REQUIRE(ClusterHelpers::compareServerLists(a.slice(), b.slice()) == false);
}

SECTION("comparing same server vpack lists returns true") {
  VPackBuilder a;
  VPackBuilder b;

  {
    VPackArrayBuilder aa(&a);
    a.add(VPackValue("test"));
  }
  {
    VPackArrayBuilder ba(&b);
    b.add(VPackValue("test"));
  }
  INFO(a.toJson());
  INFO(b.toJson());
  REQUIRE(ClusterHelpers::compareServerLists(a.slice(), b.slice()) == true);
}

SECTION("comparing same server lists returns true") {
  std::vector<std::string> a {"test"};
  std::vector<std::string> b {"test"};

  REQUIRE(ClusterHelpers::compareServerLists(a, b) == true);
}

SECTION("comparing same server lists with multiple entries returns true") {
  std::vector<std::string> a {"test", "test1", "test2"};
  std::vector<std::string> b {"test", "test1", "test2"};

  REQUIRE(ClusterHelpers::compareServerLists(a, b) == true);
}

SECTION("comparing different server lists with multiple entries returns false") {
  std::vector<std::string> a {"test", "test1"};
  std::vector<std::string> b {"test", "test1", "test2"};

  REQUIRE(ClusterHelpers::compareServerLists(a, b) == false);
}

SECTION("comparing different server lists with multiple entries returns false 2") {
  std::vector<std::string> a {"test", "test1", "test2"};
  std::vector<std::string> b {"test", "test1"};

  REQUIRE(ClusterHelpers::compareServerLists(a, b) == false);
}

SECTION("comparing different server lists with multiple entries BUT same contents returns true") {
  std::vector<std::string> a {"test", "test1", "test2"};
  std::vector<std::string> b {"test", "test2", "test1"};

  REQUIRE(ClusterHelpers::compareServerLists(a, b) == true);
}

SECTION("comparing different server lists with multiple entries but different leader returns false") {
  std::vector<std::string> a {"test", "test1", "test2"};
  std::vector<std::string> b {"test2", "test", "test1"};

  REQUIRE(ClusterHelpers::compareServerLists(a, b) == false);
}

}