////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestServer/MetricsFeature.h"

#include <cstdint>

namespace arangodb {

struct AppendEntriesRttScale {
  static log_scale_t<std::uint64_t> scale() { return {2, 1, 120000000, 16}; }
};

struct InsertBytesScale {
  static log_scale_t<std::uint64_t> scale() {
    // Up to 16GiB. 17 buckets (34/2) so they feat neatly into powers of 2.
    return {2, 1, std::uint64_t(1) << 34, 17};
  }
};

DECLARE_GAUGE(arangodb_replication2_replicated_log_number, std::uint64_t,
              "Number of replicated logs on this arangodb instance");

DECLARE_HISTOGRAM(arangodb_replication2_replicated_log_append_entries_rtt_us,
                  AppendEntriesRttScale, "RTT for AppendEntries requests [us]");
DECLARE_HISTOGRAM(arangodb_replication2_replicated_log_follower_append_entries_rt_us,
                  AppendEntriesRttScale, "RT for AppendEntries call [us]");

DECLARE_COUNTER(arangodb_replication2_replicated_log_creation_number,
                "Number of replicated logs created since server start");

DECLARE_COUNTER(arangodb_replication2_replicated_log_deletion_number,
                "Number of replicated logs deleted since server start");

DECLARE_GAUGE(
    arangodb_replication2_replicated_log_leader_number, std::uint64_t,
    "Number of replicated logs this server has, and is currently a leader of");

DECLARE_GAUGE(arangodb_replication2_replicated_log_follower_number, std::uint64_t,
              "Number of replicated logs this server has, and is currently a "
              "follower of");

DECLARE_GAUGE(arangodb_replication2_replicated_log_inactive_number, std::uint64_t,
              "Number of replicated logs this server has, and is currently "
              "neither leader nor follower of");

DECLARE_COUNTER(arangodb_replication2_replicated_log_leader_took_over_number,
                "Number of times a replicated log on this server took over as "
                "leader in a term");

DECLARE_COUNTER(arangodb_replication2_replicated_log_started_following_number,
                "Number of times a replicated log on this server started "
                "following a leader in a term");

DECLARE_HISTOGRAM(arangodb_replication2_replicated_log_inserts_bytes, InsertBytesScale,
                  "Number of bytes per insert in replicated log leader "
                  "instances on this server");

}  // namespace arangodb
