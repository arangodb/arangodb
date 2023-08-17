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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/BuildId.h"

#include <elf.h>
#include <link.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <optional>
#include <string>

extern char build_id_start[];
extern char build_id_end;

namespace arangodb::build_id {

auto getBuildId() -> std::optional<BuildId> {
  auto const* noteMemory = reinterpret_cast<const char*>(&build_id_start);
  auto const* noteHeader = reinterpret_cast<ElfW(Nhdr) const*>(noteMemory);

  if (noteHeader->n_type == NT_GNU_BUILD_ID) {
    auto const* buildIdMemory = reinterpret_cast<const char*>(
        noteMemory + sizeof(ElfW(Nhdr)) + noteHeader->n_namesz);
    return BuildId{buildIdMemory, noteHeader->n_descsz};
  }

  return std::nullopt;
}

}  // namespace arangodb::build_id
