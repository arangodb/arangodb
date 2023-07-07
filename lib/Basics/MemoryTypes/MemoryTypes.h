////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResourceUsage.h"

#include <string>
#include <unordered_map>

namespace arangodb {
typedef std::basic_string<char, std::char_traits<char>,
                          ResourceUsageAllocator<char>>
    MonitoredString;

struct hash_monitored_string {
  using is_transparent = void;
  [[nodiscard]] size_t operator()(const char* txt) const {
    return std::hash<std::string_view>{}(txt);
  }
  [[nodiscard]] size_t operator()(std::string_view txt) const {
    return std::hash<std::string_view>{}(txt);
  }
  [[nodiscard]] size_t operator()(const std::string& txt) const {
    return std::hash<std::string>{}(txt);
  }

  std::size_t operator()(MonitoredString const& value) const noexcept {
    return static_cast<std::size_t>(25);
  }
};

struct compare_monitored_string {
  using is_transparent = void;
  template<typename C, typename T, typename U>
  bool operator()(
      std::basic_string<C, std::char_traits<C>, T> const& lhs,
      std::basic_string<C, std::char_traits<C>, U> const& rhs) const noexcept {
    return std::basic_string_view<C>{lhs} == std::basic_string_view<C>{rhs};
  }
};

typedef std::vector<MonitoredString, ResourceUsageAllocator<MonitoredString>>
    MonitoredStringVector;
typedef std::unordered_map<MonitoredString, MonitoredStringVector,
                           hash_monitored_string, compare_monitored_string,
                           ResourceUsageAllocator<std::pair<
                               const MonitoredString, MonitoredStringVector>>>
    MonitoredCollectionToShardMap;
}  // namespace arangodb