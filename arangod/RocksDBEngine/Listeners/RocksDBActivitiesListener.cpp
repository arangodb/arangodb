#include "RocksDBActivitiesListener.h"

#include "Activities/RegistryGlobalVariable.h"

#include <rocksdb/listener.h>
#include <string>

namespace arangodb {

void RocksDBActivitiesListener::OnCompactionBegin(rocksdb::DB*, rocksdb::CompactionJobInfo const& info) {
    auto handle = activities::make<activities::GenericActivity>(
        "RocksDBCompaction",
        activities::GenericActivityData{
            {"job_id", std::to_string(info.job_id)},
            {"column_family", info.cf_name},
            {"base_input_level", std::to_string(info.base_input_level)},
            {"output_level", std::to_string(info.output_level)},
            {"input_files", std::to_string(info.input_files.size())},
            {"reason", std::string(
                rocksdb::GetCompactionReasonString(info.compaction_reason)
            )},
            {"phase", "running"}
        }
    );

    std::lock_guard guard(_mutex);
    _activities.emplace(info.job_id, std::move(handle));
}

void RocksDBActivitiesListener::OnCompactionCompleted(rocksdb::DB*, rocksdb::CompactionJobInfo const& info) {
    std::lock_guard guard(_mutex);
    _activities.erase(info.job_id);
}

}  // namespace arangodb