////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "v8-statistics.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/StringUtils.h"
#include "Basics/process-utils.h"
#include "Rest/GeneralRequest.h"
#include "RestServer/MetricsFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"
#include "Statistics/ServerStatistics.h"
#include "Statistics/StatisticsFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a distribution vector
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Array> DistributionList(v8::Isolate* isolate,
                                              std::initializer_list<double> const& dist) {
  v8::EscapableHandleScope scope(isolate);
  auto context = TRI_IGETC;

  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  uint32_t i = 0;
  for (auto const& val : dist) {
    result->Set(context, i, v8::Number::New(isolate, val)).FromMaybe(false);
    ++i;
  }

  return scope.Escape<v8::Array>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the distribution
////////////////////////////////////////////////////////////////////////////////

static void FillDistribution(v8::Isolate* isolate, v8::Handle<v8::Object> list,
                             v8::Handle<v8::String> name,
                             statistics::Distribution const& dist) {
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  auto context = TRI_IGETC;

  result->Set(context, TRI_V8_ASCII_STRING(isolate, "sum"), v8::Number::New(isolate, dist._total)).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "count"),
              v8::Number::New(isolate, (double)dist._count)).FromMaybe(false);

  v8::Handle<v8::Array> counts = v8::Array::New(isolate, (int)dist._counts.size());
  uint32_t pos = 0;

  for (auto i = dist._counts.begin(); i != dist._counts.end(); ++i, ++pos) {
    counts->Set(context, pos, v8::Number::New(isolate, (double)*i)).FromMaybe(false);
  }

  result->Set(context, TRI_V8_ASCII_STRING(isolate, "counts"), counts).FromMaybe(false);

  list->Set(context, name, result).FromMaybe(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns server statistics
///
/// @FUN{internal.serverStatistics()}
///
/// Returns information about the server:
///
/// - `uptime`: time since server start in seconds.
////////////////////////////////////////////////////////////////////////////////

static void JS_ServerStatistics(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  TRI_GET_GLOBALS();
  ServerStatistics const& info =
      v8g->_server.getFeature<MetricsFeature>().serverStatistics();

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->Set(context, TRI_V8_ASCII_STRING(isolate, "uptime"),
              v8::Number::New(isolate, (double)info.uptime())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "physicalMemory"),
              v8::Number::New(isolate, (double)PhysicalMemory::getValue())).FromMaybe(false);

  // transaction info
  auto const& ts = info._transactionsStatistics;
  v8::Handle<v8::Object> v8TransactionInfoObj = v8::Object::New(isolate);
  v8TransactionInfoObj->Set(context, TRI_V8_ASCII_STRING(isolate, "started"),
              v8::Number::New(isolate, (double)ts._transactionsStarted.load())).FromMaybe(false);
  v8TransactionInfoObj->Set(context, TRI_V8_ASCII_STRING(isolate, "aborted"),
              v8::Number::New(isolate, (double)ts._transactionsAborted.load())).FromMaybe(false);
  v8TransactionInfoObj->Set(context, TRI_V8_ASCII_STRING(isolate, "committed"),
              v8::Number::New(isolate, (double)ts._transactionsCommitted.load())).FromMaybe(false);
  v8TransactionInfoObj->Set(context, TRI_V8_ASCII_STRING(isolate, "intermediateCommits"),
              v8::Number::New(isolate, (double)ts._intermediateCommits.load())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "transactions"), v8TransactionInfoObj).FromMaybe(false);

  // v8 counters
  V8DealerFeature& dealer = v8g->_server.getFeature<V8DealerFeature>();
  auto v8Counters = dealer.getCurrentContextNumbers();
  v8::Handle<v8::Object> v8CountersObj = v8::Object::New(isolate);
  v8CountersObj->Set(context, TRI_V8_ASCII_STRING(isolate, "available"),
                     v8::Number::New(isolate, static_cast<uint32_t>(v8Counters.available))).FromMaybe(false);
  v8CountersObj->Set(context, TRI_V8_ASCII_STRING(isolate, "busy"),
                     v8::Number::New(isolate, static_cast<uint32_t>(v8Counters.busy))).FromMaybe(false);
  v8CountersObj->Set(context, TRI_V8_ASCII_STRING(isolate, "dirty"),
                     v8::Number::New(isolate, static_cast<uint32_t>(v8Counters.dirty))).FromMaybe(false);
  v8CountersObj->Set(context, TRI_V8_ASCII_STRING(isolate, "free"),
                     v8::Number::New(isolate, static_cast<uint32_t>(v8Counters.free))).FromMaybe(false);
  v8CountersObj->Set(context, TRI_V8_ASCII_STRING(isolate, "max"),
                     v8::Number::New(isolate, static_cast<uint32_t>(v8Counters.max))).FromMaybe(false);
  v8CountersObj->Set(context, TRI_V8_ASCII_STRING(isolate, "min"),
                     v8::Number::New(isolate, static_cast<uint32_t>(v8Counters.min))).FromMaybe(false);

  auto memoryStatistics = dealer.getCurrentContextDetails();

  v8::Handle<v8::Array> v8ListOfMemory = v8::Array::New(isolate, static_cast<int>(memoryStatistics.size()));
  uint32_t pos = 0;
  for (auto memStatistic : memoryStatistics) {
    v8::Handle<v8::Object> v8MemStat = v8::Object::New(isolate);
    v8MemStat->Set(context, TRI_V8_ASCII_STRING(isolate, "contextId"),
                   v8::Integer::New(isolate, static_cast<uint32_t>(memStatistic.id))).FromMaybe(false);
    v8MemStat->Set(context, TRI_V8_ASCII_STRING(isolate, "tMax"),
                   v8::Number::New(isolate, (double)memStatistic.tMax)).FromMaybe(false);
    v8MemStat->Set(context, TRI_V8_ASCII_STRING(isolate, "countOfTimes"),
                   v8::Integer::New(isolate, static_cast<uint32_t>(memStatistic.countOfTimes))).FromMaybe(false);
    v8MemStat->Set(context, TRI_V8_ASCII_STRING(isolate, "heapMax"),
                   v8::Number::New(isolate, (double)memStatistic.heapMax)).FromMaybe(false);
    v8MemStat->Set(context, TRI_V8_ASCII_STRING(isolate, "heapMin"),
                   v8::Number::New(isolate, (double)memStatistic.heapMin)).FromMaybe(false);
    v8MemStat->Set(context, TRI_V8_ASCII_STRING(isolate, "invocations"),
                   v8::Integer::New(isolate, static_cast<uint32_t>(memStatistic.invocations))).FromMaybe(false);

    v8ListOfMemory->Set(context, pos++, v8MemStat).FromMaybe(false);
  }

  v8CountersObj->Set(context, TRI_V8_ASCII_STRING(isolate, "memory"),
                     v8ListOfMemory).FromMaybe(false);

  result->Set(context, TRI_V8_ASCII_STRING(isolate, "v8Context"), v8CountersObj).FromMaybe(false);

  v8::Handle<v8::Object> counters = v8::Object::New(isolate);

  auto qs = SchedulerFeature::SCHEDULER->queueStatistics();

  counters->Set(context, TRI_V8_ASCII_STRING(isolate, "schedulerThreads"),
                v8::Number::New(isolate, static_cast<int32_t>(qs._running))).FromMaybe(false);

  counters->Set(context, TRI_V8_ASCII_STRING(isolate, "inProgress"),
                v8::Number::New(isolate, static_cast<int32_t>(qs._working))).FromMaybe(false);

  counters->Set(context, TRI_V8_ASCII_STRING(isolate, "queued"),
                v8::Number::New(isolate, static_cast<int32_t>(qs._queued))).FromMaybe(false);

  result->Set(context, TRI_V8_ASCII_STRING(isolate, "threads"), counters).FromMaybe(false);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not server-side statistics are enabled
