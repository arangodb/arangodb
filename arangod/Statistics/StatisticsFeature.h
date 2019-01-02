////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_STATISTICS_FEATURE_H
#define APPLICATION_FEATURES_STATISTICS_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Thread.h"
#include "Statistics/figures.h"

namespace arangodb {
namespace basics {
extern StatisticsCounter TRI_AsyncRequestsStatistics;
extern StatisticsCounter TRI_HttpConnectionsStatistics;
extern StatisticsCounter TRI_TotalRequestsStatistics;
extern StatisticsDistribution* TRI_BytesReceivedDistributionStatistics;
extern StatisticsDistribution* TRI_BytesSentDistributionStatistics;
extern StatisticsDistribution* TRI_ConnectionTimeDistributionStatistics;
extern StatisticsDistribution* TRI_IoTimeDistributionStatistics;
extern StatisticsDistribution* TRI_QueueTimeDistributionStatistics;
extern StatisticsDistribution* TRI_RequestTimeDistributionStatistics;
extern StatisticsDistribution* TRI_TotalTimeDistributionStatistics;
extern StatisticsVector TRI_BytesReceivedDistributionVectorStatistics;
extern StatisticsVector TRI_BytesSentDistributionVectorStatistics;
extern StatisticsVector TRI_ConnectionTimeDistributionVectorStatistics;
extern StatisticsVector TRI_RequestTimeDistributionVectorStatistics;
extern std::vector<StatisticsCounter> TRI_MethodRequestsStatistics;
}  // namespace basics

class StatisticsThread;

class StatisticsFeature final : public application_features::ApplicationFeature {
 public:
  static bool enabled() {
    return STATISTICS != nullptr && STATISTICS->_statistics;
  }

  static double time() { return TRI_microtime(); }

 private:
  static StatisticsFeature* STATISTICS;

 public:
  explicit StatisticsFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

 public:
  void disableStatistics() { _statistics = false; }

 private:
  bool _statistics;

  std::unique_ptr<StatisticsThread> _statisticsThread;
};
}  // namespace arangodb

#endif
