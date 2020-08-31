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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////
#include "v8-deadline.h"
// arangod dummy implementation doing nothing
void setExecutionDeadlineInMS(uint64_t timeout) {
}

bool isExecutionDeadlineReached(v8::Isolate* isolate) {
  return false;
}

double correctTimeoutToExecutionDeadlineS(double timeoutSeconds) {
  return timeoutSeconds;
}

std::chrono::milliseconds correctTimeoutToExecutionDeadline(std::chrono::milliseconds timeout) {
  return timeout;
}

void TRI_InitV8Deadline(v8::Isolate* isolate)
{
}
