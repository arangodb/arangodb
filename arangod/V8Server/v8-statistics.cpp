////////////////////////////////////////////////////////////////////////////////
/// @brief V8 statistics functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-statistics.h"
#include "Basics/StringUtils.h"
#include "Statistics/statistics.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a distribution vector
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Array> DistributionList (v8::Isolate* isolate,
                                               StatisticsVector const& dist) {
  v8::EscapableHandleScope scope(isolate);

  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  for (uint32_t i = 0;  i < (uint32_t) dist._value.size();  ++i) {
    result->Set(i, v8::Number::New(isolate, dist._value[i]));
  }

  return scope.Escape<v8::Array>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the distribution
////////////////////////////////////////////////////////////////////////////////

static void FillDistribution (v8::Isolate* isolate,
                              v8::Handle<v8::Object> list,
                              v8::Handle<v8::String> name,
                              StatisticsDistribution const& dist) {
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("sum"), v8::Number::New(isolate, dist._total));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Number::New(isolate, (double) dist._count));

  v8::Handle<v8::Array> counts = v8::Array::New(isolate, (int) dist._counts.size());
  uint32_t pos = 0;

  for (vector<uint64_t>::const_iterator i = dist._counts.begin();  i != dist._counts.end();  ++i, ++pos) {
    counts->Set(pos, v8::Number::New(isolate, (double) *i));
  }

  result->Set(TRI_V8_ASCII_STRING("counts"), counts);

  list->Set(name, result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns server statistics
///
/// @FUN{internal.serverStatistics()}
///
/// Returns information about the server:
///
/// - `uptime`: time since server start in seconds.
////////////////////////////////////////////////////////////////////////////////

static void JS_ServerStatistics (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  TRI_server_statistics_t info = TRI_GetServerStatistics();

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("uptime"),         v8::Number::New(isolate, (double) info._uptime));
  result->Set(TRI_V8_ASCII_STRING("physicalMemory"), v8::Number::New(isolate, (double) TRI_PhysicalMemory));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current request and connection statistics
////////////////////////////////////////////////////////////////////////////////

static void JS_ClientStatistics (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  StatisticsCounter httpConnections;
  StatisticsCounter totalRequests;
  vector<StatisticsCounter> methodRequests;
  StatisticsCounter asyncRequests;
  StatisticsDistribution connectionTime;

  TRI_FillConnectionStatistics(httpConnections, totalRequests, methodRequests, asyncRequests, connectionTime);

  result->Set(TRI_V8_ASCII_STRING("httpConnections"), v8::Number::New(isolate, (double) httpConnections._count));
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING("connectionTime"), connectionTime);

  StatisticsDistribution totalTime;
  StatisticsDistribution requestTime;
  StatisticsDistribution queueTime;
  StatisticsDistribution ioTime;
  StatisticsDistribution bytesSent;
  StatisticsDistribution bytesReceived;

  TRI_FillRequestStatistics(totalTime, requestTime, queueTime, ioTime, bytesSent, bytesReceived);

  FillDistribution(isolate, result, TRI_V8_ASCII_STRING("totalTime"),     totalTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING("requestTime"),   requestTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING("queueTime"),     queueTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING("ioTime"),        ioTime);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING("bytesSent"),     bytesSent);
  FillDistribution(isolate, result, TRI_V8_ASCII_STRING("bytesReceived"), bytesReceived);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current http statistics
////////////////////////////////////////////////////////////////////////////////

static void JS_HttpStatistics (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  StatisticsCounter httpConnections;
  StatisticsCounter totalRequests;
  vector<StatisticsCounter> methodRequests;
  StatisticsCounter asyncRequests;
  StatisticsDistribution connectionTime;

  TRI_FillConnectionStatistics(httpConnections, totalRequests, methodRequests, asyncRequests, connectionTime);

  // request counters
  result->Set(TRI_V8_ASCII_STRING("requestsTotal"),   v8::Number::New(isolate, (double) totalRequests._count));
  result->Set(TRI_V8_ASCII_STRING("requestsAsync"),   v8::Number::New(isolate, (double) asyncRequests._count));
  result->Set(TRI_V8_ASCII_STRING("requestsGet"),     v8::Number::New(isolate, (double) methodRequests[(int) HttpRequest::HTTP_REQUEST_GET]._count));
  result->Set(TRI_V8_ASCII_STRING("requestsHead"),    v8::Number::New(isolate, (double) methodRequests[(int) HttpRequest::HTTP_REQUEST_HEAD]._count));
  result->Set(TRI_V8_ASCII_STRING("requestsPost"),    v8::Number::New(isolate, (double) methodRequests[(int) HttpRequest::HTTP_REQUEST_POST]._count));
  result->Set(TRI_V8_ASCII_STRING("requestsPut"),     v8::Number::New(isolate, (double) methodRequests[(int) HttpRequest::HTTP_REQUEST_PUT]._count));
  result->Set(TRI_V8_ASCII_STRING("requestsPatch"),   v8::Number::New(isolate, (double) methodRequests[(int) HttpRequest::HTTP_REQUEST_PATCH]._count));
  result->Set(TRI_V8_ASCII_STRING("requestsDelete"),  v8::Number::New(isolate, (double) methodRequests[(int) HttpRequest::HTTP_REQUEST_DELETE]._count));
  result->Set(TRI_V8_ASCII_STRING("requestsOptions"), v8::Number::New(isolate, (double) methodRequests[(int) HttpRequest::HTTP_REQUEST_OPTIONS]._count));
  result->Set(TRI_V8_ASCII_STRING("requestsOther"),   v8::Number::New(isolate, (double) methodRequests[(int) HttpRequest::HTTP_REQUEST_ILLEGAL]._count));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

// -----------------------------------------------------------------------------
// --SECTION--                                             module initialisation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the statistics functions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Statistics (v8::Isolate* isolate,
                           v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  // check the isolate
  TRI_v8_global_t* v8g = TRI_GetV8Globals(isolate);
  TRI_ASSERT(v8g != nullptr);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_CLIENT_STATISTICS"), JS_ClientStatistics);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_HTTP_STATISTICS"), JS_HttpStatistics);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_SERVER_STATISTICS"), JS_ServerStatistics);

  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("CONNECTION_TIME_DISTRIBUTION"), DistributionList(isolate, TRI_ConnectionTimeDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("REQUEST_TIME_DISTRIBUTION"), DistributionList(isolate, TRI_RequestTimeDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("BYTES_SENT_DISTRIBUTION"), DistributionList(isolate, TRI_BytesSentDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("BYTES_RECEIVED_DISTRIBUTION"), DistributionList(isolate, TRI_BytesReceivedDistributionVectorStatistics));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
