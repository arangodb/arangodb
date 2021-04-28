////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Ignacio Rodriguez
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Agency/Store.h"
#include "Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/Compare.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace tests {
namespace store_test_api {

class StoreTestAPI : public ::testing::Test {
 public:
  StoreTestAPI() : _server(), _store(_server.server(), nullptr) {
  }
 protected:  
  arangodb::tests::mocks::MockCoordinator _server;
  arangodb::consensus::Store _store;
};

TEST_F(StoreTestAPI, our_first_test) {
  std::shared_ptr<VPackBuilder> q
    = VPackParser::fromJson(R"=(
        [[{"/": {"op":"delete"}}]]
      )=");
  std::vector<consensus::apply_ret_t> v = _store.applyTransactions(q);
  ASSERT_EQ(1, v.size());
  ASSERT_EQ(consensus::APPLIED, v[0]);
  
  q = VPackParser::fromJson(R"=(
        ["/x"]
      )=");
  VPackBuilder result;
  ASSERT_TRUE(_store.read(q->slice(), result));
  VPackSlice res{result.slice()};
#if 0
  ASSERT_TRUE(res.isArray() && res.length() == 1);
  res = res[0];
#endif
  ASSERT_TRUE(res.isObject() && res.length() == 0);
  auto j = VPackParser::fromJson(R"=(
       {}
     )=");
  ASSERT_TRUE(velocypack::NormalizedCompare::equals(j->slice(), result.slice()));
}

}  // namespace
}  // namespace
}  // namespace