////////////////////////////////////////////////////////////////////////////////

static void JS_EnabledStatistics(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Value> result = v8::Boolean::New(isolate, StatisticsFeature::enabled());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current request and connection statistics
////////////////////////////////////////////////////////////////////////////////

static void JS_ClientStatistics(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  ConnectionStatistics::Snapshot connectionStats;
  ConnectionStatistics::getSnapshot(connectionStats);

  result->Set(context, TRI_V8_ASCII_STRING(isolate, "httpConnections"),
              v8::Number::New(isolate, (double)connectionStats.httpConnections.get())).FromMaybe(false);
  FillDistribution(isolate, result,
                   TRI_V8_ASCII_STRING(isolate, "connectionTime"), connectionStats.connectionTime);

  RequestStatistics::Snapshot requestStats;
  RequestStatistics::getSnapshot(requestStats, stats::RequestStatisticsSource::ALL);

  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "totalTime"), requestStats.totalTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "requestTime"), requestStats.requestTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "queueTime"), requestStats.queueTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "ioTime"), requestStats.ioTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "bytesSent"), requestStats.bytesSent);
  FillDistribution(isolate, result,
                   TRI_V8_ASCII_STRING(isolate, "bytesReceived"), requestStats.bytesReceived);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current http statistics
////////////////////////////////////////////////////////////////////////////////

