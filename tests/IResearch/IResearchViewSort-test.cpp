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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include "IResearch/IResearchViewSort.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

TEST_CASE("IResearchViewSortTest", "[iresearch][iresearch-viewsort]") {

SECTION("test_defaults") {
  arangodb::iresearch::IResearchViewSort sort;
  CHECK(sort.empty());
  CHECK(0 == sort.size());
  CHECK(sort.memory() > 0);

  arangodb::velocypack::Builder builder;
  CHECK(!sort.toVelocyPack(builder));
  {
    arangodb::velocypack::ArrayBuilder arrayScope(&builder);
    CHECK(sort.toVelocyPack(builder));
  }
  auto slice = builder.slice();
  CHECK(slice.isArray());
  CHECK(0 == slice.length());
}

}
