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
  using scale_t = log_scale_t<std::uint64_t>;
  static scale_t scale() {
    // values in us, smallest bucket is up to 1ms, scales up to 2^16ms =~ 65s.
    return {scale_t::supply_smallest_bucket, 2, 0, 1'000, 16};
  }
};

struct InsertBytesScale {
  using scale_t = log_scale_t<std::uint64_t>;
  static scale_t scale() {
    // 1 byte up to 16GiB (1 * 4^17 = 16 * 2^30).
    return {scale_t::supply_smallest_bucket, 4, 0, 1, 17};
  }
};

DECLARE_GAUGE(arangodb_replication2_replicated_log_number, std::uint64_t,
              "Number of replicated logs on this arangodb instance");

DECLARE_HISTOGRAM(arangodb_replication2_replicated_log_append_entries_rtt,
                  AppendEntriesRttScale, "RTT for AppendEntries requests [us]");
DECLARE_HISTOGRAM(
    arangodb_replication2_replicated_log_follower_append_entries_rt,
    AppendEntriesRttScale, "RT for AppendEntries call [us]");

DECLARE_COUNTER(arangodb_replication2_replicated_log_creation_total,
                "Number of replicated logs created since server start");

DECLARE_COUNTER(arangodb_replication2_replicated_log_deletion_total,
                "Number of replicated logs deleted since server start");

DECLARE_GAUGE(
    arangodb_replication2_replicated_log_leader_number, std::uint64_t,
    "Number of replicated logs this server has, and is currently a leader of");

DECLARE_GAUGE(arangodb_replication2_replicated_log_follower_number,
              std::uint64_t,
              "Number of replicated logs this server has, and is currently a "
              "follower of");

DECLARE_GAUGE(arangodb_replication2_replicated_log_inactive_number,
              std::uint64_t,
              "Number of replicated logs this server has, and is currently "
              "neither leader nor follower of");

DECLARE_COUNTER(arangodb_replication2_replicated_log_leader_took_over_total,
                "Number of times a replicated log on this server took over as "
                "leader in a term");

DECLARE_COUNTER(arangodb_replication2_replicated_log_started_following_total,
                "Number of times a replicated log on this server started "
                "following a leader in a term");

DECLARE_HISTOGRAM(arangodb_replication2_replicated_log_inserts_bytes,
                  InsertBytesScale,
                  "Number of bytes per insert in replicated log leader "
                  "instances on this server [bytes]");

DECLARE_HISTOGRAM(
    arangodb_replication2_replicated_log_inserts_rtt, AppendEntriesRttScale,
    "Histogram of round-trip times of replicated log inserts [us]");

}  // namespace arangodb
