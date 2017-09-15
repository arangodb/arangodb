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

#ifndef IRESEARCH_TIMER_UTILS_H
#define IRESEARCH_TIMER_UTILS_H

#include <atomic>
#include <functional>
#include <unordered_set>
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"
#include "shared.hpp"

NS_ROOT
NS_BEGIN(timer_utils)

struct timer_stat_t {
  std::atomic<size_t> count;
  std::atomic<size_t> time;
};

class IRESEARCH_API scoped_timer: private iresearch::util::noncopyable {
 public:
  scoped_timer(timer_stat_t& stat);
  scoped_timer(scoped_timer&& other) NOEXCEPT;
  ~scoped_timer();

 private:
  size_t start_;
  timer_stat_t& stat_;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                timer registration
// -----------------------------------------------------------------------------

IRESEARCH_API timer_stat_t& get_stat(const std::string& key);

// Note: MSVC sometimes initializes the static variable and sometimes leaves it as *(nullptr)
//       therefore for MSVC before use, check if the static variable has been initialized
#define REGISTER_TIMER__(timer_name, line) \
  static auto& timer_state ## _ ## line = iresearch::timer_utils::get_stat(timer_name); \
  iresearch::timer_utils::scoped_timer timer_stat ## _ ## line( \
    MSVC_ONLY(&timer_state ## _ ## line == nullptr ? iresearch::timer_utils::get_stat(timer_name) :) \
    timer_state ## _ ## line \
  );
#define REGISTER_TIMER_EXPANDER__(timer_name, line) REGISTER_TIMER__(timer_name, line)
#define SCOPED_TIMER(timer_name) REGISTER_TIMER_EXPANDER__(timer_name, __LINE__)

#ifdef IRESEARCH_DEBUG
  #define REGISTER_TIMER(timer_name) REGISTER_TIMER_EXPANDER__(timer_name, __LINE__)
  #define REGISTER_TIMER_DETAILED() REGISTER_TIMER(std::string(CURRENT_FUNCTION) + ":" + TOSTRING(__LINE__))
  #define REGISTER_TIMER_DETAILED_VERBOSE() REGISTER_TIMER(std::string(__FILE__) + ":" + TOSTRING(__LINE__) + " -> " + std::string(CURRENT_FUNCTION))
  #define REGISTER_TIMER_NAMED_DETAILED(timer_name) REGISTER_TIMER(std::string(CURRENT_FUNCTION) + " \"" + timer_name + "\"")
  #define REGISTER_TIMER_NAMED_DETAILED_VERBOSE(timer_name) REGISTER_TIMER(std::string(__FILE__) + ":" + TOSTRING(__LINE__) + " -> " + std::string(CURRENT_FUNCTION) + " \"" + timer_name + "\"")
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
  const std::unordered_set<std::string>& tracked_keys = std::unordered_set<std::string>()
);

////////////////////////////////////////////////////////////////////////////////
/// @brief visit all tracked keys
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API bool visit(
  const std::function<bool(const std::string& key, size_t count, size_t time)>& visitor
);

NS_END // NS_BEGIN(timer_utils)
NS_END

#endif