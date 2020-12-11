////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "singleton.hpp"
#include "timer_utils.hpp"

#include <mutex>
#include <unordered_map>
#include <map>

namespace {

class timer_states: public irs::singleton<timer_states> {
 public:
  typedef std::string key_type;
  typedef irs::timer_utils::timer_stat_t entry_type;
  typedef std::unordered_map<key_type, entry_type> state_map_t;

  timer_states(): track_all_keys_(false) {}

  entry_type& find(const key_type& key) {
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
      const std::unordered_set<key_type>& tracked_keys = std::unordered_set<key_type>()) {
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
      const std::function<bool(const key_type& key, size_t count, size_t time_us)>& visitor
  ) {
    static const auto usec =
      (1000000. * std::chrono::system_clock::period::num) / std::chrono::system_clock::period::den;

    for (auto& entry: state_map_) {
      if (!visitor(entry.first, entry.second.count, size_t(entry.second.time * usec))) { // truncate 'time_us'
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

} // namespace {

namespace iresearch {
namespace timer_utils {

scoped_timer::scoped_timer(timer_stat_t& stat):
  start_(std::chrono::system_clock::now().time_since_epoch().count()), stat_(stat) {
  ++(stat_.count);
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
    const std::function<bool(const std::string& key, size_t count, size_t time_us)>& visitor
) {
  return timer_states::instance().visit(visitor);
}

void flush_stats(std::ostream &out) {
  std::map<std::string, std::pair<size_t, size_t>> ordered_stats;

  iresearch::timer_utils::visit([&ordered_stats](const std::string& key, size_t count, size_t time)->bool {
    std::string key_str = key;

#if defined(__GNUC__)
    if (key_str.compare(0, strlen("virtual "), "virtual ") == 0) {
      key_str = key_str.substr(strlen("virtual "));
    }

    size_t i;

    if (std::string::npos != (i = key_str.find(' ')) && key_str.find('(') > i) {
      key_str = key_str.substr(i + 1);
    }
#elif defined(_MSC_VER)
    size_t i;

    if (std::string::npos != (i = key_str.find("__cdecl "))) {
      key_str = key_str.substr(i + strlen("__cdecl "));
    }
#endif

    ordered_stats.emplace(key_str, std::make_pair(count, time));
    return true;
  });

  for (auto& entry: ordered_stats) {
    auto& key = entry.first;
    auto& count = entry.second.first;
    auto& time = entry.second.second;
    out << key << "\tcalls:" << count << ",\ttime: " << time/1000 << " us,\tavg call: " << time/1000/(double)count << " us"<< std::endl;
  }
}

} // timer_utils
} // namespace iresearch {
