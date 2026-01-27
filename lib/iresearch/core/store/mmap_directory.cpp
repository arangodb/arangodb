////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "store/mmap_directory.hpp"

#include "store/store_utils.hpp"
#include "utils/file_utils.hpp"
#include "utils/memory.hpp"
#include "utils/mmap_utils.hpp"

namespace irs {
namespace {

using mmap_utils::mmap_handle;

// Converts the specified IOAdvice to corresponding posix madvice
inline int GetPosixMadvice(IOAdvice advice) {
  switch (advice) {
    case IOAdvice::NORMAL:
    case IOAdvice::DIRECT_READ:
      return IR_MADVICE_NORMAL;
    case IOAdvice::SEQUENTIAL:
      return IR_MADVICE_SEQUENTIAL;
    case IOAdvice::RANDOM:
      return IR_MADVICE_RANDOM;
    case IOAdvice::READONCE:
      return IR_MADVICE_NORMAL;
    case IOAdvice::READONCE_SEQUENTIAL:
      return IR_MADVICE_SEQUENTIAL;
    case IOAdvice::READONCE_RANDOM:
      return IR_MADVICE_RANDOM;
  }

  IRS_LOG_ERROR(
    absl::StrCat("madvice '", static_cast<uint32_t>(advice),
                 "' is not valid (RANDOM|SEQUENTIAL), fallback to NORMAL"));

  return IR_MADVICE_NORMAL;
}

std::shared_ptr<mmap_handle> OpenHandle(const path_char_t* file,
                                        IOAdvice advice,
                                        IResourceManager& rm) noexcept {
  IRS_ASSERT(file);

  std::shared_ptr<mmap_handle> handle;

  try {
    handle = std::make_shared<mmap_handle>(rm);
  } catch (...) {
    return nullptr;
  }

  if (!handle->open(file)) {
    IRS_LOG_ERROR(absl::StrCat("Failed to open mmapped input file, path: ",
                               file_utils::ToStr(file)));
    return nullptr;
  }

  if (0 == handle->size()) {
    return handle;
  }

  const int padvice = GetPosixMadvice(advice);

  if (IR_MADVICE_NORMAL != padvice && !handle->advise(padvice)) {
    IRS_LOG_ERROR(absl::StrCat("Failed to madvise input file, path: ",
                               file_utils::ToStr(file), ", error ", errno));
  }

  handle->dontneed(bool(advice & IOAdvice::READONCE));

  return handle;
}

std::shared_ptr<mmap_handle> OpenHandle(const std::filesystem::path& dir,
                                        std::string_view name, IOAdvice advice,
                                        IResourceManager& rm) noexcept {
  try {
    const auto path = dir / name;

    return OpenHandle(path.c_str(), advice, rm);
  } catch (...) {
  }

  return nullptr;
}

#ifdef __linux__
size_t BytesInCache(uint8_t* addr, size_t length) {
  static const size_t kPageSize = sysconf(_SC_PAGESIZE);
  IRS_ASSERT(reinterpret_cast<uintptr_t>(addr) % kPageSize == 0);
  std::vector<uint8_t> pages(
    std::min(8 * kPageSize, (length + kPageSize - 1) / kPageSize), 0);
  size_t bytes = 0;
  auto count = [&](uint8_t* data, size_t bytes_size, size_t pages_size) {
    mincore(static_cast<void*>(data), bytes_size, pages.data());
    auto it = pages.begin();
    auto end = it + pages_size;
    for (; it != end; ++it) {
      if (*it != 0) {
        bytes += kPageSize;
      }
    }
  };

  const auto available_pages = pages.size();
  const auto available_space = available_pages * kPageSize;

  const auto* end = addr + length;
  while (addr + available_space < end) {
    count(addr, available_space, available_pages);
    addr += available_space;
  }
  if (addr != end) {
    const size_t bytes_size = end - addr;
    count(addr, bytes_size, (bytes_size + kPageSize - 1) / kPageSize);
  }
  return bytes;
}
#endif

// Input stream for memory mapped directory
class MMapIndexInput final : public bytes_view_input {
 public:
  explicit MMapIndexInput(std::shared_ptr<mmap_handle>&& handle) noexcept
    : handle_{std::move(handle)} {
    if (IRS_LIKELY(handle_ && handle_->size())) {
      IRS_ASSERT(handle_->addr() != MAP_FAILED);
      const auto* begin = reinterpret_cast<byte_type*>(handle_->addr());
      bytes_view_input::reset(begin, handle_->size());
    } else {
      handle_.reset();
    }
  }

  uint64_t CountMappedMemory() const final {
#ifdef __linux__
    if (handle_ == nullptr) {
      return 0;
    }
    return BytesInCache(static_cast<uint8_t*>(handle_->addr()),
                        handle_->size());
#else
    return 0;
#endif
  }

  MMapIndexInput(const MMapIndexInput& rhs) noexcept
    : bytes_view_input{rhs}, handle_{rhs.handle_} {}

  ptr dup() const final { return std::make_unique<MMapIndexInput>(*this); }

  ptr reopen() const final { return dup(); }

 private:
  MMapIndexInput& operator=(const MMapIndexInput&) = delete;

  std::shared_ptr<mmap_utils::mmap_handle> handle_;
};

}  // namespace

MMapDirectory::MMapDirectory(std::filesystem::path path,
                             directory_attributes attrs,
                             const ResourceManagementOptions& rm)
  : FSDirectory{std::move(path), std::move(attrs), rm} {}

index_input::ptr MMapDirectory::open(std::string_view name,
                                     IOAdvice advice) const noexcept {
  if (IOAdvice::DIRECT_READ == (advice & IOAdvice::DIRECT_READ)) {
    return FSDirectory::open(name, advice);
  }

  auto handle =
    OpenHandle(directory(), name, advice, *resource_manager_.file_descriptors);

  if (!handle) {
    return nullptr;
  }

  try {
    return std::make_unique<MMapIndexInput>(std::move(handle));
  } catch (...) {
  }

  return nullptr;
}

bool CachingMMapDirectory::length(uint64_t& result,
                                  std::string_view name) const noexcept {
  if (cache_.Visit(name, [&](const auto& cached) noexcept {
        result = cached->size();
        return true;
      })) {
    return true;
  }

  return MMapDirectory::length(result, name);
}

index_input::ptr CachingMMapDirectory::open(std::string_view name,
                                            IOAdvice advice) const noexcept {
  if (bool(advice & (IOAdvice::READONCE | IOAdvice::DIRECT_READ))) {
    return MMapDirectory::open(name, advice);
  }

  auto make_stream = [](auto&& handle) noexcept -> index_input::ptr {
    IRS_ASSERT(handle);

    try {
      return std::make_unique<MMapIndexInput>(std::move(handle));
    } catch (...) {
    }

    return nullptr;
  };

  std::shared_ptr<mmap_utils::mmap_handle> handle;

  if (cache_.Visit(name, [&](const auto& cached) noexcept {
        handle = cached;
        return handle != nullptr;
      })) {
    return make_stream(std::move(handle));
  }

  handle =
    OpenHandle(directory(), name, advice, *resource_manager_.file_descriptors);
  if (handle) {
    cache_.Put(name, [&]() noexcept { return handle; });

    return make_stream(std::move(handle));
  }

  return nullptr;
}

}  // namespace irs
