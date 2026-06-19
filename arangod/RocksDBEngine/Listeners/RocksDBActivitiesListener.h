#include "Activities/GenericActivity.h"

#include <rocksdb/listener.h>

#include <mutex>
#include <unordered_map>

#pragma once

namespace rocksdb {
struct CompactionJobInfo;
class DB;
}  // namespace rocksdb

namespace arangodb {

class RocksDBActivitiesListener : public rocksdb::EventListener {
    public:
     RocksDBActivitiesListener() = default;

     void OnCompactionBegin(rocksdb::DB*, rocksdb::CompactionJobInfo const&) override;
     void OnCompactionCompleted(rocksdb::DB*, rocksdb::CompactionJobInfo const&) override;
    
    private: 
     std::mutex _mutex;
     std::unordered_map<int, activities::GenericActivity::HandleType> _activities;
};
}  // namespace arangodb