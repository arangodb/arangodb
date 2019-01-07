////////////////////////////////////////////////////////////////////////////////
/// disclaimer
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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "CriticalThread.h"
#include "HeartbeatThread.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief static object to record thread crashes.  it is static so that
///        it can contain information about threads that crash before
///        HeartbeatThread starts (i.e. HeartbeatThread starts late).  this list
///        is intentionally NEVER PURGED so that it can be reposted to logs
///        regularly
////////////////////////////////////////////////////////////////////////////////

void CriticalThread::crashNotification(std::exception const& ex) {
  HeartbeatThread::recordThreadDeath(name());
}
