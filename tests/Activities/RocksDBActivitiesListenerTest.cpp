////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <rocksdb/listener.h>
#include <velocypack/Iterator.h>

#include "Activities/Registry.h"
#include "Activities/RegistryGlobalVariable.h"
#include "RocksDBEngine/Listeners/RocksDBActivitiesListener.h"

namespace arangodb {

namespace {
rocksdb::CompactionJobInfo makeInfo(int job_id, std::string cf = "default",
                                    int baseLevel = 0, int outLevel = 1) {
  rocksdb::CompactionJobInfo info{};
  info.cf_name = std::move(cf);
  info.job_id = job_id;
  info.base_input_level = baseLevel;
  info.output_level = outLevel;
  info.compaction_reason = rocksdb::CompactionReason::kLevelL0FilesNum;
  return info;
}

std::vector<velocypack::Slice> compactionActivities() {
  auto snap = activities::registry.snapshot();
  EXPECT_TRUE(snap.ok());
  std::vector<velocypack::Slice> out;
  for (auto entry : velocypack::ArrayIterator(snap.get().slice())) {
    auto type = entry.get("type");
    if (type.isString() && type.stringView() == "RocksDBCompaction") {
      out.push_back(entry);
    }
  }
  return out;
}
}  // namespace

struct RocksDBActivitiesListenerTest : public ::testing::Test {
  static void SetUpTestSuite() { activities::registry.garbageCollectAll(); }
  void TearDown() override {
    activities::registry.garbageCollectAll();
    EXPECT_EQ(activities::registry.size(), 0);
  }
  RocksDBActivitiesListener listener;
};

TEST_F(RocksDBActivitiesListenerTest,
       activity_is_present_while_compaction_is_running) {
  auto info = makeInfo(/*job_id*/ 3, /*cf*/ "documents", /*baseLevel*/ 2,
                       /*outLevel*/ 3);
  listener.OnCompactionBegin(nullptr, info);

  auto acts = compactionActivities();
  ASSERT_EQ(acts.size(), 1u);
  auto data = acts[0].get("data");
  EXPECT_EQ(data.get("job_id").copyString(), "3");
  EXPECT_EQ(data.get("column_family").copyString(), "documents");
  EXPECT_EQ(data.get("base_input_level").copyString(), "2");
  EXPECT_EQ(data.get("output_level").copyString(), "3");
  listener.OnCompactionCompleted(nullptr, info);
}

TEST_F(RocksDBActivitiesListenerTest,
       activity_data_contains_all_expected_fields) {
  auto info = makeInfo(7, "documents", 2, 3);
  info.input_files = {"sst1.sst", "sst2.sst", "sst3.sst"};
  info.compaction_reason = rocksdb::CompactionReason::kManualCompaction;
  listener.OnCompactionBegin(nullptr, info);

  auto acts = compactionActivities();
  ASSERT_EQ(acts.size(), 1u);
  EXPECT_EQ(acts[0].get("type").copyString(), "RocksDBCompaction");
  EXPECT_TRUE(acts[0].get("parent").isNone());

  auto data = acts[0].get("data");
  EXPECT_EQ(data.get("job_id").copyString(), "7");
  EXPECT_EQ(data.get("column_family").copyString(), "documents");
  EXPECT_EQ(data.get("base_input_level").copyString(), "2");
  EXPECT_EQ(data.get("output_level").copyString(), "3");
  EXPECT_EQ(data.get("input_files").copyString(), "3");
  EXPECT_EQ(data.get("reason").copyString(), "ManualCompaction");

  listener.OnCompactionCompleted(nullptr, info);
}

TEST_F(RocksDBActivitiesListenerTest,
       multiple_compactions_complete_out_of_order) {
  listener.OnCompactionBegin(nullptr, makeInfo(1, "A"));
  listener.OnCompactionBegin(nullptr, makeInfo(2, "B"));
  EXPECT_EQ(compactionActivities().size(), 2u);

  listener.OnCompactionCompleted(nullptr, makeInfo(2, "B"));
  auto acts = compactionActivities();
  ASSERT_EQ(acts.size(), 1u);
  EXPECT_EQ(acts[0].get("data").get("job_id").copyString(), "1");

  listener.OnCompactionCompleted(nullptr, makeInfo(1, "A"));
  EXPECT_TRUE(compactionActivities().empty());
}

TEST_F(RocksDBActivitiesListenerTest, completion_without_begin_is_noop) {
  listener.OnCompactionCompleted(nullptr, makeInfo(0));
  EXPECT_TRUE(compactionActivities().empty());
}

TEST_F(RocksDBActivitiesListenerTest,
       duplecate_begin_for_same_job_id_keeps_first_activity) {
  auto first = makeInfo(5, "first");
  auto second = makeInfo(5, "second");
  listener.OnCompactionBegin(nullptr, first);
  listener.OnCompactionBegin(nullptr, second);

  auto acts = compactionActivities();
  ASSERT_EQ(acts.size(), 1u);
  EXPECT_EQ(acts[0].get("data").get("column_family").copyString(), "first");

  listener.OnCompactionCompleted(nullptr, first);
}

}  // namespace arangodb