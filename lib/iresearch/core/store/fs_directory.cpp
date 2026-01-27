////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "fs_directory.hpp"

#include "error/error.hpp"
#include "shared.hpp"
#include "store/directory_attributes.hpp"
#include "store/directory_cleaner.hpp"
#include "utils/crc.hpp"
#include "utils/file_utils.hpp"
#include "utils/log.hpp"
#include "utils/object_pool.hpp"

#include <absl/strings/str_cat.h>

#ifdef _WIN32
#include <Windows.h>  // for GetLastError()
#endif

namespace irs {
namespace {

// Converts the specified IOAdvice to corresponding posix fadvice
inline int get_posix_fadvice(IOAdvice advice) noexcept {
  switch (advice) {
    case IOAdvice::NORMAL:
    case IOAdvice::DIRECT_READ:
      return IR_FADVICE_NORMAL;
    case IOAdvice::SEQUENTIAL:
      return IR_FADVICE_SEQUENTIAL;
    case IOAdvice::RANDOM:
      return IR_FADVICE_RANDOM;
    case IOAdvice::READONCE:
      return IR_FADVICE_DONTNEED;
    case IOAdvice::READONCE_SEQUENTIAL:
      return IR_FADVICE_SEQUENTIAL | IR_FADVICE_NOREUSE;
    case IOAdvice::READONCE_RANDOM:
      return IR_FADVICE_RANDOM | IR_FADVICE_NOREUSE;
  }

  IRS_LOG_ERROR(
    absl::StrCat("fadvice '", static_cast<uint32_t>(advice),
                 "' is not valid (RANDOM|SEQUENTIAL), fallback to NORMAL"));

  return IR_FADVICE_NORMAL;
}

inline file_utils::OpenMode get_read_mode(IOAdvice advice) {
  if (IOAdvice::DIRECT_READ == (advice & IOAdvice::DIRECT_READ)) {
    return file_utils::OpenMode::Read | file_utils::OpenMode::Direct;
  }
  return file_utils::OpenMode::Read;
}

}  // namespace

MSVC_ONLY(__pragma(warning(push)))
// the compiler encountered a deprecated declaration
MSVC_ONLY(__pragma(warning(disable : 4996)))

class fs_lock : public index_lock {
 public:
  fs_lock(const std::filesystem::path& dir, std::string_view file)
    : dir_{dir}, file_{file} {}

  bool lock() final {
    if (handle_) {
      // don't allow self obtaining
      return false;
    }

    bool exists;

    if (!file_utils::exists(exists, dir_.c_str())) {
      IRS_LOG_ERROR("Error caught");
      return false;
    }

    // create directory if it is not exists
    if (!exists && !file_utils::mkdir(dir_.c_str(), true)) {
      IRS_LOG_ERROR("Error caught");
      return false;
    }

    const auto path = dir_ / file_;

    // create lock file
    if (!file_utils::verify_lock_file(path.c_str())) {
      if (!file_utils::exists(exists, path.c_str()) ||
          (exists && !file_utils::remove(path.c_str()))) {
        IRS_LOG_ERROR("Error caught");
        return false;
      }

      handle_ = file_utils::create_lock_file(path.c_str());
    }

    return handle_ != nullptr;
  }

  bool is_locked(bool& result) const noexcept final {
    if (handle_ != nullptr) {
      result = true;
      return true;
    }

    try {
      const auto path = dir_ / file_;

      result = file_utils::verify_lock_file(path.c_str());
      return true;
    } catch (...) {
    }

    return false;
  }

  bool unlock() noexcept final {
    if (handle_ != nullptr) {
      handle_ = nullptr;
#ifdef _WIN32
      // file will be automatically removed on close
      return true;
#else
      const auto path = dir_ / file_;

      return file_utils::remove(path.c_str());
#endif
    }

    return false;
  }

 private:
  std::filesystem::path dir_;
  std::string file_;
  file_utils::lock_handle_t handle_;
};

class fs_index_output : public buffered_index_output {
 public:
  DEFINE_FACTORY_INLINE(index_output);  // cppcheck-suppress unknownMacro

