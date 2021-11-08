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

namespace arangodb::replication2::replicated_log {

[[maybe_unused]] static inline auto operator==(LogStatistics const& left, LogStatistics const& right) {
  return left.spearHead == right.spearHead && left.commitIndex == right.commitIndex &&
         left.firstIndex == right.firstIndex;
}

}

TEST(LogStatusTest, term_index_pair_vpack) {
  auto spearHead = TermIndexPair{LogTerm{2}, LogIndex{1}};
  VPackBuilder builder;
  spearHead.toVelocyPack(builder);
  auto slice = builder.slice();
  auto fromVPack = TermIndexPair::fromVelocyPack(slice);
  EXPECT_EQ(spearHead, fromVPack);

  auto jsonBuffer = R"({
    "term": 2,
    "index": 1
  })"_vpack;
  auto jsonSlice = velocypack::Slice(jsonBuffer.data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
            << "expected " << jsonSlice.toJson() << " found " << slice.toJson();
}

TEST(LogStatusTest, log_statistics_vpack) {
  LogStatistics statistics;
  statistics.spearHead = TermIndexPair{LogTerm{2}, LogIndex{1}};
  statistics.commitIndex = LogIndex{1};
  statistics.firstIndex = LogIndex{1};
  VPackBuilder builder;
  statistics.toVelocyPack(builder);
  auto slice = builder.slice();
  auto fromVPack = LogStatistics::fromVelocyPack(slice);
  EXPECT_EQ(statistics, fromVPack);

  auto jsonBuffer = R"({
    "commitIndex": 1,
    "firstIndex": 1,
    "spearhead": {
      "term": 2,
      "index": 1
    }
  })"_vpack;
  auto jsonSlice = velocypack::Slice(jsonBuffer.data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
      << "expected " << jsonSlice.toJson() << " found " << slice.toJson();
}