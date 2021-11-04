////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Basics/VelocyPackHelper.h"
#include "velocypack/Builder.h"
#include "velocypack/Options.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/types.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::basics;

static inline auto vpackFromJsonString(char const* c) -> VPackBuffer<uint8_t> {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  std::shared_ptr<VPackBuilder> builder = parser.steal();
  std::shared_ptr<VPackBuffer<uint8_t>> buffer = builder->steal();
  VPackBuffer<uint8_t> b = std::move(*buffer);
  return b;
}

static inline auto operator"" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}

TEST(LogStatusTest, FirstSimpleTest) {
  TermIndexPair pair;
  pair.index = LogIndex{12};
  pair.term = LogTerm{5};
  VPackBuilder builder;
  pair.toVelocyPack(builder);
  TermIndexPair newPair = TermIndexPair::fromVelocyPack(builder.slice());
  EXPECT_EQ(pair, newPair);
}
/*
TEST(LogStatusTest, SecondSimpleTest) {
  auto jsonBuffer = "{\"term\":5, \"index\":12, \"foo\":[12]}"_vpack;
  auto json = velocypack::Slice(jsonBuffer.data());
  TermIndexPair pair = TermIndexPair::fromVelocyPack(json);
  VPackBuilder builder;
  pair.toVelocyPack(builder);
  EXPECT_TRUE(VelocyPackHelper::equal(json, builder.slice(), true)) << "expected = " << json.toJson() << " found = " << builder.toJson();
}
 */
/*
"local": {
"commitIndex": 2,
"firstIndex": 1,
"spearhead": {
"term": 2,
"index": 2
}
*/