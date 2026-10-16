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

#pragma once

#include "Activities/GenericActivity.h"

#include <rocksdb/listener.h>

#include <mutex>
#include <unordered_map>

namespace arangodb {

class RocksDBActivitiesListener final : public rocksdb::EventListener {
 public:
  ~RocksDBActivitiesListener() override = default;

  void OnCompactionBegin(rocksdb::DB*,
                         rocksdb::CompactionJobInfo const&) override;
  void OnCompactionCompleted(rocksdb::DB*,
                             rocksdb::CompactionJobInfo const&) override;

 private:
  std::mutex _mutex;
  std::unordered_map<int, activities::GenericActivity::HandleType> _activities;
};

}  // namespace arangodb