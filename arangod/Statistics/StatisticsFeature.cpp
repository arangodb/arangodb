////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "StatisticsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CpuUsageFeature.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/application-exit.h"
#include "Basics/process-utils.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/Descriptions.h"
#include "Statistics/RequestStatistics.h"
#include "Statistics/ServerStatistics.h"
#include "Statistics/StatisticsWorker.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"

#include <initializer_list>
#include <chrono>
#include <thread>

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::statistics;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

namespace arangodb {
namespace statistics {

// local_name: {"prometheus_name", "type", "help"}
std::map<std::string, std::vector<std::string>> statStrings{
  {"bytesReceived",
   {"arangodb_client_connection_statistics_bytes_received_bucket", "gauge",
    "Bytes received for a request"}},
  {"bytesReceivedCount",
   {"arangodb_client_connection_statistics_bytes_received_count", "gauge",
    "Bytes received for a request"}},
  {"bytesReceivedSum",
   {"arangodb_client_connection_statistics_bytes_received_sum", "gauge",
    "Bytes received for a request"}},
  {"bytesSent",
   {"arangodb_client_connection_statistics_bytes_sent_bucket", "gauge",
    "Bytes sent for a request"}},
  {"bytesSentCount",
   {"arangodb_client_connection_statistics_bytes_sent_count", "gauge",
    "Bytes sent for a request"}},
  {"bytesSentSum",
   {"arangodb_client_connection_statistics_bytes_sent_sum", "gauge",
    "Bytes sent for a request"}},
  {"minorPageFaults",
   {"arangodb_process_statistics_minor_page_faults", "gauge",
    "The number of minor faults the process has made which have not required loading a memory page from disk. This figure is not reported on Windows"}},
  {"majorPageFaults",
   {"arangodb_process_statistics_major_page_faults", "gauge",
    "On Windows, this figure contains the total number of page faults. On other system, this figure contains the number of major faults the process has made which have required loading a memory page from disk"}},
  {"bytesReceived",
   {"arangodb_client_connection_statistics_bytes_received_bucket", "gauge",
    "Bytes received for a request"}},
  {"userTime",
   {"arangodb_process_statistics_user_time", "gauge",
    "Amount of time that this process has been scheduled in user mode, measured in seconds"}},
  {"systemTime",
   {"arangodb_process_statistics_system_time", "gauge",
    "Amount of time that this process has been scheduled in kernel mode, measured in seconds"}},
  {"numberOfThreads",
   {"arangodb_process_statistics_number_of_threads", "gauge",
    "Number of threads in the arangod process"}},
  {"residentSize",
   {"arangodb_process_statistics_resident_set_size", "gauge", "The total size of the number of pages the process has in real memory. This is just the pages which count toward text, data, or stack space. This does not include pages which have not been demand-loaded in, or which are swapped out. The resident set size is reported in bytes"}},
  {"residentSizePercent",
   {"arangodb_process_statistics_resident_set_size_percent", "gauge", "The relative size of the number of pages the process has in real memory compared to system memory. This is just the pages which count toward text, data, or stack space. This does not include pages which have not been demand-loaded in, or which are swapped out. The value is a ratio between 0.00 and 1.00"}},
  {"virtualSize",
   {"arangodb_process_statistics_virtual_memory_size", "gauge", "On Windows, this figure contains the total amount of memory that the memory manager has committed for the arangod process. On other systems, this figure contains The size of the virtual memory the process is using"}},
  {"clientHttpConnections",
   {"arangodb_client_connection_statistics_client_connections", "gauge",
    "The number of client connections that are currently open"}},
  {"connectionTime",
   {"arangodb_client_connection_statistics_connection_time_bucket", "gauge",
    "Total connection time of a client"}},
  {"connectionTimeCount",
   {"arangodb_client_connection_statistics_connection_time_count", "gauge",
    "Total connection time of a client"}},
  {"connectionTimeSum",
   {"arangodb_client_connection_statistics_connection_time_sum", "gauge",
    "Total connection time of a client"}},
  {"totalTime",
   {"arangodb_client_connection_statistics_total_time_bucket", "gauge",
    "Total time needed to answer a request"}},
  {"totalTimeCount",
   {"arangodb_client_connection_statistics_total_time_count", "gauge",
    "Total time needed to answer a request"}},
  {"totalTimeSum",
   {"arangodb_client_connection_statistics_total_time_sum", "gauge",
    "Total time needed to answer a request"}},
  {"requestTime",
   {"arangodb_client_connection_statistics_request_time_bucket", "gauge",
    "Request time needed to answer a request"}},
  {"requestTimeCount",
   {"arangodb_client_connection_statistics_request_time_count", "gauge",
    "Request time needed to answer a request"}},
  {"requestTimeSum",
   {"arangodb_client_connection_statistics_request_time_sum", "gauge",
    "Request time needed to answer a request"}},
  {"queueTime",
   {"arangodb_client_connection_statistics_queue_time_bucket", "gauge",
    "Request time needed to answer a request"}},
  {"queueTimeCount",
   {"arangodb_client_connection_statistics_queue_time_count", "gauge",
    "Request time needed to answer a request"}},
  {"queueTimeSum",
   {"arangodb_client_connection_statistics_queue_time_sum", "gauge",
    "Request time needed to answer a request"}},
  {"ioTime",
   {"arangodb_client_connection_statistics_io_time_bucket", "gauge",
    "Request time needed to answer a request"}},
  {"ioTimeCount",
   {"arangodb_client_connection_statistics_io_time_count", "gauge",
    "Request time needed to answer a request"}},
  {"ioTimeSum",
   {"arangodb_client_connection_statistics_io_time_sum", "gauge",
    "Request time needed to answer a request"}},
  {"httpReqsTotal",
   {"arangodb_http_request_statistics_total_requests", "gauge",
    "Total number of HTTP requests"}},
  {"httpReqsSuperuser",
   {"arangodb_http_request_statistics_superuser_requests", "gauge",
    "Total number of HTTP requests executed by superuser/JWT"}},
  {"httpReqsUser",
   {"arangodb_http_request_statistics_user_requests", "gauge",
    "Total number of HTTP requests executed by clients"}},
  {"httpReqsAsync",
   {"arangodb_http_request_statistics_async_requests", "gauge",
    "Number of asynchronously executed HTTP requests"}},
  {"httpReqsDelete",
   {"arangodb_http_request_statistics_http_delete_requests", "gauge",
    "Number of HTTP DELETE requests"}},
  {"httpReqsGet",
   {"arangodb_http_request_statistics_http_get_requests", "gauge",
    "Number of HTTP GET requests"}},
  {"httpReqsHead",
   {"arangodb_http_request_statistics_http_head_requests", "gauge",
    "Number of HTTP HEAD requests"}},
  {"httpReqsOptions",
   {"arangodb_http_request_statistics_http_options_requests", "gauge",
    "Number of HTTP OPTIONS requests"}},
  {"httpReqsPatch",
   {"arangodb_http_request_statistics_http_patch_requests", "gauge",
    "Number of HTTP PATCH requests"}},
  {"httpReqsPost",
   {"arangodb_http_request_statistics_http_post_requests", "gauge",
    "Number of HTTP POST requests"}},
  {"httpReqsPut",
   {"arangodb_http_request_statistics_http_put_requests", "gauge",
    "Number of HTTP PUT requests"}},
  {"httpReqsOther",
   {"arangodb_http_request_statistics_other_http_requests", "gauge",
    "Number of other HTTP requests"}},
  {"uptime",
   {"arangodb_server_statistics_server_uptime", "gauge",
    "Number of seconds elapsed since server start"}},
  {"physicalSize",
   {"arangodb_server_statistics_physical_memory", "gauge",
    "Physical memory in bytes"}},
  {"userPercent",
   {"arangodb_server_statistics_user_percent", "gauge",
    "Percentage of time that the system CPUs have spent in user mode"}},
  {"systemPercent",
   {"arangodb_server_statistics_system_percent", "gauge",
    "Percentage of time that the system CPUs have spent in kernel mode"}},
  {"idlePercent",
   {"arangodb_server_statistics_idle_percent", "gauge",
    "Percentage of time that the system CPUs have been idle"}},
  {"iowaitPercent",
   {"arangodb_server_statistics_iowait_percent", "gauge",
    "Percentage of time that the system CPUs have been waiting for I/O"}},
  {"v8ContextAvailable",
   {"arangodb_v8_context_alive", "gauge",
    "Number of V8 contexts currently alive"}},
  {"v8ContextBusy",
   {"arangodb_v8_context_busy", "gauge",
    "Number of V8 contexts currently busy"}},
  {"v8ContextDirty",
   {"arangodb_v8_context_dirty", "gauge",
    "Number of V8 contexts currently dirty"}},
  {"v8ContextFree",
   {"arangodb_v8_context_free", "gauge",
    "Number of V8 contexts currently free"}},
  {"v8ContextMax",
   {"arangodb_v8_context_max", "gauge",
    "Maximum number of concurrent V8 contexts"}},
  {"v8ContextMin",
   {"arangodb_v8_context_min", "gauge",
    "Minimum number of concurrent V8 contexts"}},
};

std::initializer_list<double> const BytesReceivedDistributionCuts{250, 1000, 2000, 5000, 10000};
std::initializer_list<double> const BytesSentDistributionCuts{250, 1000, 2000, 5000, 10000};
std::initializer_list<double> const ConnectionTimeDistributionCuts{0.1, 1.0, 60.0};
std::initializer_list<double> const RequestTimeDistributionCuts{
    0.01, 0.05, 0.1, 0.2, 0.5, 1.0, 5.0, 15.0, 30.0};

Counter AsyncRequests;
Counter HttpConnections;
Counter TotalRequests;
Counter TotalRequestsSuperuser;
Counter TotalRequestsUser;
MethodRequestCounters MethodRequests;
Distribution ConnectionTimeDistribution(ConnectionTimeDistributionCuts);

RequestFigures::RequestFigures() :
  bytesReceivedDistribution(BytesReceivedDistributionCuts),
  bytesSentDistribution(BytesSentDistributionCuts),
  ioTimeDistribution(RequestTimeDistributionCuts),
  queueTimeDistribution(RequestTimeDistributionCuts),
  requestTimeDistribution(RequestTimeDistributionCuts),
  totalTimeDistribution(RequestTimeDistributionCuts) {}

RequestFigures SuperuserRequestFigures;
RequestFigures UserRequestFigures;
}  // namespace statistics
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                  StatisticsThread
// -----------------------------------------------------------------------------

class StatisticsThread final : public Thread {
 public:
  explicit StatisticsThread(ApplicationServer& server) 
    : Thread(server, "Statistics") {}
  ~StatisticsThread() { shutdown(); }

