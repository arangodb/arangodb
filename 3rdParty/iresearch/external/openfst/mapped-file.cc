// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//

#include <fst/mapped-file.h>

#include <fcntl.h>
#ifndef _MSC_VER
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _MSC_VER

#include <algorithm>
#include <cerrno>
#include <ios>
#include <memory>

#include <fst/log.h>

namespace fst {

MappedFile::MappedFile(const MemoryRegion &region) : region_(region) {}

MappedFile::~MappedFile() {
  if (region_.size != 0) {
#ifndef _MSC_VER
    if (region_.mmap) {
      VLOG(2) << "munmap'ed " << region_.size << " bytes at " << region_.mmap;
      if (munmap(region_.mmap, region_.size) != 0) {
        LOG(ERROR) << "Failed to unmap region: " << strerror(errno);
      }
    } else 
#endif // _MSC_VER
    {
      if (region_.data) {
        operator delete(static_cast<char *>(region_.data) - region_.offset);
      }
    }
  }
}

MappedFile *MappedFile::Map(std::istream *istrm, bool memorymap,
                            const std::string &source, size_t size) {
  (void) memorymap;
#ifndef _MSC_VER
  const auto spos = istrm->tellg();
  VLOG(2) << "memorymap: " << (memorymap ? "true" : "false") << " source: \""
          << source << "\""
          << " size: " << size << " offset: " << spos;
  if (memorymap && spos >= 0 && spos % kArchAlignment == 0) {
    const size_t pos = spos;
    const int fd = open(source.c_str(), O_RDONLY);
    if (fd != -1) {
      std::unique_ptr<MappedFile> mmf(MapFromFileDescriptor(fd, pos, size));
      if (close(fd) == 0 && mmf != nullptr) {
        istrm->seekg(pos + size, std::ios::beg);
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
  #endif // _MSC_VER

  // Reads the file into the buffer in chunks not larger than kMaxReadChunk.
  std::unique_ptr<MappedFile> mf(Allocate(size));
  auto *buffer = static_cast<char *>(mf->mutable_data());
  while (size > 0) {
    const auto next_size = std::min(size, kMaxReadChunk);
    const auto current_pos = istrm->tellg();
    if (!istrm->read(buffer, next_size)) {
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
#ifndef _MSC_VER
  const int pagesize = sysconf(_SC_PAGESIZE);
  const off_t offset = pos % pagesize;
  const off_t upsize = size + offset;
  void *map = mmap(nullptr, upsize, PROT_READ, MAP_SHARED, fd, pos - offset);
  if (map == MAP_FAILED) {
    LOG(ERROR) << "mmap failed for fd=" << fd << " size=" << upsize
               << " offset=" << (pos - offset);
    return nullptr;
  }
  MemoryRegion region;
  region.mmap = map;
  region.size = upsize;
  region.data = static_cast<void *>(static_cast<char *>(map) + offset);
  region.offset = offset;
  return new MappedFile(region);
#else
  return nullptr;
#endif // _MSC_VER
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
    region.offset = kArchAlignment - (address % align);
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

constexpr size_t MappedFile::kArchAlignment;

constexpr size_t MappedFile::kMaxReadChunk;

}  // namespace fst
