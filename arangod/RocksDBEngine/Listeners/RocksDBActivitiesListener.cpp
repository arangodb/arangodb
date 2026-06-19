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

#include "RocksDBActivitiesListener.h"

#include "Activities/RegistryGlobalVariable.h"
#include "Logger/LogMacros.h"

#include <string>

namespace arangodb {

void RocksDBActivitiesListener::OnCompactionBegin(
    rocksdb::DB*, rocksdb::CompactionJobInfo const& info) {
  try {
    auto handle = activities::make<activities::GenericActivity>(
        "RocksDBCompaction",
        activities::GenericActivityData{
            {"job_id", std::to_string(info.job_id)},
            {"column_family", info.cf_name},
            {"base_input_level", std::to_string(info.base_input_level)},
            {"output_level", std::to_string(info.output_level)},
            {"input_files", std::to_string(info.input_files.size())},
            {"reason", std::string(rocksdb::GetCompactionReasonString(
                           info.compaction_reason))}});

    std::lock_guard guard(_mutex);
    _activities.emplace(info.job_id, std::move(handle));
  } catch (std::exception const& e) {
    LOG_TOPIC("5a91c", WARN, Logger::ENGINES)
        << "failed to create RocksDBCompaction activity for job " << info.job_id
        << ": " << e.what();
  } catch (...) {
    LOG_TOPIC("5a91d", WARN, Logger::ENGINES)
        << "failed to create RocksDBCompaction activity for job " << info.job_id
        << ": unknown exception";
  }
}

void RocksDBActivitiesListener::OnCompactionCompleted(
    rocksdb::DB*, rocksdb::CompactionJobInfo const& info) {
  std::lock_guard guard(_mutex);
  _activities.erase(info.job_id);
}

}  // namespace arangodb