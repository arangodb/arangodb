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

#include <array>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/system-functions.h"
#include "Rest/CommonDefines.h"
#include "Statistics/figures.h"

namespace arangodb {
class Thread;
class StatisticsWorker;
namespace stats {
  class Descriptions;
}

namespace statistics {
extern std::initializer_list<double> const BytesReceivedDistributionCuts;
extern std::initializer_list<double> const BytesSentDistributionCuts;
extern std::initializer_list<double> const ConnectionTimeDistributionCuts;
extern std::initializer_list<double> const RequestTimeDistributionCuts;

extern Counter AsyncRequests;
extern Counter HttpConnections;
extern Counter TotalRequests;
extern Counter TotalRequestsSuperuser;
extern Counter TotalRequestsUser;

constexpr size_t MethodRequestsStatisticsSize =
    ((size_t)arangodb::rest::RequestType::ILLEGAL) + 1;
using MethodRequestCounters = std::array<Counter, MethodRequestsStatisticsSize>;
extern MethodRequestCounters MethodRequests;
extern Distribution ConnectionTimeDistribution;

struct RequestFigures {
  RequestFigures();

  RequestFigures(RequestFigures const&) = delete;
  RequestFigures(RequestFigures&&) = delete;
  RequestFigures& operator=(RequestFigures const&) = delete;
  RequestFigures& operator=(RequestFigures&&) = delete;

  Distribution bytesReceivedDistribution;
  Distribution bytesSentDistribution;
  Distribution ioTimeDistribution;
  Distribution queueTimeDistribution;
  Distribution requestTimeDistribution;
  Distribution totalTimeDistribution;
};
extern RequestFigures SuperuserRequestFigures;
extern RequestFigures UserRequestFigures;
}  // namespace statistics

class StatisticsFeature final : public application_features::ApplicationFeature {
 public:
  static bool enabled() {
    return STATISTICS != nullptr && STATISTICS->_statistics;
  }

  static double time() { return TRI_microtime(); }

 private:
  static StatisticsFeature* STATISTICS;

 public:
  explicit StatisticsFeature(application_features::ApplicationServer& server);
  ~StatisticsFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void toPrometheus(std::string& result, double const& now);

  static stats::Descriptions const* descriptions() {
    if (STATISTICS != nullptr) {
      return STATISTICS->_descriptions.get();
    }
    return nullptr;
  }

 private:
  bool _statistics;
  bool _statisticsHistory;
  bool _statisticsHistoryTouched;

  std::unique_ptr<stats::Descriptions> _descriptions;
  std::unique_ptr<Thread> _statisticsThread;
  std::unique_ptr<StatisticsWorker> _statisticsWorker;
};

}  // namespace arangodb

#endif
