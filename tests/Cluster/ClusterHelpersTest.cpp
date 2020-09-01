////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "Cluster/ClusterHelpers.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

TEST(ComparingServerListsTest, comparing_non_array_slices_will_return_false) {
  VPackBuilder a;
  VPackBuilder b;

  ASSERT_FALSE(ClusterHelpers::compareServerLists(a.slice(), b.slice()));
}

TEST(ComparingServerListsTest, comparing_same_server_vpack_lists_returns_true) {
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
  ASSERT_TRUE(ClusterHelpers::compareServerLists(a.slice(), b.slice()));
}

TEST(ComparingServerListsTest, comparing_same_server_lists_returns_true) {
  std::vector<std::string> a{"test"};
  std::vector<std::string> b{"test"};

  ASSERT_TRUE(ClusterHelpers::compareServerLists(a, b));
}

TEST(ComparingServerListsTest, comparing_same_server_lists_with_multiple_entries_returns_true) {
  std::vector<std::string> a{"test", "test1", "test2"};
  std::vector<std::string> b{"test", "test1", "test2"};

  ASSERT_TRUE(ClusterHelpers::compareServerLists(a, b));
}

TEST(ComparingServerListsTest,
     comparing_different_server_lists_with_multiple_entries_returns_false) {
  std::vector<std::string> a{"test", "test1"};
  std::vector<std::string> b{"test", "test1", "test2"};

  ASSERT_FALSE(ClusterHelpers::compareServerLists(a, b));
}

TEST(ComparingServerListsTest,
     comparing_different_server_lists_with_multiple_entries_returns_false_2) {
  std::vector<std::string> a{"test", "test1", "test2"};
  std::vector<std::string> b{"test", "test1"};

  ASSERT_FALSE(ClusterHelpers::compareServerLists(a, b));
}

TEST(ComparingServerListsTest,
     comparing_different_server_lists_with_multiple_entries_but_same_contents_returns_true) {
  std::vector<std::string> a{"test", "test1", "test2"};
  std::vector<std::string> b{"test", "test2", "test1"};

  ASSERT_TRUE(ClusterHelpers::compareServerLists(a, b));
}

TEST(ComparingServerListsTest,
     comparing_different_server_lists_with_multiple_entries_but_different_leader_returns_false) {
  std::vector<std::string> a{"test", "test1", "test2"};
  std::vector<std::string> b{"test2", "test", "test1"};

  ASSERT_FALSE(ClusterHelpers::compareServerLists(a, b));
}
