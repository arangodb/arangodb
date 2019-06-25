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
#include "Basics/StringUtils.h"
#include "Basics/process-utils.h"
#include "Rest/GeneralRequest.h"
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
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a distribution vector
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Array> DistributionList(v8::Isolate* isolate,
                                              std::vector<double> const& dist) {
  v8::EscapableHandleScope scope(isolate);

  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  for (uint32_t i = 0; i < (uint32_t)dist.size(); ++i) {
    result->Set(i, v8::Number::New(isolate, dist[i]));
  }

  return scope.Escape<v8::Array>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the distribution
////////////////////////////////////////////////////////////////////////////////

static void FillDistribution(v8::Isolate* isolate, v8::Handle<v8::Object> list,
                             v8::Handle<v8::String> name,
                             StatisticsDistribution const& dist) {
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING(isolate, "sum"), v8::Number::New(isolate, dist._total));
  result->Set(TRI_V8_ASCII_STRING(isolate, "count"),
              v8::Number::New(isolate, (double)dist._count));

  v8::Handle<v8::Array> counts = v8::Array::New(isolate, (int)dist._counts.size());
  uint32_t pos = 0;

  for (auto i = dist._counts.begin(); i != dist._counts.end(); ++i, ++pos) {
    counts->Set(pos, v8::Number::New(isolate, (double)*i));
  }

  result->Set(TRI_V8_ASCII_STRING(isolate, "counts"), counts);

  list->Set(name, result);
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

  ServerStatistics info = ServerStatistics::statistics();

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING(isolate, "uptime"),
              v8::Number::New(isolate, (double)info._uptime));
  result->Set(TRI_V8_ASCII_STRING(isolate, "physicalMemory"),
              v8::Number::New(isolate, (double)TRI_PhysicalMemory));

  v8::Handle<v8::Object> v8CountersObj = v8::Object::New(isolate);
  V8DealerFeature* dealer =
      application_features::ApplicationServer::getFeature<V8DealerFeature>(
          "V8Dealer");

  auto v8Counters = dealer->getCurrentContextNumbers();
  v8CountersObj->Set(TRI_V8_ASCII_STRING(isolate, "available"),
                     v8::Number::New(isolate, static_cast<int32_t>(v8Counters.available)));
  v8CountersObj->Set(TRI_V8_ASCII_STRING(isolate, "busy"),
                     v8::Number::New(isolate, static_cast<int32_t>(v8Counters.busy)));
  v8CountersObj->Set(TRI_V8_ASCII_STRING(isolate, "dirty"),
                     v8::Number::New(isolate, static_cast<int32_t>(v8Counters.dirty)));
  v8CountersObj->Set(TRI_V8_ASCII_STRING(isolate, "free"),
                     v8::Number::New(isolate, static_cast<int32_t>(v8Counters.free)));
  v8CountersObj->Set(TRI_V8_ASCII_STRING(isolate, "max"),
                     v8::Number::New(isolate, static_cast<int32_t>(v8Counters.max)));
  result->Set(TRI_V8_ASCII_STRING(isolate, "v8Context"), v8CountersObj);

  v8::Handle<v8::Object> counters = v8::Object::New(isolate);

  auto qs = SchedulerFeature::SCHEDULER->queueStatistics();

  counters->Set(TRI_V8_ASCII_STRING(isolate, "schedulerThreads"),
                v8::Number::New(isolate, static_cast<int32_t>(qs._running)));

  counters->Set(TRI_V8_ASCII_STRING(isolate, "inProgress"),
                v8::Number::New(isolate, static_cast<int32_t>(qs._working)));

  counters->Set(TRI_V8_ASCII_STRING(isolate, "queued"),
                v8::Number::New(isolate, static_cast<int32_t>(qs._queued)));

  result->Set(TRI_V8_ASCII_STRING(isolate, "threads"), counters);

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

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  StatisticsCounter httpConnections;
  StatisticsCounter totalRequests;
  std::array<StatisticsCounter, MethodRequestsStatisticsSize> methodRequests;
  StatisticsCounter asyncRequests;
  StatisticsDistribution connectionTime;

  ConnectionStatistics::fill(httpConnections, totalRequests, methodRequests,
                             asyncRequests, connectionTime);

  result->Set(TRI_V8_ASCII_STRING(isolate, "httpConnections"),
              v8::Number::New(isolate, (double)httpConnections._count));
  FillDistribution(isolate, result,
                   TRI_V8_ASCII_STRING(isolate, "connectionTime"), connectionTime);

  StatisticsDistribution totalTime;
  StatisticsDistribution requestTime;
  StatisticsDistribution queueTime;
  StatisticsDistribution ioTime;
  StatisticsDistribution bytesSent;
  StatisticsDistribution bytesReceived;

  RequestStatistics::fill(totalTime, requestTime, queueTime, ioTime, bytesSent, bytesReceived);

  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "totalTime"), totalTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "requestTime"), requestTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "queueTime"), queueTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "ioTime"), ioTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING(isolate, "bytesSent"), bytesSent);
  FillDistribution(isolate, result,
                   TRI_V8_ASCII_STRING(isolate, "bytesReceived"), bytesReceived);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current http statistics
