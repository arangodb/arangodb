////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Basics/Result.h"
#include "Basics/system-functions.h"
#include "Metrics/Fwd.h"
#include "Rest/CommonDefines.h"
#include "RestServer/arangod.h"
#include "Statistics/Descriptions.h"
#include "Statistics/figures.h"

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>

struct TRI_vocbase_t;

namespace arangodb {
class Thread;
class StatisticsWorker;
namespace stats {
class Descriptions;
}
namespace velocypack {
class Builder;
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

class StatisticsFeature final : public ArangodFeature {
 public:
  static double time() { return TRI_microtime(); }

 public:
  static constexpr std::string_view name() noexcept { return "Statistics"; }

  explicit StatisticsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void stop() override final;
  void toPrometheus(std::string& result, double now, std::string_view globals,
                    bool ensureWhitespace);

  stats::Descriptions const& descriptions() const { return _descriptions; }

  static arangodb::velocypack::Builder fillDistribution(
      statistics::Distribution const& dist);

  Result getClusterSystemStatistics(
      TRI_vocbase_t& vocbase, double start,
      arangodb::velocypack::Builder& result) const;

  bool allDatabases() const noexcept { return _statisticsAllDatabases; }

 private:
  static void appendMetric(std::string& result, std::string const& val,
                           std::string const& label, std::string_view globals,
                           bool ensureWhitespace);

  static void appendHistogram(std::string& result,
                              statistics::Distribution const& dist,
                              std::string const& label,
                              std::initializer_list<std::string> const& les,
                              bool isInteger, std::string_view globals,
                              bool ensureWhitespace);
  bool _statistics;
  bool _statisticsHistory;
  bool _statisticsHistoryTouched;
  bool _statisticsAllDatabases;

  stats::Descriptions _descriptions;
  std::unique_ptr<Thread> _statisticsThread;
  std::unique_ptr<StatisticsWorker> _statisticsWorker;

  metrics::Gauge<uint64_t>& _requestStatisticsMemoryUsage;
  metrics::Gauge<uint64_t>& _connectionStatisticsMemoryUsage;
};

}  // namespace arangodb