 public:
  void run() override {
    auto& databaseFeature = server().getFeature<arangodb::DatabaseFeature>();
    if (databaseFeature.upgrade()) {
      // don't start the thread when we are running an upgrade
      return;
    }

    uint64_t const MAX_SLEEP_TIME = 250;

    uint64_t sleepTime = 100;
    int nothingHappened = 0;

    while (!isStopping()) {
      size_t count = RequestStatistics::processAll();

      if (count == 0) {
        if (++nothingHappened == 10 * 30) {
          // increase sleep time every 30 seconds
          nothingHappened = 0;
          sleepTime += 50;

          if (sleepTime > MAX_SLEEP_TIME) {
            sleepTime = MAX_SLEEP_TIME;
          }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));

      } else {
        nothingHappened = 0;

        if (count < 10) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else if (count < 100) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 StatisticsFeature
// -----------------------------------------------------------------------------

StatisticsFeature::StatisticsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Statistics"),
      _statistics(true),
      _statisticsHistory(true),
      _statisticsHistoryTouched(false),
      _descriptions(server) {
  setOptional(true);
  startsAfter<AqlFeaturePhase>();
}

StatisticsFeature::~StatisticsFeature() = default;

void StatisticsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.disable-statistics", "server.statistics");

  options->addSection("server", "Server features");

  options->addOption("--server.statistics",
                     "turn statistics gathering on or off",
                     new BooleanParameter(&_statistics),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
  options->addOption("--server.statistics-history",
                     "turn storing statistics in database on or off",
                     new BooleanParameter(&_statisticsHistory),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
    .setIntroducedIn(30409)
    .setIntroducedIn(30501);
}

void StatisticsFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_statistics) {
    // turn ourselves off
    disable();
  }

  _statisticsHistoryTouched = options->processingResult().touched("--server.statistics-history");
}

void StatisticsFeature::prepare() {
  // initialize counters for all HTTP request types
  ConnectionStatistics::initialize();
  RequestStatistics::initialize();
}

void StatisticsFeature::start() {
  TRI_ASSERT(isEnabled());

  if (!server().hasFeature<arangodb::SystemDatabaseFeature>()) {
    LOG_TOPIC("9b551", FATAL, arangodb::Logger::STATISTICS)
        << "could not find feature 'SystemDatabase'";
    FATAL_ERROR_EXIT();
  }
  auto& sysDbFeature = server().getFeature<arangodb::SystemDatabaseFeature>();

  auto vocbase = sysDbFeature.use();

  if (!vocbase) {
    LOG_TOPIC("cff56", FATAL, arangodb::Logger::STATISTICS)
        << "could not find system database";
    FATAL_ERROR_EXIT();
  }

  _statisticsThread.reset(new StatisticsThread(server()));

  if (!_statisticsThread->start()) {
    LOG_TOPIC("46b0c", FATAL, arangodb::Logger::STATISTICS)
        << "could not start statistics thread";
    FATAL_ERROR_EXIT();
  }

  // force history disable on Agents
  if (arangodb::ServerState::instance()->isAgent() && !_statisticsHistoryTouched) {
    _statisticsHistory = false;
  } 

  if (ServerState::instance()->isDBServer()) {
    // the StatisticsWorker runs queries against the _statistics
    // collections, so it does not work on DB servers
    _statisticsHistory = false;
  }

  if (_statisticsHistory) {
    TRI_ASSERT(!ServerState::instance()->isDBServer());

    _statisticsWorker.reset(new StatisticsWorker(*vocbase));

    if (!_statisticsWorker->start()) {
      LOG_TOPIC("6ecdc", FATAL, arangodb::Logger::STATISTICS)
        << "could not start statistics worker";
      FATAL_ERROR_EXIT();
    }
  } // if
}

void StatisticsFeature::stop() {
  if (_statisticsThread != nullptr) {
    _statisticsThread->beginShutdown();

    while (_statisticsThread->isRunning()) {
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }
  }

  if (_statisticsWorker != nullptr) {
    _statisticsWorker->beginShutdown();

    while (_statisticsWorker->isRunning()) {
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }
  }

  _statisticsThread.reset();
  _statisticsWorker.reset();
}

VPackBuilder StatisticsFeature::fillDistribution(statistics::Distribution const& dist) {
  VPackBuilder builder;
  builder.openObject();

  builder.add("sum", VPackValue(dist._total));
  builder.add("count", VPackValue(dist._count));

  builder.add("counts", VPackValue(VPackValueType::Array));
  for (auto const& val : dist._counts) {
    builder.add(VPackValue(val));
  }
  builder.close();

  builder.close();

  return builder;
}

void StatisticsFeature::appendHistogram(
  std::string& result, statistics::Distribution const& dist,
  std::string const& label, std::initializer_list<std::string> const& les) {

  auto const countLabel = label + "Count";
  auto const countSum = label + "Sum";
  VPackBuilder tmp = fillDistribution(dist);
  VPackSlice slc = tmp.slice();
  VPackSlice counts = slc.get("counts");
  
  auto const& stat = statStrings.at(label); 
  std::string const& name = stat.at(0);

  result +=
    "\n#TYPE " + name + " " + stat[1] + 
    "\n#HELP " + name + " " + stat[2] + '\n';

  TRI_ASSERT(les.size() == counts.length());
  size_t i = 0;
  for (auto const& le : les) {
    result +=
      name + "{le=\"" + le + "\"}"  + " " +
      std::to_string(counts.at(i++).getNumber<uint64_t>()) + '\n';
  }
}

void StatisticsFeature::appendMetric(std::string& result, std::string const& val, std::string const& label) {
  auto const& stat = statStrings.at(label); 
  std::string const& name = stat.at(0);

  result +=
    "\n#TYPE " + name + " " + stat[1] +
    "\n#HELP " + name + " " + stat[2] + 
    '\n' + name + " " + val + '\n';
}

void StatisticsFeature::toPrometheus(std::string& result, double const& now) {
  ProcessInfo info = TRI_ProcessInfoSelf();
  uint64_t rss = static_cast<uint64_t>(info._residentSize);
  double rssp = 0;

  if (PhysicalMemory::getValue() != 0) {
    rssp = static_cast<double>(rss) / static_cast<double>(PhysicalMemory::getValue());
  }

  ServerStatistics const& serverInfo =
      server().getFeature<MetricsFeature>().serverStatistics();

  // processStatistics()
  appendMetric(result, std::to_string(info._minorPageFaults), "minorPageFaults");
  appendMetric(result, std::to_string(info._majorPageFaults), "majorPageFaults");
  if (info._scClkTck != 0) {  // prevent division by zero
    appendMetric(
      result, std::to_string(
        static_cast<double>(info._userTime) / static_cast<double>(info._scClkTck)), "userTime");
    appendMetric(
      result, std::to_string(
        static_cast<double>(info._systemTime) / static_cast<double>(info._scClkTck)), "systemTime");
  }
  appendMetric(result, std::to_string(info._numberThreads), "numberOfThreads");
  appendMetric(result, std::to_string(rss), "residentSize");
  appendMetric(result, std::to_string(rssp), "residentSizePercent");
  appendMetric(result, std::to_string(info._virtualSize), "virtualSize");
  appendMetric(result, std::to_string(PhysicalMemory::getValue()), "physicalSize");
  appendMetric(result, std::to_string(serverInfo.uptime()), "uptime");

  CpuUsageFeature& cpuUsage = server().getFeature<CpuUsageFeature>();
  if (cpuUsage.isEnabled()) {
    auto snapshot = cpuUsage.snapshot();
    appendMetric(result, std::to_string(snapshot.userPercent()), "userPercent");
    appendMetric(result, std::to_string(snapshot.systemPercent()), "systemPercent");
    appendMetric(result, std::to_string(snapshot.idlePercent()), "idlePercent");
    appendMetric(result, std::to_string(snapshot.iowaitPercent()), "iowaitPercent");
  }

  if (isEnabled()) {
    ConnectionStatistics::Snapshot connectionStats;
    ConnectionStatistics::getSnapshot(connectionStats);

    RequestStatistics::Snapshot requestStats;
    RequestStatistics::getSnapshot(requestStats, stats::RequestStatisticsSource::ALL);
    
    // _clientStatistics()
    appendMetric(result, std::to_string(connectionStats.httpConnections.get()), "clientHttpConnections");
    appendHistogram(result, connectionStats.connectionTime, "connectionTime", {"0.01", "1.0", "60.0", "+Inf"});
  appendHistogram(result, requestStats.totalTime, "totalTime",
                  {"0.01", "0.05", "0.1", "0.2", "0.5", "1.0", "5.0", "15.0",
                   "30.0", "+Inf"});
  appendHistogram(result, requestStats.requestTime, "requestTime",
                  {"0.01", "0.05", "0.1", "0.2", "0.5", "1.0", "5.0", "15.0",
                   "30.0", "+Inf"});
  appendHistogram(result, requestStats.queueTime, "queueTime",
                  {"0.01", "0.05", "0.1", "0.2", "0.5", "1.0", "5.0", "15.0",
                   "30.0", "+Inf"});
  appendHistogram(result, requestStats.ioTime, "ioTime",
                  {"0.01", "0.05", "0.1", "0.2", "0.5", "1.0", "5.0", "15.0",
                   "30.0", "+Inf"});
    appendHistogram(result, requestStats.bytesSent, "bytesSent", {"250", "1000", "2000", "5000", "10000", "+Inf"});
    appendHistogram(result, requestStats.bytesReceived, "bytesReceived", {"250", "1000", "2000", "5000", "10000", "+Inf"});

    // _httpStatistics()
    using rest::RequestType;
    appendMetric(result, std::to_string(connectionStats.asyncRequests.get()), "httpReqsAsync");
    appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::DELETE_REQ].get()), "httpReqsDelete");
    appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::GET].get()), "httpReqsGet");
    appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::HEAD].get()), "httpReqsHead");
    appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::OPTIONS].get()), "httpReqsOptions");
    appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::PATCH].get()), "httpReqsPatch");
    appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::POST].get()), "httpReqsPost");
    appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::PUT].get()), "httpReqsPut");
    appendMetric(result, std::to_string(connectionStats.methodRequests[(int)RequestType::ILLEGAL].get()), "httpReqsOther");
    appendMetric(result, std::to_string(connectionStats.totalRequests.get()), "httpReqsTotal");
    appendMetric(result, std::to_string(connectionStats.totalRequestsSuperuser.get()), "httpReqsSuperuser");
    appendMetric(result, std::to_string(connectionStats.totalRequestsUser.get()), "httpReqsUser");
  }

  V8DealerFeature::Statistics v8Counters{};
  if (server().hasFeature<V8DealerFeature>()) {
    V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
    if (dealer.isEnabled()) {
      v8Counters = dealer.getCurrentContextNumbers();
    }
  }
  appendMetric(result, std::to_string(v8Counters.available), "v8ContextAvailable");
  appendMetric(result, std::to_string(v8Counters.busy), "v8ContextBusy");
  appendMetric(result, std::to_string(v8Counters.dirty), "v8ContextDirty");
  appendMetric(result, std::to_string(v8Counters.free), "v8ContextFree");
  appendMetric(result, std::to_string(v8Counters.min), "v8ContextMin");
  appendMetric(result, std::to_string(v8Counters.max), "v8ContextMax");
  result += "\n";
}
