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

TEST_F(RocksDBActivitiesListenerTest, activity_is_present_while_compaction_is_running) {
    auto info = makeInfo(/*job_id*/ 3, /*cf*/ "documents", /*baseLevel*/ 2, /*outLevel*/ 3);
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

}  // namespace arangodb