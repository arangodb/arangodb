
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// Author: sorenj@google.com (Jeffrey Sorensen)

#include <fst/mapped-file.h>

#include <errno.h>
#include <fcntl.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <algorithm>

// This limit was put in place as a workaround for b/7948248, in which Placer
// allocates at least one extra buffer for each read, and sometimes more than
// one. For very large reads (~2GB), the memory spike was causing OOM failures
// on startup.
static const size_t kMaxReadChunk = 256 * 1024 * 1024;  // 256 MB

namespace fst {

// Alignment required for mapping structures (in bytes.)  Regions of memory
// that are not aligned upon a 128 bit boundary will be read from the file
// instead.  This is consistent with the alignment boundary set in the
// const and compact fst code.
const int MappedFile::kArchAlignment = 16;

MappedFile::MappedFile(const MemoryRegion &region) : region_(region) { }

MappedFile::~MappedFile() {
  if (region_.size != 0) {
    if (region_.mmap != NULL) {
#ifndef _MSC_VER
      VLOG(1) << "munmap'ed " << region_.size << " bytes at " << region_.mmap;
      if (munmap(region_.mmap, region_.size) != 0) {
        LOG(ERROR) << "failed to unmap region: "<< strerror(errno);
      }
#endif
    } else {
      if (region_.data != 0) {
        operator delete(static_cast<char*>(region_.data) - region_.offset);
      }
    }
  }
}

MappedFile* MappedFile::Allocate(size_t size, int align) {
  MemoryRegion region;
  region.data = 0;
  region.offset = 0;
  if (size > 0) {
    char *buffer = static_cast<char*>(operator new(size + align));
    size_t address = reinterpret_cast<size_t>(buffer);
    region.offset = kArchAlignment - (address % align);
    region.data = buffer + region.offset;
  }
  region.mmap = 0;
  region.size = size;
  return new MappedFile(region);
}

MappedFile* MappedFile::Borrow(void *data) {
  MemoryRegion region;
  region.data = data;
  region.mmap = data;
  region.size = 0;
  region.offset = 0;
  return new MappedFile(region);
}

MappedFile* MappedFile::Map(istream* s, bool memorymap,
                            const string& source, size_t size) {
  std::streampos spos = s->tellg();
#ifndef _MSC_VER
  VLOG(1) << "memorymap: " << (memorymap ? "true" : "false")
          << " source: \"" << source << "\""
          << " size: " << size << " offset: " << spos;
  if (memorymap && spos >= 0 && spos % kArchAlignment == 0) {
    size_t pos = spos;
    int fd = open(source.c_str(), O_RDONLY);
    if (fd != -1) {
      int pagesize = sysconf(_SC_PAGESIZE);
      off_t offset = pos % pagesize;
      off_t upsize = size + offset;
      void *map = mmap(0, upsize, PROT_READ, MAP_SHARED, fd, pos - offset);
      char *data = reinterpret_cast<char*>(map);
      if (close(fd) == 0 && map != MAP_FAILED) {
        MemoryRegion region;
        region.mmap = map;
        region.size = upsize;
        region.data = reinterpret_cast<void*>(data + offset);
        region.offset = offset;
        MappedFile *mmf = new MappedFile(region);
        s->seekg(pos + size, ios::beg);
        if (s) {
          VLOG(1) << "mmap'ed region of " << size << " at offset " << pos
                  << " from " << source << " to addr " << map;
          return mmf;
        }
        delete mmf;
      } else {
        LOG(INFO) << "Mapping of file failed: " << strerror(errno);
      }
    }
  }
#endif
  // If all else fails resort to reading from file into allocated buffer.
  if (memorymap) {
    LOG(WARNING) << "File mapping at offset " << spos << " of file "
                 << source << " could not be honored, reading instead.";
  }

  // Read the file into the buffer in chunks not larger than kMaxReadChunk.
  MappedFile* mf = Allocate(size);
  char* buffer = reinterpret_cast<char*>(mf->mutable_data());
  while (size > 0) {
    const size_t next_size = std::min(size, kMaxReadChunk);
    std::streampos current_pos = s->tellg();
    if (!s->read(buffer, next_size)) {
      LOG(ERROR) << "Failed to read " << next_size << " bytes at offset "
                 << current_pos << "from \"" << source << "\".";
      delete mf;
      return NULL;
    }
    size -= next_size;
    buffer += next_size;
    VLOG(2) << "Read " << next_size << " bytes. " << size << " remaining.";
  }
  return mf;
}

}  // namespace fst