static void JS_HttpStatistics(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  ConnectionStatistics::Snapshot stats;
  ConnectionStatistics::getSnapshot(stats);

  using rest::RequestType;

  // request counters
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsTotal"),
              v8::Number::New(isolate, (double)stats.totalRequests.get())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsSuperuser"),
              v8::Number::New(isolate, (double)stats.totalRequestsSuperuser.get())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsUser"),
              v8::Number::New(isolate, (double)stats.totalRequestsUser.get())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsAsync"),
              v8::Number::New(isolate, (double)stats.asyncRequests.get())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsGet"),
              v8::Number::New(
                  isolate, (double)stats.methodRequests[(int)RequestType::GET].get())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsHead"),
              v8::Number::New(
                  isolate, (double)stats.methodRequests[(int)RequestType::HEAD].get())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsPost"),
              v8::Number::New(
                  isolate, (double)stats.methodRequests[(int)RequestType::POST].get())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsPut"),
              v8::Number::New(
                  isolate, (double)stats.methodRequests[(int)RequestType::PUT].get())).FromMaybe(false);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "requestsPatch"),
              v8::Number::New(
                  isolate, (double)stats.methodRequests[(int)RequestType::PATCH].get())).FromMaybe(false);
  result->Set(context, 
      TRI_V8_ASCII_STRING(isolate, "requestsDelete"),
      v8::Number::New(isolate,
                      (double)stats.methodRequests[(int)RequestType::DELETE_REQ].get())).FromMaybe(false);
  result->Set(context, 
      TRI_V8_ASCII_STRING(isolate, "requestsOptions"),
      v8::Number::New(isolate,
                      (double)stats.methodRequests[(int)rest::RequestType::OPTIONS].get())).FromMaybe(false);
  result->Set(context, 
      TRI_V8_ASCII_STRING(isolate, "requestsOther"),
      v8::Number::New(isolate,
                      (double)stats.methodRequests[(int)rest::RequestType::ILLEGAL].get())).FromMaybe(false);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the statistics functions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Statistics(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_ENABLED_STATISTICS"),
                               JS_EnabledStatistics);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_CLIENT_STATISTICS"),
                               JS_ClientStatistics);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "SYS_HTTP_STATISTICS"), JS_HttpStatistics);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_SERVER_STATISTICS"),
                               JS_ServerStatistics);

  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "CONNECTION_TIME_DISTRIBUTION"),
      DistributionList(isolate, statistics::ConnectionTimeDistributionCuts));
  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REQUEST_TIME_DISTRIBUTION"),
      DistributionList(isolate, statistics::RequestTimeDistributionCuts));
  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "BYTES_SENT_DISTRIBUTION"),
      DistributionList(isolate, statistics::BytesSentDistributionCuts));
  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "BYTES_RECEIVED_DISTRIBUTION"),
      DistributionList(isolate, statistics::BytesReceivedDistributionCuts));
}
