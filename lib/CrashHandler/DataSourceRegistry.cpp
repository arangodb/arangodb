////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include "CrashHandler/DataSourceRegistry.h"

#include <algorithm>

namespace arangodb::crash_handler {

void DataSourceRegistry::addDataSource(
    ICrashHandlerDataSource const* dataSource) {
  std::lock_guard guard(_dataSourceMtx);
  _dataSources.push_back(dataSource);
}

std::vector<ICrashHandlerDataSource const*> const&
DataSourceRegistry::getDataSources() const {
  return _dataSources;
}

void DataSourceRegistry::removeDataSource(
    ICrashHandlerDataSource const* dataSource) {
  std::lock_guard guard(_dataSourceMtx);
  std::ranges::remove(_dataSources, dataSource);
}

}  // namespace arangodb::crash_handler
