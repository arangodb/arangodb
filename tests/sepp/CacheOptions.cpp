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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CacheOptions.h"

#include "Basics/PhysicalMemory.h"
#include "Cache/CacheOptionsProvider.h"

namespace arangodb::sepp {

CacheOptions::CacheOptions() {
  _options.cacheSize =
      (PhysicalMemory::getValue() >= (static_cast<std::uint64_t>(4) << 30))
          ? static_cast<std::uint64_t>((PhysicalMemory::getValue() -
                                        (static_cast<std::uint64_t>(2) << 30)) *
                                       0.25)
          : (256 << 20);
  // currently there is no way to turn stats off
  _options.enableWindowedStats = true;
}

}  // namespace arangodb::sepp