////////////////////////////////////////////////////////////////////////////////

static void JS_HttpStatistics(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  StatisticsCounter httpConnections;
  StatisticsCounter totalRequests;
  std::array<StatisticsCounter, MethodRequestsStatisticsSize> methodRequests;
  StatisticsCounter asyncRequests;
  StatisticsDistribution connectionTime;

  ConnectionStatistics::fill(httpConnections, totalRequests, methodRequests,
                             asyncRequests, connectionTime);

  // request counters
  result->Set(TRI_V8_ASCII_STRING(isolate, "requestsTotal"),
              v8::Number::New(isolate, (double)totalRequests._count));
  result->Set(TRI_V8_ASCII_STRING(isolate, "requestsAsync"),
              v8::Number::New(isolate, (double)asyncRequests._count));
  result->Set(TRI_V8_ASCII_STRING(isolate, "requestsGet"),
              v8::Number::New(
                  isolate, (double)methodRequests[(int)rest::RequestType::GET]._count));
  result->Set(TRI_V8_ASCII_STRING(isolate, "requestsHead"),
              v8::Number::New(
                  isolate, (double)methodRequests[(int)rest::RequestType::HEAD]._count));
  result->Set(TRI_V8_ASCII_STRING(isolate, "requestsPost"),
              v8::Number::New(
                  isolate, (double)methodRequests[(int)rest::RequestType::POST]._count));
  result->Set(TRI_V8_ASCII_STRING(isolate, "requestsPut"),
              v8::Number::New(
                  isolate, (double)methodRequests[(int)rest::RequestType::PUT]._count));
  result->Set(TRI_V8_ASCII_STRING(isolate, "requestsPatch"),
              v8::Number::New(
                  isolate, (double)methodRequests[(int)rest::RequestType::PATCH]._count));
  result->Set(
      TRI_V8_ASCII_STRING(isolate, "requestsDelete"),
      v8::Number::New(isolate,
                      (double)methodRequests[(int)rest::RequestType::DELETE_REQ]._count));
  result->Set(
      TRI_V8_ASCII_STRING(isolate, "requestsOptions"),
      v8::Number::New(isolate,
                      (double)methodRequests[(int)rest::RequestType::OPTIONS]._count));
  result->Set(
      TRI_V8_ASCII_STRING(isolate, "requestsOther"),
      v8::Number::New(isolate,
                      (double)methodRequests[(int)rest::RequestType::ILLEGAL]._count));

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
      DistributionList(isolate, TRI_ConnectionTimeDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "REQUEST_TIME_DISTRIBUTION"),
      DistributionList(isolate, TRI_RequestTimeDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "BYTES_SENT_DISTRIBUTION"),
      DistributionList(isolate, TRI_BytesSentDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "BYTES_RECEIVED_DISTRIBUTION"),
      DistributionList(isolate, TRI_BytesReceivedDistributionVectorStatistics));
}
