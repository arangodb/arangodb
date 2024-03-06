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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "BuildId/BuildId.h"

#include <elf.h>
#include <link.h>
#include <string>

extern char build_id_start[];
extern char build_id_end;

namespace arangodb::build_id {

constexpr const char* build_id_failed = "";

auto getBuildId() -> std::string_view {
  auto const* noteMemory = reinterpret_cast<const char*>(&build_id_start);
  auto const* noteHeader = reinterpret_cast<ElfW(Nhdr) const*>(noteMemory);

  if (noteHeader->n_type == NT_GNU_BUILD_ID) {
    auto const* buildIdMemory = reinterpret_cast<const char*>(
        noteMemory + sizeof(ElfW(Nhdr)) + noteHeader->n_namesz);
    return std::string_view{buildIdMemory, noteHeader->n_descsz};
  }
  return std::string_view{build_id_failed};
}

}  // namespace arangodb::build_id
