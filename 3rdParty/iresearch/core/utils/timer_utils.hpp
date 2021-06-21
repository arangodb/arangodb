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

#ifndef IRESEARCH_TIMER_UTILS_H
#define IRESEARCH_TIMER_UTILS_H

#include <atomic>
#include <functional>

#include <absl/container/flat_hash_set.h>

#include "utils/noncopyable.hpp"
#include "utils/string.hpp"
#include "shared.hpp"

namespace iresearch {
namespace timer_utils {

struct timer_stat_t {
  std::atomic<size_t> count;
  std::atomic<size_t> time;
};

class IRESEARCH_API scoped_timer : util::noncopyable {
 public:
  scoped_timer(timer_stat_t& stat);
  scoped_timer(scoped_timer&&) = default;
  ~scoped_timer();

 private:
  scoped_timer& operator=(scoped_timer&&) = delete;

  size_t start_;
  timer_stat_t& stat_;
};

static_assert(std::is_nothrow_move_constructible_v<scoped_timer>);

// -----------------------------------------------------------------------------
// --SECTION--                                                timer registration
// -----------------------------------------------------------------------------

IRESEARCH_API timer_stat_t& get_stat(const std::string& key);

// Note: MSVC sometimes initializes the static variable and sometimes leaves it as *(nullptr)
//       therefore for MSVC before use, check if the static variable has been initialized
#define REGISTER_TIMER__(timer_name, line) \
  static auto& timer_state ## _ ## line = ::iresearch::timer_utils::get_stat(timer_name); \
  ::iresearch::timer_utils::scoped_timer timer_stat ## _ ## line( \
    MSVC_ONLY(&timer_state ## _ ## line == nullptr ? ::iresearch::timer_utils::get_stat(timer_name) :) \
    timer_state ## _ ## line \
  );
#define REGISTER_TIMER_EXPANDER__(timer_name, line) REGISTER_TIMER__(timer_name, line)
#define SCOPED_TIMER(timer_name) REGISTER_TIMER_EXPANDER__(timer_name, __LINE__)

#if defined(IRESEARCH_DEBUG) && !defined(IRESEARCH_VALGRIND)
  #define REGISTER_TIMER(timer_name) REGISTER_TIMER_EXPANDER__(timer_name, __LINE__)
  #define REGISTER_TIMER_DETAILED() REGISTER_TIMER(std::string(IRESEARCH_CURRENT_FUNCTION) + ":" + TOSTRING(__LINE__))
  #define REGISTER_TIMER_DETAILED_VERBOSE() REGISTER_TIMER(std::string(__FILE__) + ":" + TOSTRING(__LINE__) + " -> " + std::string(IRESEARCH_CURRENT_FUNCTION))
  #define REGISTER_TIMER_NAMED_DETAILED(timer_name) REGISTER_TIMER(std::string(IRESEARCH_CURRENT_FUNCTION) + " \"" + timer_name + "\"")
  #define REGISTER_TIMER_NAMED_DETAILED_VERBOSE(timer_name) REGISTER_TIMER(std::string(__FILE__) + ":" + TOSTRING(__LINE__) + " -> " + std::string(IRESEARCH_CURRENT_FUNCTION) + " \"" + timer_name + "\"")
#else
  #define REGISTER_TIMER(timer_name)
  #define REGISTER_TIMER_DETAILED()
  #define REGISTER_TIMER_DETAILED_VERBOSE()
  #define REGISTER_TIMER_NAMED_DETAILED(timer_name)
  #define REGISTER_TIMER_NAMED_DETAILED_VERBOSE(timer_name)
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    stats tracking
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize stat tracking of specific keys
///        all previous timer stats are invalid after this call
///        NOTE: this method call must be externally synchronized with
///              get_stat(...) and visit(...), i.e. do not call both concurrently
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API void init_stats(
  bool track_all_keys = false,
  const absl::flat_hash_set<std::string>& tracked_keys = {});

////////////////////////////////////////////////////////////////////////////////
/// @brief visit all tracked keys
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API bool visit(
  const std::function<bool(const std::string& key, size_t count, size_t time_us)>& visitor
);

////////////////////////////////////////////////////////////////////////////////
/// @brief flush formatted timer stats to a specified stream
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API void flush_stats(std::ostream &out);

} // timer_utils
} // namespace iresearch {

#endif
