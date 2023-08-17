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

#include <boost/align/align_up.hpp>

using namespace boost::alignment;

namespace arangodb::build_id {

struct MMappedElfHeader {
  explicit MMappedElfHeader() : _memory(MAP_FAILED), _size(0) {}
  MMappedElfHeader(MMappedElfHeader const&) = delete;
  MMappedElfHeader(MMappedElfHeader&& other) = delete;
  ~MMappedElfHeader() {
    if (_memory != MAP_FAILED) {
      munmap(_memory, _size);
    }
  }

  auto map() -> bool {
    int selffd;
    struct stat statbuf;

    if (selffd = open("/proc/self/exe", O_RDONLY); selffd < 0) {
      return false;
    }

    if (fstat(selffd, &statbuf) < 0) {
      close(selffd);
      return false;
    }

    _memory = mmap(0, statbuf.st_size, PROT_READ, MAP_PRIVATE, selffd, 0);

    // Intentional: After mmap returned successfully, fd can be closed without
    // the mapping becoming invalid. This way selffd is not leaked.
    close(selffd);

    if (_memory == MAP_FAILED) {
      return false;
    }

    _size = statbuf.st_size;
    return true;
  }

  auto get() const -> const char* {
    return reinterpret_cast<const char*>(_memory);
  }

  auto valid() const -> bool { return _memory != nullptr; }

 private:
  void* _memory;
  size_t _size;
};

auto getBuildId() -> std::optional<BuildId> {
  auto mmappedElfHeader = MMappedElfHeader();

  auto mapped = mmappedElfHeader.map();
  if (!mapped) {
    return std::nullopt;
  }

  auto* elfHeaderMemory = mmappedElfHeader.get();
  auto* elfHeader = reinterpret_cast<ElfW(Ehdr) const*>(elfHeaderMemory);

  auto* programHeadersMemory = elfHeaderMemory + elfHeader->e_phoff;
  auto* programHeaders =
      reinterpret_cast<ElfW(Phdr) const*>(programHeadersMemory);
  auto const programHeadersCount = elfHeader->e_phnum;

  for (size_t i = 0; i < programHeadersCount; ++i) {
    if (programHeaders[i].p_type == PT_NOTE) {
      auto* currentNoteMemory = elfHeaderMemory + programHeaders[i].p_offset;
      auto const* noteSectionMemoryEnd =
          currentNoteMemory + programHeaders[i].p_filesz;

      while (currentNoteMemory < noteSectionMemoryEnd) {
        auto const* noteHeader =
            reinterpret_cast<ElfW(Nhdr) const*>(currentNoteMemory);

        if (noteHeader->n_type == NT_GNU_BUILD_ID) {
          auto const* buildIdMemory = reinterpret_cast<const char*>(
              currentNoteMemory + sizeof(ElfW(Nhdr)) + noteHeader->n_namesz);
          return BuildId{buildIdMemory, noteHeader->n_descsz};
        }

        currentNoteMemory += sizeof(ElfW(Nhdr)) +
                             align_up(noteHeader->n_namesz, 4) +
                             align_up(noteHeader->n_descsz, 4);
      }
    }
  }

  return std::nullopt;
}
}  // namespace arangodb::build_id
