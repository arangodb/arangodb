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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb {

struct V8DealerFeatureOptions {
  double gcFrequency = 60.0;
  uint64_t gcInterval = 2000;
  double maxExecutorAge = 60.0;
  std::string appPath;
  std::string startupDirectory;
  std::vector<std::string> moduleDirectories;
  // maximum number of executors to create
  uint64_t nrMaxExecutors = 0;
  // minimum number of executors to keep
  uint64_t nrMinExecutors = 0;
  // maximum number of V8 context invocations
  uint64_t maxExecutorInvocations = 0;

  // copy JavaScript files into database directory on startup
  bool copyInstallation = false;
  // enable /_admin/execute API
  bool allowAdminExecute = false;
  // allow JavaScript transactions?
  bool allowJavaScriptTransactions = true;
  // allow JavaScript user-defined functions?
  bool allowJavaScriptUdfs = true;
  // allow JavaScript tasks (tasks module)?
  bool allowJavaScriptTasks = true;
  // enable JavaScript globally
  bool enableJS = true;
};

}  // namespace arangodb
