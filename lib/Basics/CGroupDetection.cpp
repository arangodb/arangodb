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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "Basics/CGroupDetection.h"
#include "Basics/files.h"

namespace arangodb::cgroup {

namespace {

/// @brief detect which cgroup version is in use on the system
Version detectCGroupVersionImpl() {
  if (TRI_ExistsFile("/sys/fs/cgroup/cgroup.controllers")) {
    return Version::V2;
  }

  if (TRI_ExistsFile("/sys/fs/cgroup/cpu") ||
      TRI_ExistsFile("/sys/fs/cgroup/memory")) {
    return Version::V1;
  }

  return Version::NONE;
}

}  // namespace

/// @brief return cached cgroup version using Meyer's Singleton pattern
/// to avoid static initialization order fiasco
Version getVersion() {
  static Version cachedVersion = detectCGroupVersionImpl();
  return cachedVersion;
}

}  // namespace arangodb::cgroup
