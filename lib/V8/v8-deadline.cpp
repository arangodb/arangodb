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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////
#include "V8/v8-deadline.h"
#include "Basics/system-functions.h"
#include "v8-utils.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief set a point in time after which we will abort certain operations
////////////////////////////////////////////////////////////////////////////////
static double executionDeadline = 0.0;

// arangosh only: set a deadline
static void JS_SetExecutionDeadlineTo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("SetGlobalExecutionDeadlineTo(<timeout>)");
  }

  auto when = executionDeadline;
  auto now = TRI_microtime();

  auto n = TRI_ObjectToUInt64(isolate, args[0], false);
  if (n == 0) {
    executionDeadline = 0.0;
  } else {
    executionDeadline = TRI_microtime() + n / 1000;
  }

  TRI_V8_RETURN_BOOL((when > 0.00001) && (now - when > 0.0) );
  TRI_V8_TRY_CATCH_END
}

bool isExecutionDeadlineReached(v8::Isolate* isolate) {
  auto when = executionDeadline;
  if (when < 0.00001) {
    return false;
  }
  auto now = TRI_microtime();
  if (now < when) {
    return false;
  }

  TRI_CreateErrorObject(isolate, TRI_ERROR_DISABLED, "Execution deadline reached!", true);
  return true;
}

double correctTimeoutToExecutionDeadlineS(double timeoutSeconds) {
  auto when = executionDeadline;
  if (when < 0.00001) {
    return timeoutSeconds;
  }
  auto now = TRI_microtime();
  auto delta = when - now;
  if (delta > timeoutSeconds) {
    return timeoutSeconds;
  }
  return delta;
}

std::chrono::milliseconds correctTimeoutToExecutionDeadline(std::chrono::milliseconds timeout) {
  using namespace std::chrono;

  double epochDoubleWhen = executionDeadline;
  if (epochDoubleWhen < 0.00001) {
    return timeout;
  }

  time_point now = std::chrono::system_clock::now();
  milliseconds durationWhen(static_cast<int>(epochDoubleWhen * 1000));
  time_point<system_clock> timepointWhen(durationWhen);

  milliseconds delta = duration_cast<milliseconds>(now - timepointWhen);
  if (delta > timeout) {
    return timeout;
  }
  return delta;

}

void TRI_InitV8Deadline(v8::Isolate* isolate)
{
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_COMMUNICATE_SLEEP_DEADLINE"),
                               JS_SetExecutionDeadlineTo);
}
