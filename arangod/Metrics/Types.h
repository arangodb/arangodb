////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string_view>

namespace arangodb::metrics {

////////////////////////////////////////////////////////////////////////////////
/// These are constants can be used for `type` in /admin/_metrics?type=...
///
/// `type` needed only for internal requests:
/// `db_json` -- for async collect some metrics from DBServer to Coordinator
/// `cd_json` -- for async collect cluster metrics from one Coordinator
///   to another, commonly from follower to leader, needed to update local cache
/// `last` -- We have some user request on Coordinator, then it make this
///   request, and it consider requested Coordinator is a leader,
///   if it's not true, we don't want redirect another time
////////////////////////////////////////////////////////////////////////////////
inline constexpr std::string_view kDBJson = "db_json";
inline constexpr std::string_view kCDJson = "cd_json";
inline constexpr std::string_view kLast = "last";

}  // namespace arangodb::metrics