  static index_output::ptr open(const path_char_t* name,
                                const ResourceManagementOptions& rm) noexcept
    try {
    IRS_ASSERT(name);
    size_t descriptors{1};
    rm.file_descriptors->Increase(descriptors);
    irs::Finally cleanup = [&]() noexcept {
      rm.file_descriptors->DecreaseChecked(descriptors);
    };
    file_utils::handle_t handle(
      file_utils::open(name, file_utils::OpenMode::Write, IR_FADVICE_NORMAL));

    if (nullptr == handle) {
      IRS_LOG_ERROR(
        absl::StrCat("Failed to open output file, error: ", GET_ERROR(),
                     ", path: ", file_utils::ToStr(name)));

      return nullptr;
    }

    auto res = fs_index_output::make<fs_index_output>(std::move(handle), rm);
    descriptors = 0;
    return res;
  } catch (...) {
    return nullptr;
  }

  int64_t checksum() const final {
    const_cast<fs_index_output*>(this)->flush();
    return crc_.checksum();
  }

 protected:
  size_t CloseImpl() final {
    const auto size = buffered_index_output::CloseImpl();
    IRS_ASSERT(handle_);
    handle_.reset(nullptr);
    rm_.file_descriptors->Decrease(1);
    return size;
  }

  void flush_buffer(const byte_type* b, size_t len) final {
    IRS_ASSERT(handle_);

    const auto len_written =
      irs::file_utils::fwrite(handle_.get(), b, sizeof(byte_type) * len);
    crc_.process_bytes(b, len_written);

    if (len && len_written != len) {
      throw io_error{absl::StrCat("Failed to write buffer, written '",
                                  len_written, "' out of '", len, "' bytes")};
    }
  }

 private:
  fs_index_output(file_utils::handle_t&& handle,
                  const ResourceManagementOptions& rm)
    : handle_(std::move(handle)), rm_{rm} {
    IRS_ASSERT(handle_);
    rm_.transactions->Increase(sizeof(fs_index_output));
    buffered_index_output::reset(buf_, sizeof buf_);
  }

  ~fs_index_output() override {
    rm_.transactions->Decrease(sizeof(fs_index_output));
  }

  byte_type buf_[1024];
  file_utils::handle_t handle_;
  const ResourceManagementOptions& rm_;
  crc32c crc_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class fs_index_input
//////////////////////////////////////////////////////////////////////////////
class pooled_fs_index_input;  // predeclaration used by fs_index_input
class fs_index_input : public buffered_index_input {
 public:
  uint64_t CountMappedMemory() const final { return 0; }

  using buffered_index_input::read_internal;

  int64_t checksum(size_t offset) const final {
    // "read_internal" modifies pos_
    Finally restore_position = [pos = this->pos_, this]() noexcept {
      const_cast<fs_index_input*>(this)->pos_ = pos;
    };

    const auto begin = pos_;
    const auto end = (std::min)(begin + offset, handle_->size);

    crc32c crc;
    byte_type buf[sizeof buf_];

    for (auto pos = begin; pos < end;) {
      const auto to_read = (std::min)(end - pos, sizeof buf);
      pos += const_cast<fs_index_input*>(this)->read_internal(buf, to_read);
      crc.process_bytes(buf, to_read);
    }

    return crc.checksum();
  }

  ptr dup() const override { return ptr(new fs_index_input(*this)); }

