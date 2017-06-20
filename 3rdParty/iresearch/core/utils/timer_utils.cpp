//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include <mutex>
#include <unordered_map>
#include "singleton.hpp"
#include "timer_utils.hpp"

NS_LOCAL

class timer_states: public iresearch::singleton<timer_states> {
 public:
  typedef std::string key_type;
  typedef iresearch::timer_utils::timer_stat_t entry_type;
  typedef std::unordered_map<key_type, entry_type> state_map_t;

  timer_states(): track_all_keys_(false) {}

  entry_type& find(
    const key_type& key
  ) {
    if (track_all_keys_) {
      std::lock_guard<std::mutex> lock(mutex_);
      auto itr = state_map_.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());

      return itr.first->second;
    }

    static entry_type unused;
    auto itr = state_map_.find(key);

    if (itr == state_map_.end()) {
      return unused;
    }

    return itr->second;
  }

  void init(
    bool track_all_keys = false,
    const std::unordered_set<key_type>& tracked_keys = std::unordered_set<key_type>()
  ) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& entry: state_map_) {
      entry.second.count = 0;
      entry.second.time = 0;
    }

    track_all_keys_ = track_all_keys;

    for (auto& key: tracked_keys) {
      state_map_.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
    }
  }

  bool visit(
    const std::function<bool(const key_type& key, size_t count, size_t time)>& visitor
  ) {
    for (auto& entry: state_map_) {
      if (!visitor(entry.first, entry.second.count, entry.second.time)) {
        return false;
      }
    }

    return true;
  }

 private:
  std::mutex mutex_;
  state_map_t state_map_;
  bool track_all_keys_;
};

NS_END // NS_LOCAL

NS_ROOT
NS_BEGIN(timer_utils)

scoped_timer::scoped_timer(timer_stat_t& stat):
  start_(std::chrono::system_clock::now().time_since_epoch().count()), stat_(stat) {
  ++(stat_.count);
}

scoped_timer::scoped_timer(scoped_timer&& other) NOEXCEPT
  : start_(other.start_), stat_(other.stat_) {
}

scoped_timer::~scoped_timer() {
  stat_.time += std::chrono::system_clock::now().time_since_epoch().count() - start_;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                timer registration
// -----------------------------------------------------------------------------

timer_stat_t& get_stat(const std::string& key) {
  return timer_states::instance().find(key);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    stats tracking
// -----------------------------------------------------------------------------

void init_stats(
  bool track_all_keys /*= false*/,
  const std::unordered_set<std::string>& tracked_keys /*= std::unordered_set<iresearch::std::string>()*/
) {
  timer_states::instance().init(track_all_keys, tracked_keys);
}

bool visit(
  const std::function<bool(const std::string& key, size_t count, size_t time)>& visitor
) {
  return timer_states::instance().visit(visitor);
}

NS_END // NS_BEGIN(timer_utils)
NS_END