////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Scheduler/SchedulerFeatureOptions.h"

#include "Basics/NumberOfCores.h"

#include <algorithm>

namespace arangodb {

/*static*/ uint64_t SchedulerFeatureOptions::getDefaultMaxThreads() noexcept {
  // use two times the number of hardware threads as the default,
  // but never less than 32
  return (std::max)(static_cast<uint64_t>(32),
                    static_cast<uint64_t>(NumberOfCores::getValue()) * 2);
}

SchedulerFeatureOptions::SchedulerFeatureOptions()
    : nrMaximalThreads(getDefaultMaxThreads()) {}

}  // namespace arangodb