  static index_input::ptr open(const path_char_t* name, size_t pool_size,
                               IOAdvice advice,
                               const ResourceManagementOptions& rm) noexcept
    try {
    IRS_ASSERT(name);

    size_t descriptors{1};
    rm.file_descriptors->Increase(descriptors);
    irs::Finally cleanup = [&]() noexcept {
      rm.file_descriptors->DecreaseChecked(descriptors);
    };

    auto handle = file_handle::make(rm);
    handle->io_advice = advice;
    handle->handle =
      irs::file_utils::open(name, get_read_mode(handle->io_advice),
                            get_posix_fadvice(handle->io_advice));

    if (nullptr == handle->handle) {
      IRS_LOG_ERROR(
        absl::StrCat("Failed to open input file, error: ", GET_ERROR(),
                     ", path: ", file_utils::ToStr(name)));

      return nullptr;
    }
    uint64_t size;
    if (!file_utils::byte_size(size, *handle)) {
      IRS_LOG_ERROR(
        absl::StrCat("Failed to get stat for input file, error: ", GET_ERROR(),
                     ", path: ", file_utils::ToStr(name)));

      return nullptr;
    }

    handle->size = size;

    auto res = ptr(new fs_index_input(std::move(handle), pool_size));
    descriptors = 0;
    return res;
  } catch (...) {
    return nullptr;
  }

  size_t length() const final { return handle_->size; }

  ptr reopen() const override;

 protected:
  void seek_internal(size_t pos) final {
    if (pos > handle_->size) {
      throw io_error{absl::StrCat("seek out of range for input file, length '",
                                  handle_->size, "', position '", pos, "'")};
    }

    pos_ = pos;
  }

  size_t read_internal(byte_type* b, size_t len) final {
    IRS_ASSERT(b);
    IRS_ASSERT(handle_->handle);

    void* fd = *handle_;

    if (handle_->pos != pos_) {
      if (irs::file_utils::fseek(fd, static_cast<long>(pos_), SEEK_SET) != 0) {
        throw io_error{absl::StrCat("failed to seek to '", pos_,
                                    "' for input file, error '",
                                    irs::file_utils::ferror(fd), "'")};
      }
      handle_->pos = pos_;
    }

    const size_t read = irs::file_utils::fread(fd, b, sizeof(byte_type) * len);
    pos_ = handle_->pos += read;

    IRS_ASSERT(handle_->pos == pos_);
    return read;
  }

 private:
  friend pooled_fs_index_input;

  /* use shared wrapper here since we don't want to
   * call "ftell" every time we need to know current
   * position */
  struct file_handle {
    using ptr = std::shared_ptr<file_handle>;

    static ptr make(const ResourceManagementOptions& rm) {
      return std::make_shared<file_handle>(rm);
    }

    file_handle(const ResourceManagementOptions& rm) : resource_manager{rm} {}

    ~file_handle() {
      const bool release = handle.get() != nullptr;
      handle.reset();
      resource_manager.file_descriptors->DecreaseChecked(release);
    }
    operator void*() const { return handle.get(); }

    file_utils::handle_t handle; /* native file handle */
    size_t size{};               /* file size */
    size_t pos{};                /* current file position*/
    IOAdvice io_advice{IOAdvice::NORMAL};
    const ResourceManagementOptions& resource_manager;
  };

  fs_index_input(file_handle::ptr&& handle, size_t pool_size);

  fs_index_input(const fs_index_input& rhs);

  ~fs_index_input() override;

  fs_index_input& operator=(const fs_index_input&) = delete;

  byte_type buf_[1024];
  file_handle::ptr handle_;  // shared file handle
  size_t pool_size_;  // size of pool for instances of pooled_fs_index_input
  size_t pos_;        // current input stream position
};

class pooled_fs_index_input final : public fs_index_input {
 public:
  explicit pooled_fs_index_input(const fs_index_input& in);
  ~pooled_fs_index_input() noexcept final;
  index_input::ptr dup() const final {
    return ptr(new pooled_fs_index_input(*this));
  }
  index_input::ptr reopen() const final;

 private:
  struct builder {
    using ptr = std::unique_ptr<file_handle>;

    static std::unique_ptr<file_handle> make(
      const ResourceManagementOptions& rm) {
      return std::make_unique<file_handle>(rm);
    }
  };

  using fd_pool_t = unbounded_object_pool<builder>;
  std::shared_ptr<fd_pool_t> fd_pool_;

