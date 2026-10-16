// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//

#include <fst/mapped-file.h>

#include <fcntl.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>         // for _get_osfhandle, _open
#include <memoryapi.h>  // for CreateFileMapping, UnmapViewOfFile
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif  // _WIN32

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ios>
#include <limits>
#include <memory>

#include <fst/log.h>

namespace fst {

#ifdef _WIN32
namespace {
static constexpr DWORD DWORD_MAX = std::numeric_limits<DWORD>::max();
}  // namespace
#endif  // _WIN32

MappedFile::MappedFile(const MemoryRegion &region) : region_(region) {}

MappedFile::~MappedFile() {
  if (region_.size != 0) {
    if (region_.mmap) {
      VLOG(2) << "munmap'ed " << region_.size << " bytes at " << region_.mmap;
#ifdef _WIN32
      if (UnmapViewOfFile(region_.mmap) != 0) {
        LOG(ERROR) << "Failed to unmap region: " << GetLastError();
      }
      CloseHandle(region_.file_mapping);
#else
      if (munmap(region_.mmap, region_.size) != 0) {
        LOG(ERROR) << "Failed to unmap region: " << strerror(errno);
      }
#endif
    } else {
      if (region_.data) {
        operator delete(static_cast<char *>(region_.data) - region_.offset);
      }
    }
  }
}

MappedFile *MappedFile::Map(std::istream &istrm, bool memorymap,
                            const std::string &source, size_t size) {
  const auto spos = istrm.tellg();
  VLOG(2) << "memorymap: " << (memorymap ? "true" : "false") << " source: \""
          << source << "\""
          << " size: " << size << " offset: " << spos;
  if (memorymap && spos >= 0 && spos % kArchAlignment == 0) {
    const size_t pos = static_cast<size_t>(spos);
#ifdef _WIN32
    const int fd = _open(source.c_str(), _O_RDONLY);
#else
    const int fd = open(source.c_str(), O_RDONLY);
#endif
    if (fd != -1) {
      std::unique_ptr<MappedFile> mmf(MapFromFileDescriptor(fd, pos, size));
      if (close(fd) == 0 && mmf != nullptr) {
        istrm.seekg(pos + size, std::ios::beg);
        if (istrm) {
          VLOG(2) << "mmap'ed region of " << size << " at offset " << pos
                  << " from " << source << " to addr " << mmf->region_.mmap;
          return mmf.release();
        }
      } else {
        LOG(WARNING) << "Mapping of file failed: " << strerror(errno);
      }
    }
  }

  // If all else fails, reads from the file into the allocated buffer.
  if (memorymap) {
    LOG(WARNING) << "File mapping at offset " << spos << " of file " << source
                 << " could not be honored, reading instead";
  }
  // Reads the file into the buffer in chunks not larger than kMaxReadChunk.
  std::unique_ptr<MappedFile> mf(Allocate(size));
  auto *buffer = static_cast<char *>(mf->mutable_data());
  while (size > 0) {
    const auto next_size = std::min(size, kMaxReadChunk);
    const auto current_pos = istrm.tellg();
    if (!istrm.read(buffer, next_size)) {
      LOG(ERROR) << "Failed to read " << next_size << " bytes at offset "
                 << current_pos << "from \"" << source << "\"";
      return nullptr;
    }
    size -= next_size;
    buffer += next_size;
    VLOG(2) << "Read " << next_size << " bytes. " << size << " remaining";
  }
  return mf.release();
}

MappedFile *MappedFile::MapFromFileDescriptor(int fd, size_t pos, size_t size) {
#ifdef _WIN32
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  const DWORD pagesize = sysInfo.dwPageSize;
#else
  const int pagesize = sysconf(_SC_PAGESIZE);
#endif  // _WIN32

  const size_t offset = pos % pagesize;
  const size_t offset_pos = pos - offset;
  const size_t upsize = size + offset;

#ifdef _WIN32
  if (fd == -1) {
    LOG(ERROR) << "Invalid file descriptor fd=" << fd;
    return nullptr;
  }
  HANDLE file = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (file == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Invalid file descriptor fd=" << fd;
    return nullptr;
  }
  const DWORD max_size_hi =
      sizeof(size_t) > sizeof(DWORD) ? upsize >> (CHAR_BIT * sizeof(DWORD)) : 0;
  const DWORD max_size_lo = upsize & DWORD_MAX;
  HANDLE file_mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY,
                                           max_size_hi, max_size_lo, nullptr);
  if (file_mapping == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Can't create mapping for fd=" << fd << " size=" << upsize
               << ": " << GetLastError();
    return nullptr;
  }

  const DWORD offset_pos_hi =
      sizeof(size_t) > sizeof(DWORD) ? offset_pos >> (CHAR_BIT * sizeof(DWORD))
                                     : 0;
  const DWORD offset_pos_lo = offset_pos & DWORD_MAX;
  void *map = MapViewOfFile(file_mapping, FILE_MAP_READ,
                            offset_pos_hi, offset_pos_lo, upsize);
  if (!map) {
    LOG(ERROR) << "mmap failed for fd=" << fd << " size=" << upsize
               << " offset=" << offset_pos << ": " << GetLastError();
    CloseHandle(file_mapping);
    return nullptr;
  }
#else
  void *map = mmap(nullptr, upsize, PROT_READ, MAP_SHARED, fd, offset_pos);
  if (map == MAP_FAILED) {
    LOG(ERROR) << "mmap failed for fd=" << fd << " size=" << upsize
               << " offset=" << offset_pos;
    return nullptr;
  }
#endif
  MemoryRegion region;
  region.mmap = map;
  region.size = upsize;
  region.data = static_cast<void *>(static_cast<char *>(map) + offset);
  region.offset = offset;
#ifdef _WIN32
  region.file_mapping = file_mapping;
#endif  // _WIN32
  return new MappedFile(region);
}

MappedFile *MappedFile::Allocate(size_t size, size_t align) {
  MemoryRegion region;
  region.data = nullptr;
  region.offset = 0;
  if (size > 0) {
    // TODO(jrosenstock,sorenj): Use std::align() when that is no longer banned.
    // Use std::aligned_alloc() when C++17 is allowed.
    char *buffer = static_cast<char *>(operator new(size + align));
    uintptr_t address = reinterpret_cast<uintptr_t>(buffer);
    region.offset = align - (address % align);
    region.data = buffer + region.offset;
  }
  region.mmap = nullptr;
  region.size = size;
  return new MappedFile(region);
}

MappedFile *MappedFile::Borrow(void *data) {
  MemoryRegion region;
  region.data = data;
  region.mmap = data;
  region.size = 0;
  region.offset = 0;
  return new MappedFile(region);
}

}  // namespace fst
