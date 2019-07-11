////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Descriptions.h"
#include "Basics/process-utils.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"
#include "Statistics/ServerStatistics.h"
#include "Statistics/StatisticsFeature.h"
#include "V8Server/V8DealerFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

std::string stats::fromGroupType(stats::GroupType gt) {
  switch (gt) {
    case stats::GroupType::System:
      return "system";
    case stats::GroupType::Client:
      return "client";
    case stats::GroupType::Http:
      return "http";
    case stats::GroupType::Vst:
      return "vst";
    case stats::GroupType::Server:
      return "server";
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
}

void stats::Group::toVPack(velocypack::Builder& b) const {
  b.add("group", VPackValue(stats::fromGroupType(type)));
  b.add("name", VPackValue(name));
  b.add("description", VPackValue(description));
}

std::string stats::fromFigureType(stats::FigureType t) {
  switch (t) {
    case stats::FigureType::Current:
      return "current";
    case stats::FigureType::Accumulated:
      return "accumulated";
    case stats::FigureType::Distribution:
      return "distribution";
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
}

std::string stats::fromUnit(stats::Unit u) {
  switch (u) {
    case stats::Unit::Seconds:
      return "seconds";
    case stats::Unit::Bytes:
      return "bytes";
    case stats::Unit::Percent:
      return "percent";
    case stats::Unit::Number:
      return "number";
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
}

void stats::Figure::toVPack(velocypack::Builder& b) const {
  b.add("group", VPackValue(stats::fromGroupType(groupType)));
  b.add("identifier", VPackValue(identifier));
  b.add("name", VPackValue(name));
  b.add("description", VPackValue(description));
  b.add("type", VPackValue(stats::fromFigureType(type)));
  if (type == stats::FigureType::Distribution) {
    TRI_ASSERT(!cuts.empty());
    b.add("cuts", VPackValue(VPackValueType::Array, true));
    for (double cut : cuts) {
      b.add(VPackValue(cut));
    }
    b.close();
  }
  b.add("units", VPackValue(stats::fromUnit(units)));
}

stats::Descriptions::Descriptions()
    : _requestTimeCuts(arangodb::basics::TRI_RequestTimeDistributionVectorStatistics),
      _connectionTimeCuts(arangodb::basics::TRI_ConnectionTimeDistributionVectorStatistics),
      _bytesSendCuts(arangodb::basics::TRI_BytesSentDistributionVectorStatistics),
      _bytesReceivedCuts(arangodb::basics::TRI_BytesReceivedDistributionVectorStatistics) {
  _groups.emplace_back(Group{stats::GroupType::System, "Process Statistics",
                             "Statistics about the ArangoDB process"});
  _groups.emplace_back(Group{stats::GroupType::Client,
                             "Client Connection Statistics",
                             "Statistics about the connections."});
  _groups.emplace_back(Group{stats::GroupType::Http, "HTTP Request Statistics",
                             "Statistics about the HTTP requests."});
  _groups.emplace_back(Group{stats::GroupType::Server, "Server Statistics",
                             "Statistics about the ArangoDB server"});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "userTime",
                               "User Time",
                               "Amount of time that this process has been "
                               "scheduled in user mode, measured in seconds.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Seconds,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "systemTime",
                               "System Time",
                               "Amount of time that this process has been "
                               "scheduled in kernel mode, measured in seconds.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Seconds,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "numberOfThreads",
                               "Number of Threads",
                               "Number of threads in the arangod process.",
                               stats::FigureType::Current,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{
      stats::GroupType::System,
      "residentSize",
      "Resident Set Size",
      "The total size of the number of pages the process has in real memory. "
      "This is just the pages which count toward text, data, or stack space. "
      "This does not include pages which have not been demand-loaded in, or "
      "which are swapped out. The resident set size is reported in bytes.",
      stats::FigureType::Current,
      stats::Unit::Bytes,
      {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "residentSizePercent",
                               "Resident Set Size",
                               "The percentage of physical memory used by the "
                               "process as resident set size.",
                               stats::FigureType::Current,
                               stats::Unit::Percent,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "virtualSize",
                               "Virtual Memory Size",
                               "On Windows, this figure contains the total "
                               "amount of memory that the memory manager has "
                               "committed for the arangod process. On other "
                               "systems, this figure contains The size of the "
                               "virtual memory the process is using.",
                               stats::FigureType::Current,
                               stats::Unit::Bytes,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "minorPageFaults",
                               "Minor Page Faults",
                               "The number of minor faults the process has "
                               "made which have not required loading a memory "
                               "page from disk. This figure is not reported on "
                               "Windows.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{
      stats::GroupType::System,
      "majorPageFaults",
      "Major Page Faults",
      "On Windows, this figure contains the total number of page faults. On "
      "other system, this figure contains the number of major faults the "
      "process has made which have required loading a memory page from disk.",
      stats::FigureType::Accumulated,
      stats::Unit::Number,
      {}});

  // .............................................................................
  // client statistics
  // .............................................................................

  _figures.emplace_back(
      Figure{stats::GroupType::Client,
             "httpConnections",
             "Client Connections",
             "The number of connections that are currently open.",
             stats::FigureType::Current,
             stats::Unit::Number,
             {}});

  _figures.emplace_back(
      Figure{stats::GroupType::Client, "totalTime", "Total Time",
             "Total time needed to answer a request.", stats::FigureType::Distribution,
             // cuts: internal.requestTimeDistribution,
             stats::Unit::Seconds, _requestTimeCuts});

  _figures.emplace_back(
      Figure{stats::GroupType::Client, "requestTime", "Request Time",
             "Request time needed to answer a request.", stats::FigureType::Distribution,
             // cuts: internal.requestTimeDistribution,
             stats::Unit::Seconds, _requestTimeCuts});

  _figures.emplace_back(
      Figure{stats::GroupType::Client, "queueTime", "Queue Time",
             "Queue time needed to answer a request.", stats::FigureType::Distribution,
             // cuts: internal.requestTimeDistribution,
             stats::Unit::Seconds, _requestTimeCuts});

  _figures.emplace_back(Figure{stats::GroupType::Client, "bytesSent",
                               "Bytes Sent", "Bytes sents for a request.",
                               stats::FigureType::Distribution,
                               // cuts: internal.bytesSentDistribution,
                               stats::Unit::Bytes, _bytesSendCuts});

  _figures.emplace_back(Figure{stats::GroupType::Client, "bytesReceived",
                               "Bytes Received", "Bytes received for a request.",
                               stats::FigureType::Distribution,
                               // cuts: internal.bytesReceivedDistribution,
                               stats::Unit::Bytes, _bytesReceivedCuts});

  _figures.emplace_back(
      Figure{stats::GroupType::Client, "connectionTime", "Connection Time",
             "Total connection time of a client.", stats::FigureType::Distribution,
             // cuts: internal.connectionTimeDistribution,
             stats::Unit::Seconds, _connectionTimeCuts});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsTotal",
                               "Total requests",
                               "Total number of HTTP requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(
      Figure{stats::GroupType::Http,
             "requestsAsync",
             "Async requests",
             "Number of asynchronously executed HTTP requests.",
             stats::FigureType::Accumulated,
             stats::Unit::Number,
             {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsGet",
                               "HTTP GET requests",
                               "Number of HTTP GET requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsHead",
                               "HTTP HEAD requests",
                               "Number of HTTP HEAD requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsPost",
                               "HTTP POST requests",
                               "Number of HTTP POST requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsPut",
                               "HTTP PUT requests",
                               "Number of HTTP PUT requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsPatch",
                               "HTTP PATCH requests",
                               "Number of HTTP PATCH requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsDelete",
                               "HTTP DELETE requests",
                               "Number of HTTP DELETE requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsOptions",
                               "HTTP OPTIONS requests",
                               "Number of HTTP OPTIONS requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsOther",
                               "other HTTP requests",
                               "Number of other HTTP requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  // .............................................................................
  // server statistics
  // .............................................................................

  _figures.emplace_back(Figure{stats::GroupType::Server,
                               "uptime",
                               "Server Uptime",
                               "Number of seconds elapsed since server start.",
                               stats::FigureType::Current,
                               stats::Unit::Seconds,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Server,
                               "physicalMemory",
                               "Physical Memory",
                               "Physical memory in bytes.",
                               stats::FigureType::Current,
                               stats::Unit::Bytes,
                               {}});
}

void stats::Descriptions::serverStatistics(velocypack::Builder& b) const {
  ServerStatistics info = ServerStatistics::statistics();
  b.add("uptime", VPackValue(info._uptime));
  b.add("physicalMemory", VPackValue(TRI_PhysicalMemory));

  V8DealerFeature* dealer =
      application_features::ApplicationServer::getFeature<V8DealerFeature>(
          "V8Dealer");
  if (dealer->isEnabled()) {
    b.add("v8Context", VPackValue(VPackValueType::Object, true));
    auto v8Counters = dealer->getCurrentContextNumbers();
    b.add("available", VPackValue(v8Counters.available));
    b.add("busy", VPackValue(v8Counters.busy));
    b.add("dirty", VPackValue(v8Counters.dirty));
    b.add("free", VPackValue(v8Counters.free));
    b.add("max", VPackValue(v8Counters.max));
    b.close();
  }

  b.add("threads", VPackValue(VPackValueType::Object, true));
  SchedulerFeature::SCHEDULER->toVelocyPack(b);
  b.close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the distribution
////////////////////////////////////////////////////////////////////////////////

static void FillDistribution(VPackBuilder& b, std::string const& name,
                             basics::StatisticsDistribution const& dist) {
  b.add(name, VPackValue(VPackValueType::Object, true));
  b.add("sum", VPackValue(dist._total));
  b.add("count", VPackValue(dist._count));
  b.add("counts", VPackValue(VPackValueType::Array, true));
  for (auto const& it : dist._counts) {
    b.add(VPackValue(it));
  }
  b.close();
  b.close();
}

void stats::Descriptions::clientStatistics(velocypack::Builder& b) const {
  basics::StatisticsCounter httpConnections;
  basics::StatisticsCounter totalRequests;
  std::array<basics::StatisticsCounter, basics::MethodRequestsStatisticsSize> methodRequests;
  basics::StatisticsCounter asyncRequests;
  basics::StatisticsDistribution connectionTime;

  // FIXME why are httpConnections in here ?
  ConnectionStatistics::fill(httpConnections, totalRequests, methodRequests,
                             asyncRequests, connectionTime);

  b.add("httpConnections", VPackValue(httpConnections._count));
  FillDistribution(b, "connectionTime", connectionTime);

  basics::StatisticsDistribution totalTime;
  basics::StatisticsDistribution requestTime;
  basics::StatisticsDistribution queueTime;
  basics::StatisticsDistribution ioTime;
  basics::StatisticsDistribution bytesSent;
  basics::StatisticsDistribution bytesReceived;

  RequestStatistics::fill(totalTime, requestTime, queueTime, ioTime, bytesSent, bytesReceived);

  FillDistribution(b, "totalTime", totalTime);
  FillDistribution(b, "requestTime", requestTime);
  FillDistribution(b, "queueTime", queueTime);
  FillDistribution(b, "ioTime", ioTime);
  FillDistribution(b, "bytesSent", bytesSent);
  FillDistribution(b, "bytesReceived", bytesReceived);
}

void stats::Descriptions::httpStatistics(velocypack::Builder& b) const {
  basics::StatisticsCounter httpConnections;
  basics::StatisticsCounter totalRequests;
  std::array<basics::StatisticsCounter, basics::MethodRequestsStatisticsSize> methodRequests;
  basics::StatisticsCounter asyncRequests;
  basics::StatisticsDistribution connectionTime;

  ConnectionStatistics::fill(httpConnections, totalRequests, methodRequests,
                             asyncRequests, connectionTime);

  // request counters
  b.add("requestsTotal", VPackValue(totalRequests._count));
  b.add("requestsAsync", VPackValue(asyncRequests._count));
  b.add("requestsGet", VPackValue(methodRequests[(int)rest::RequestType::GET]._count));
  b.add("requestsHead", VPackValue(methodRequests[(int)rest::RequestType::HEAD]._count));
  b.add("requestsPost", VPackValue(methodRequests[(int)rest::RequestType::POST]._count));
  b.add("requestsPut", VPackValue(methodRequests[(int)rest::RequestType::PUT]._count));
  b.add("requestsPatch",
        VPackValue(methodRequests[(int)rest::RequestType::PATCH]._count));
  b.add("requestsDelete",
        VPackValue(methodRequests[(int)rest::RequestType::DELETE_REQ]._count));
  b.add("requestsOptions",
        VPackValue(methodRequests[(int)rest::RequestType::OPTIONS]._count));
  b.add("requestsOther",
        VPackValue(methodRequests[(int)rest::RequestType::ILLEGAL]._count));
}

void stats::Descriptions::processStatistics(VPackBuilder& b) const {
  ProcessInfo info = TRI_ProcessInfoSelf();
  double rss = (double)info._residentSize;
  double rssp = 0;

  if (TRI_PhysicalMemory != 0) {
    rssp = rss / TRI_PhysicalMemory;
  }

  b.add("minorPageFaults", VPackValue(info._minorPageFaults));
  b.add("majorPageFaults", VPackValue(info._majorPageFaults));
  b.add("userTime", VPackValue((double)info._userTime / (double)info._scClkTck));
  b.add("systemTime", VPackValue((double)info._systemTime / (double)info._scClkTck));
  b.add("numberOfThreads", VPackValue(info._numberThreads));
  b.add("residentSize", VPackValue(rss));
  b.add("residentSizePercent", VPackValue(rssp));
  b.add("virtualSize", VPackValue(info._virtualSize));
}