  pooled_fs_index_input(const pooled_fs_index_input& in) = default;
  file_handle::ptr reopen(const file_handle& src) const;
};

fs_index_input::fs_index_input(file_handle::ptr&& handle, size_t pool_size)
  : handle_(std::move(handle)), pool_size_(pool_size), pos_(0) {
  IRS_ASSERT(handle_);
  handle_->resource_manager.readers->Increase(sizeof(pooled_fs_index_input));
  buffered_index_input::reset(buf_, sizeof buf_, 0);
}

fs_index_input::fs_index_input(const fs_index_input& rhs)
  : handle_(rhs.handle_), pool_size_(rhs.pool_size_), pos_(rhs.file_pointer()) {
  IRS_ASSERT(handle_);
  handle_->resource_manager.readers->Increase(sizeof(pooled_fs_index_input));
  buffered_index_input::reset(buf_, sizeof buf_, pos_);
}

fs_index_input::~fs_index_input() {
  if (handle_) {
    auto* r = handle_->resource_manager.readers;
    handle_.reset();
    r->Decrease(sizeof(pooled_fs_index_input));
  }
}

index_input::ptr fs_index_input::reopen() const {
  return std::make_unique<pooled_fs_index_input>(*this);
}

pooled_fs_index_input::pooled_fs_index_input(const fs_index_input& in)
  : fs_index_input(in), fd_pool_(std::make_shared<fd_pool_t>(pool_size_)) {
  handle_ = reopen(*handle_);
}

pooled_fs_index_input::~pooled_fs_index_input() noexcept {
  IRS_ASSERT(handle_);
  auto* r = handle_->resource_manager.readers;
  handle_.reset();  // release handle before the fs_pool_ is deallocated
  r->Decrease(sizeof(pooled_fs_index_input));
}

index_input::ptr pooled_fs_index_input::reopen() const {
  auto ptr = dup();
  IRS_ASSERT(ptr);

  auto& in = static_cast<pooled_fs_index_input&>(*ptr);
  in.handle_ = reopen(*handle_);  // reserve a new handle from pool
  IRS_ASSERT(in.handle_ && in.handle_->handle);

  return ptr;
}

fs_index_input::file_handle::ptr pooled_fs_index_input::reopen(
  const file_handle& src) const {
  // reserve a new handle from the pool
  std::shared_ptr<fs_index_input::file_handle> handle{
    const_cast<pooled_fs_index_input*>(this)->fd_pool_->emplace(
      src.resource_manager)};
  size_t descriptors{0};
  irs::Finally cleanup = [&]() noexcept {
    src.resource_manager.file_descriptors->DecreaseChecked(descriptors);
  };
  if (!handle->handle) {
    src.resource_manager.file_descriptors->Increase(1);
    descriptors = 1;
    handle->handle = irs::file_utils::open(
      src, get_read_mode(src.io_advice),
      get_posix_fadvice(
        src.io_advice));  // same permission as in fs_index_input::open(...)

    if (!handle->handle) {
      // even win32 uses 'errno' for error codes in calls to file_open(...)
      throw io_error{
        absl::StrCat("Failed to reopen input file, error: ", GET_ERROR())};
    }
    descriptors = 0;
    handle->io_advice = src.io_advice;
  }

  const auto pos = irs::file_utils::ftell(
    handle->handle.get());  // match position of file descriptor

  if (pos < 0) {
    throw io_error{absl::StrCat(
      "Failed to obtain current position of input file, error: ", GET_ERROR())};
  }

  handle->pos = pos;
  handle->size = src.size;
  return handle;
}

FSDirectory::FSDirectory(std::filesystem::path dir, directory_attributes attrs,
                         const ResourceManagementOptions& rm,
                         size_t fd_pool_size)
  : resource_manager_{rm},
    attrs_{std::move(attrs)},
    dir_{std::move(dir)},
    fd_pool_size_{fd_pool_size} {}

index_output::ptr FSDirectory::create(std::string_view name) noexcept {
  try {
    const auto path = dir_ / name;

    auto out = fs_index_output::open(path.c_str(), resource_manager_);

    if (!out) {
      IRS_LOG_ERROR(absl::StrCat("Failed to open output file, path: ", name));
    }

    return out;
  } catch (...) {
  }

  return nullptr;
}

const std::filesystem::path& FSDirectory::directory() const noexcept {
  return dir_;
}

bool FSDirectory::exists(bool& result, std::string_view name) const noexcept {
  const auto path = dir_ / name;

  return file_utils::exists(result, path.c_str());
}

bool FSDirectory::length(uint64_t& result,
                         std::string_view name) const noexcept {
  const auto path = dir_ / name;

  return file_utils::byte_size(result, path.c_str());
}

index_lock::ptr FSDirectory::make_lock(std::string_view name) noexcept {
  return index_lock::make<fs_lock>(dir_, name);
}

bool FSDirectory::mtime(std::time_t& result,
                        std::string_view name) const noexcept {
  const auto path = dir_ / name;

  return file_utils::mtime(result, path.c_str());
}

bool FSDirectory::remove(std::string_view name) noexcept {
  try {
    const auto path = dir_ / name;

    return file_utils::remove(path.c_str());
  } catch (...) {
  }

  return false;
}

index_input::ptr FSDirectory::open(std::string_view name,
                                   IOAdvice advice) const noexcept {
  try {
    const auto path = dir_ / name;

    return fs_index_input::open(path.c_str(), fd_pool_size_, advice,
                                resource_manager_);
  } catch (...) {
  }

  return nullptr;
}

bool FSDirectory::rename(std::string_view src, std::string_view dst) noexcept {
  try {
    const auto src_path = dir_ / src;
    const auto dst_path = dir_ / dst;

    return file_utils::move(src_path.c_str(), dst_path.c_str());
  } catch (...) {
  }

  return false;
}

bool FSDirectory::visit(const directory::visitor_f& visitor) const {
  bool exists;

  if (!file_utils::exists_directory(exists, dir_.c_str()) || !exists) {
    return false;
  }

#ifdef _WIN32
  std::filesystem::path path;
  auto dir_visitor = [&path, &visitor](const path_char_t* name) {
    path = name;

    auto filename = path.string();
    return visitor(filename);
  };
#else
  std::string filename;
  auto dir_visitor = [&filename, &visitor](const path_char_t* name) {
    filename.assign(name);
    return visitor(filename);
  };
#endif

  return file_utils::visit_directory(dir_.c_str(), dir_visitor, false);
}

bool FSDirectory::sync(std::string_view name) noexcept {
  try {
    const auto path = dir_ / name;

    if (file_utils::file_sync(path.c_str())) {
      return true;
    }

    IRS_LOG_ERROR(absl::StrCat("Failed to sync file, error: ", GET_ERROR(),
                               ", path: ", path.string()));
  } catch (...) {
    IRS_LOG_ERROR(absl::StrCat("Failed to sync file, name: ", name));
  }

  return false;
}

bool FSDirectory::sync(std::span<const std::string_view> files) noexcept {
  return std::all_of(std::begin(files), std::end(files),
                     [this](std::string_view name) mutable noexcept {
                       return this->sync(name);
                     });
}

MSVC_ONLY(__pragma(warning(pop)))

bool CachingFSDirectory::length(uint64_t& result,
                                std::string_view name) const noexcept {
  if (cache_.Visit(name, [&](const auto length) noexcept {
        result = length;
        return true;
      })) {
    return true;
  }

  return FSDirectory::length(result, name);
}

index_input::ptr CachingFSDirectory::open(std::string_view name,
                                          IOAdvice advice) const noexcept {
  auto stream = FSDirectory::open(name, advice);

  if ((IOAdvice::READONCE != (advice & IOAdvice::READONCE)) && stream) {
    cache_.Put(name, [&]() noexcept { return stream->length(); });
  }

  return stream;
}

}  // namespace irs
