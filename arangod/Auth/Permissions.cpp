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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////
#include "Permissions.h"

#include <Assertions/ProdAssert.h>

namespace arangodb {

auto to_string(CollectionAccessLevel level) -> std::string_view {
  switch (level) {
    case CollectionAccessLevel::None:
      return "none";
    case CollectionAccessLevel::Read:
      return "read";
    case CollectionAccessLevel::WriteData:
      return "writedata";
    case CollectionAccessLevel::WriteMeta:
      return "writemeta";
  }
  ADB_PROD_CRASH();
}

auto to_string(DatabaseAccessLevel level) -> std::string_view {
  switch (level) {
    case DatabaseAccessLevel::None:
      return "none";
    case DatabaseAccessLevel::Read:
      return "read";
    case DatabaseAccessLevel::Write:
      return "write";
  }
}

auto to_string(ViewAccessLevel level) -> std::string_view {
  switch (level) {
    case ViewAccessLevel::None:
      return "none";
    case ViewAccessLevel::Read:
      return "read";
    case ViewAccessLevel::Modify:
      return "modify";
  }
}

auto to_string(AnalyzerAccessLevel level) -> std::string_view {
  switch (level) {
    case AnalyzerAccessLevel::None:
      return "none";
    case AnalyzerAccessLevel::Read:
      return "read";
    case AnalyzerAccessLevel::Modify:
      return "modify";
  }
}

}  // namespace arangodb
