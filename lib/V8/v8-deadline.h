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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include <chrono>
#include <v8.h>
#include <optional>
#include "Basics/process-utils.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
}  // namespace arangodb

////////////////////////////////////////////////////////////////////////////////
/// @brief set a point in time after which we will abort external connection
////////////////////////////////////////////////////////////////////////////////
bool isExecutionDeadlineReached();
bool isExecutionDeadlineReached(v8::Isolate* isolate);
double correctTimeoutToExecutionDeadlineS(double timeoutSeconds);
std::chrono::milliseconds correctTimeoutToExecutionDeadline(
    std::chrono::milliseconds timeout);
uint32_t correctTimeoutToExecutionDeadline(uint32_t timeout);

void TRI_InitV8Deadline(v8::Isolate* isolate);

// make the deadline handling bite Now.
void triggerV8DeadlineNow(bool fromSignal);

namespace arangodb {
extern std::optional<ExternalProcessStatus> getHistoricStatus(
    TRI_pid_t pid, arangodb::application_features::ApplicationServer& server);
}
