////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <map>
#include <optional>
#include <vector>
#include "Basics/process-utils.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief external process stati of processes exited while being monitored
////////////////////////////////////////////////////////////////////////////////

extern std::map<TRI_pid_t, ExternalProcessStatus> ExitedExternalProcessStatus;

extern std::optional<ExternalProcessStatus> getHistoricStatus(TRI_pid_t pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief external process being monitored
////////////////////////////////////////////////////////////////////////////////

extern std::vector<ExternalId> monitoredProcesses;

////////////////////////////////////////////////////////////////////////////////
/// @brief enlist external process to become monitored
////////////////////////////////////////////////////////////////////////////////

extern void addMonitorPID(ExternalId& pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief enlist external process to become monitored
////////////////////////////////////////////////////////////////////////////////

extern void removeMonitorPID(ExternalId const& pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief launch the actual process monitor worker thread
////////////////////////////////////////////////////////////////////////////////

extern void launchMonitorThread(
    arangodb::application_features::ApplicationServer& server);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop external process motiror worker thread
////////////////////////////////////////////////////////////////////////////////

extern void terminateMonitorThread(
    arangodb::application_features::ApplicationServer& server);

}  // namespace arangodb
