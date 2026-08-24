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
#pragma once

#include <cstdint>

namespace arangodb {
enum class IndexType : std::uint8_t {
  Unknown = 0,
  Primary = 1,
  Geo = 2,
  Geo1 = 3,
  Geo2 = 4,
  Hash = 5,
  Edge = 6,
  Fulltext = 7,
  Skiplist = 8,
  TTL = 9,
  Persistent = 10,
  IResearchLink = 11,
  NoAccess = 12,
  Zkd = 13,
  MDI = 14,
  MDIPrefixed = 15,
  Inverted = 16,
  Vector = 17,
};
}  // namespace arangodb
