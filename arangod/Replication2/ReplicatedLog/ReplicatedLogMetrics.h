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

#include <cstdint>
#include <memory>
#include <chrono>

class Counter;
template <typename T>
class Histogram;
template <typename T>
class Gauge;
template <typename T>
struct log_scale_t;

namespace arangodb {
class MetricsFeature;
}

namespace arangodb::replication2::replicated_log {

struct ReplicatedLogMetrics {
  explicit ReplicatedLogMetrics(arangodb::MetricsFeature& metricsFeature);

 private:
  template <typename Builder, bool mock = false>
  static auto createMetric(MetricsFeature* metricsFeature)
      -> std::shared_ptr<typename Builder::metric_t>;

 protected:
  template <typename MFP, std::enable_if_t<std::is_same_v<arangodb::MetricsFeature*, MFP> || std::is_null_pointer_v<MFP>, int> = 0,
            bool mock = std::is_null_pointer_v<MFP>>
  explicit ReplicatedLogMetrics(MFP metricsFeature);

 public:
  std::shared_ptr<Gauge<uint64_t>> const replicatedLogNumber;
  std::shared_ptr<Histogram<log_scale_t<std::uint64_t>>> const replicatedLogAppendEntriesRttUs;
  std::shared_ptr<Histogram<log_scale_t<std::uint64_t>>> const replicatedLogFollowerAppendEntriesRtUs;
  std::shared_ptr<Counter> const replicatedLogCreationNumber;
  std::shared_ptr<Counter> const replicatedLogDeletionNumber;
  std::shared_ptr<Gauge<std::uint64_t>> const replicatedLogLeaderNumber;
  std::shared_ptr<Gauge<std::uint64_t>> const replicatedLogFollowerNumber;
  std::shared_ptr<Gauge<std::uint64_t>> const replicatedLogInactiveNumber;
  std::shared_ptr<Counter> const replicatedLogLeaderTookOverNumber;
  std::shared_ptr<Counter> const replicatedLogStartedFollowingNumber;
  std::shared_ptr<Histogram<log_scale_t<std::uint64_t>>> const replicatedLogInsertsBytes;
  std::shared_ptr<Histogram<log_scale_t<std::uint64_t>>> const replicatedLogInsertsRtt;
};

struct MeasureTimeGuard {
  explicit MeasureTimeGuard(std::shared_ptr<Histogram<log_scale_t<std::uint64_t>>> histogram) noexcept;
  MeasureTimeGuard(MeasureTimeGuard const&) = delete;
  MeasureTimeGuard(MeasureTimeGuard&&) = default;
  ~MeasureTimeGuard();

  void fire();
 private:
  std::chrono::steady_clock::time_point const _start;
  std::shared_ptr<Histogram<log_scale_t<std::uint64_t>>> _histogram;
};

}  // namespace arangodb::replication2::replicated_log
