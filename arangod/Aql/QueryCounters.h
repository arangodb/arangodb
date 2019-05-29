////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_QUERY_COUNTERS_H
#define ARANGOD_AQL_QUERY_COUNTERS_H 1

#include "Basics/Common.h"

#include <unordered_map>

namespace arangodb {
namespace aql {

/// @brief non-thread-safe class for maintaining per-query user-defined named
/// counters. counters are accessed by a string name, and are internally stored
/// as int64_t values without any overflow/underflow checks
class QueryCounters {
 public:
  QueryCounters(QueryCounters const&) = delete;
  QueryCounters operator=(QueryCounters const&) = delete;

  QueryCounters() = default;

  /// @brief sets counter to 0
  void clear(std::string const& name);

  /// @brief sets counter to specified new value, returns old value
  int64_t set(std::string const& name, int64_t newValue);
  
  /// @brief relative adjustment of counter by +/- delta, returns old value
  int64_t adjust(std::string const& name, bool increase, int64_t delta);
  
  /// @brief returns value of counter
  int64_t get(std::string const& name) const;

 private:
  /// @brief the actual counters, accessed by name
  /// access to this member must be made thread-safe if this class is to be
  /// used in a multi-threaded context
  std::unordered_map<std::string, int64_t> _counters;
};

}  // namespace aql
}  // namespace arangodb

#endif
