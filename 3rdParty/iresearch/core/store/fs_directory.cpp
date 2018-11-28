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

#include "shared.hpp"
#include "directory_attributes.hpp"
#include "fs_directory.hpp"
#include "error/error.hpp"
#include "utils/log.hpp"
#include "utils/object_pool.hpp"
#include "utils/string_utils.hpp"
#include "utils/utf8_path.hpp"
#include "utils/file_utils.hpp"
#include "utils/crc.hpp"

#ifdef _WIN32
  #include <Windows.h> // for GetLastError()
#endif

#include <boost/locale/encoding.hpp>

NS_LOCAL

inline size_t buffer_size(FILE* file) NOEXCEPT {
  UNUSED(file);
  return 1024;
//  auto block_size = irs::file_utils::block_size(file_no(file));
//
//  if (block_size < 0) {
//    // fallback to default value
//    block_size = 1024;
//  }
//
//  return block_size;
}

#if !defined(__APPLE__)

//////////////////////////////////////////////////////////////////////////////
/// @brief converts the specified IOAdvice to corresponding posix fadvice
//////////////////////////////////////////////////////////////////////////////
inline int get_posix_fadvice(irs::IOAdvice advice) {
  switch (advice) {
    case irs::IOAdvice::NORMAL:
      return IR_FADVICE_NORMAL;
    case irs::IOAdvice::SEQUENTIAL:
      return IR_FADVICE_SEQUENTIAL;
    case irs::IOAdvice::RANDOM:
      return IR_FADVICE_RANDOM;
    case irs::IOAdvice::READONCE:
      return IR_FADVICE_DONTNEED;
    case irs::IOAdvice::READONCE_SEQUENTIAL:
      return IR_FADVICE_SEQUENTIAL | IR_FADVICE_NOREUSE;
    case irs::IOAdvice::READONCE_RANDOM:
      return IR_FADVICE_RANDOM | IR_FADVICE_NOREUSE;
  }

  IR_FRMT_ERROR(
    "fadvice '%d' is not valid (RANDOM|SEQUENTIAL), fallback to NORMAL",
    uint32_t(advice)
  );

  return IR_FADVICE_NORMAL;
}

#endif

NS_END

NS_ROOT
MSVC_ONLY(__pragma(warning(push)))
MSVC_ONLY(__pragma(warning(disable: 4996))) // the compiler encountered a deprecated declaration

//////////////////////////////////////////////////////////////////////////////
/// @class fs_lock
//////////////////////////////////////////////////////////////////////////////

class fs_lock : public index_lock {
 public:
  fs_lock(const std::string& dir, const std::string& file)
    : dir_(dir), file_(file) {
  }

  virtual bool lock() override {
    if (handle_) {
      // don't allow self obtaining
      return false;
    }

    bool exists;
    utf8_path path;

    if (!(path/=dir_).exists(exists)) {
      IR_FRMT_ERROR("Error caught in: %s", __FUNCTION__);
      return false;
    }

    // create directory if it is not exists
    if (!exists && !path.mkdir()) {
      IR_FRMT_ERROR("Error caught in: %s", __FUNCTION__);
      return false;
    }

    // create lock file
    if (!file_utils::verify_lock_file((path/=file_).c_str())) {
      if (!path.exists(exists) || (exists && !path.remove())) {
        IR_FRMT_ERROR("Error caught in: %s", __FUNCTION__);
        return false;
      }

      handle_ = file_utils::create_lock_file(path.c_str());
    }

    return handle_ != nullptr;
  }

  virtual bool is_locked(bool& result) const NOEXCEPT override {
    if (handle_ != nullptr) {
      result = true;
      return true;
    }

    try {
      utf8_path path;

      (path/=dir_)/=file_;
      result = file_utils::verify_lock_file(path.c_str());

      return true;
    } catch (...) {
      IR_LOG_EXCEPTION();
    }

    return false;
  }

  virtual bool unlock() NOEXCEPT override {
    if (handle_ != nullptr) {
      handle_ = nullptr;
#ifdef _WIN32
      // file will be automatically removed on close
      return true;
#else
      return ((utf8_path()/=dir_)/=file_).remove();
#endif
    }

    return false;
  }

 private:
  std::string dir_;
  std::string file_;
  file_utils::lock_handle_t handle_;
}; // fs_lock

//////////////////////////////////////////////////////////////////////////////
/// @class fs_index_output
//////////////////////////////////////////////////////////////////////////////
class fs_index_output : public buffered_index_output {
 public:
  static index_output::ptr open(const file_path_t name) NOEXCEPT {
    assert(name);

    file_utils::handle_t handle(file_open(name, "wb"));

    if (nullptr == handle) {
      auto path = boost::locale::conv::utf_to_utf<char>(name);

      // even win32 uses 'errno' fo error codes in calls to file_open(...)
      IR_FRMT_ERROR("Failed to open output file, error: %d, path: %s", errno, path.c_str());

      return nullptr;
    }

    const auto buf_size = buffer_size(handle.get());

    try {
      return fs_index_output::make<fs_index_output>(
        std::move(handle),
        buf_size
      );
    } catch(...) {
      IR_LOG_EXCEPTION();
    }

    return nullptr;
  }

  virtual void close() override {
    buffered_index_output::close();
    handle.reset(nullptr);
  }

  virtual int64_t checksum() const override {
    const_cast<fs_index_output*>(this)->flush();
    return crc.checksum();
  }

 protected:
  virtual void flush_buffer(const byte_type* b, size_t len) override {
    assert(handle);

    const auto len_written = fwrite(b, sizeof(byte_type), len, handle.get());
    crc.process_bytes(b, len_written);

    if (len && len_written != len) {
      throw io_error(string_utils::to_string(
        "failed to write buffer, written '" IR_SIZE_T_SPECIFIER "' out of '" IR_SIZE_T_SPECIFIER "' bytes",
        len_written, len
      ));
    }
  }

 private:
  DEFINE_FACTORY_INLINE(index_output);

  fs_index_output(file_utils::handle_t&& handle, size_t buf_size) NOEXCEPT
    : buffered_index_output(buf_size),
      handle(std::move(handle)) {
  }

  file_utils::handle_t handle;
  crc32c crc;
}; // fs_index_output

//////////////////////////////////////////////////////////////////////////////
/// @class fs_index_input
//////////////////////////////////////////////////////////////////////////////
class pooled_fs_index_input; // predeclaration used by fs_index_input
class fs_index_input : public buffered_index_input {
 public:
  virtual int64_t checksum(size_t offset) const override final {
    const auto begin = handle_->pos;
    const auto end = (std::min)(begin + offset, handle_->size);

    crc32c crc;
    byte_type buf[1024];

    for (auto pos = begin; pos < end; ) {
      const auto to_read = (std::min)(end - pos, sizeof buf);
      pos += const_cast<fs_index_input*>(this)->read_internal(buf, to_read);
      crc.process_bytes(buf, to_read);
    }

    return crc.checksum();
  }

  virtual ptr dup() const override {
    return index_input::make<fs_index_input>(*this);
  }

  static index_input::ptr open(
    const file_path_t name, size_t pool_size, IOAdvice /*advice*/
  ) NOEXCEPT {
    // FIXME honor IOAdvice
    // FIXME On Windows use FILE_FLAG_SEQUENTIAL_SCAN in CreateFile

    assert(name);

    auto handle = file_handle::make();

    handle->handle = file_open(name, "rb");

    if (nullptr == handle->handle) {
      auto path = boost::locale::conv::utf_to_utf<char>(name);

      // even win32 uses 'errno' for error codes in calls to file_open(...)
      IR_FRMT_ERROR("Failed to open input file, error: %d, path: %s", errno, path.c_str());

      return nullptr;
    }

    // convert file descriptor to POSIX
    uint64_t size;

    if (!file_utils::byte_size(size, file_no(*handle))) {
      auto path = boost::locale::conv::utf_to_utf<char>(name);
      #ifdef _WIN32
        auto error = GetLastError();
      #else
        auto error = errno;
      #endif

      IR_FRMT_ERROR("Failed to get stat for input file, error: %d, path: %s", error, path.c_str());

      return nullptr;
    }

    handle->size = size;

    const auto buf_size = ::buffer_size(handle->handle.get());

    try {
      return fs_index_input::make<fs_index_input>(
        std::move(handle),
        buf_size,
        pool_size
      );
    } catch(...) {
      IR_LOG_EXCEPTION();
    }

    return nullptr;
  }

  virtual size_t length() const override {
    return handle_->size;
  }

  virtual ptr reopen() const override;

 protected:
  virtual void seek_internal(size_t pos) override {
    if (pos >= handle_->size) {
      throw io_error(string_utils::to_string(
        "seek out of range for input file, length '" IR_SIZE_T_SPECIFIER "', position '" IR_SIZE_T_SPECIFIER "'",
        handle_->size, pos
      ));
    }

    pos_ = pos;
  }

  virtual size_t read_internal(byte_type* b, size_t len) override {
    assert(b);
    assert(handle_->handle);

    FILE* stream = *handle_;

    if (handle_->pos != pos_) {
      if (fseek(stream, static_cast<long>(pos_), SEEK_SET) != 0) {
        throw io_error(string_utils::to_string(
          "failed to seek to '" IR_SIZE_T_SPECIFIER "' for input file, error '%d'",
          pos_, ferror(stream)
        ));
      }

      handle_->pos = pos_;
    }

    const size_t read = fread(b, sizeof(byte_type), len, stream);
    pos_ = handle_->pos += read;

    if (read != len) {
      if (feof(stream)) {
        //eof(true);
        // read past eof
        throw eof_error();
      }

      // read error
      throw io_error(string_utils::to_string(
        "failed to read from input file, read '" IR_SIZE_T_SPECIFIER "' out of '" IR_SIZE_T_SPECIFIER "' bytes, error '%d'",
        read, len, ferror(stream)
      ));
    }

    assert(handle_->pos == pos_);
    return read;
  }

 private:
  friend pooled_fs_index_input;

  /* use shared wrapper here since we don't want to
  * call "ftell" every time we need to know current 
  * position */
  struct file_handle {
    DECLARE_SHARED_PTR(file_handle);
    DECLARE_FACTORY();

    operator FILE*() const { return handle.get(); }

    file_utils::handle_t handle; /* native file handle */
    size_t size{}; /* file size */
    size_t pos{}; /* current file position*/
  }; // file_handle

  DEFINE_FACTORY_INLINE(index_input);

  fs_index_input(
      file_handle::ptr&& handle,
      size_t buffer_size,
      size_t pool_size) NOEXCEPT
    : buffered_index_input(buffer_size),
      handle_(std::move(handle)),
      pool_size_(pool_size),
      pos_(0) {
    assert(handle_);
  }

  fs_index_input(const fs_index_input&) = default;
  fs_index_input& operator=(const fs_index_input&) = delete;

  file_handle::ptr handle_; // shared file handle
  size_t pool_size_; // size of pool for instances of pooled_fs_index_input
  size_t pos_; // current input stream position
}; // fs_index_input

DEFINE_FACTORY_DEFAULT(fs_index_input::file_handle);

class pooled_fs_index_input final : public fs_index_input {
 public:
  DECLARE_UNIQUE_PTR(pooled_fs_index_input); // allow private construction

  explicit pooled_fs_index_input(const fs_index_input& in);
  virtual ~pooled_fs_index_input() NOEXCEPT;
  virtual index_input::ptr dup() const override {
    return index_input::make<pooled_fs_index_input>(*this);
  }
  virtual index_input::ptr reopen() const override;

 private:
  typedef unbounded_object_pool<file_handle> fd_pool_t;
  std::shared_ptr<fd_pool_t> fd_pool_;

  pooled_fs_index_input(const pooled_fs_index_input& in) = default;
  file_handle::ptr reopen(const file_handle& src) const;
}; // pooled_fs_index_input

index_input::ptr fs_index_input::reopen() const {
  return index_input::make<pooled_fs_index_input>(*this);
}

pooled_fs_index_input::pooled_fs_index_input(const fs_index_input& in)
  : fs_index_input(in),
    fd_pool_(memory::make_shared<fd_pool_t>(pool_size_)) {
  handle_ = reopen(*handle_);
}

pooled_fs_index_input::~pooled_fs_index_input() NOEXCEPT {
  handle_.reset(); // release handle before the fs_pool_ is deallocated
}

index_input::ptr pooled_fs_index_input::reopen() const {
  auto ptr = dup();
  assert(ptr);

  auto& in = static_cast<pooled_fs_index_input&>(*ptr);
  in.handle_ = reopen(*handle_); // reserve a new handle from pool
  assert(in.handle_ && in.handle_->handle);

  return ptr;
}

fs_index_input::file_handle::ptr pooled_fs_index_input::reopen(
  const file_handle& src
) const {
  // reserve a new handle from the pool
  auto handle = const_cast<pooled_fs_index_input*>(this)->fd_pool_->emplace().release();

  if (!handle->handle) {
    handle->handle = file_open(src, "rb"); // same permission as in fs_index_input::open(...)

    if (!handle->handle) {
      // even win32 uses 'errno' for error codes in calls to file_open(...)
      throw io_error(string_utils::to_string(
        "Failed to reopen input file, error: %d", errno
      ));
    }
  }

  const auto pos = ::ftell(handle->handle.get()); // match position of file descriptor

  if (pos < 0) {
    throw io_error(string_utils::to_string(
      "Failed to obtain currnt position of input file, error: %d", errno
    ));
  }

  handle->pos = pos;
  handle->size = src.size;

  return handle;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       fs_directory implementation
// -----------------------------------------------------------------------------

fs_directory::fs_directory(const std::string& dir)
  : dir_(dir) {
}

attribute_store& fs_directory::attributes() NOEXCEPT {
  return attributes_;
}

index_output::ptr fs_directory::create(const std::string& name) NOEXCEPT {
  try {
    utf8_path path;

    (path/=dir_)/=name;

    auto out = fs_index_output::open(path.c_str());

    if (!out) {
      IR_FRMT_ERROR("Failed to open output file, path: %s", name.c_str());
    }

    return out;
  } catch(...) {
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

const std::string& fs_directory::directory() const NOEXCEPT {
  return dir_;
}

bool fs_directory::exists(
  bool& result, const std::string& name
) const NOEXCEPT {
  return ((utf8_path()/=dir_)/=name).exists(result);
}

bool fs_directory::length(
  uint64_t& result, const std::string& name
) const NOEXCEPT {
  return ((utf8_path()/=dir_)/=name).file_size(result);
}

index_lock::ptr fs_directory::make_lock(const std::string& name) NOEXCEPT {
  return index_lock::make<fs_lock>(dir_, name);
}

bool fs_directory::mtime(
  std::time_t& result, const std::string& name
) const NOEXCEPT {
  return ((utf8_path()/=dir_)/=name).mtime(result);
}

bool fs_directory::remove(const std::string& name) NOEXCEPT {
  try {
    utf8_path path;

    (path/=dir_)/=name;

    return path.remove();
  } catch (...) {
    IR_LOG_EXCEPTION();
  }

  return false;
}

index_input::ptr fs_directory::open(
    const std::string& name,
    IOAdvice advice) const NOEXCEPT {
  try {
    utf8_path path;
    auto pool_size =
      const_cast<attribute_store&>(attributes()).emplace<fd_pool_size>()->size;

    (path/=dir_)/=name;

    return fs_index_input::open(path.c_str(), pool_size, advice);
  } catch(...) {
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

bool fs_directory::rename(
  const std::string& src, const std::string& dst
) NOEXCEPT {
  try {
    utf8_path src_path;
    utf8_path dst_path;

    (src_path/=dir_)/=src;
    (dst_path/=dir_)/=dst;

    return src_path.rename(dst_path);
  } catch (...) {
    IR_LOG_EXCEPTION();
  }

  return false;
}

bool fs_directory::visit(const directory::visitor_f& visitor) const {
  auto directory = (utf8_path()/=dir_).native();
  bool exists;

  if (!file_utils::exists_directory(exists, directory.c_str()) || !exists) {
    return false;
  }

#ifdef _WIN32
  utf8_path path;
  auto dir_visitor = [&path, &visitor] (const file_path_t name) {
    path.clear();
    path /= name;

    auto filename = path.utf8();
    return visitor(filename);
  };
#else
  std::string filename;
  auto dir_visitor = [&filename, &visitor] (const file_path_t name) {
    filename.assign(name);
    return visitor(filename);
  };
#endif

  return file_utils::visit_directory(directory.c_str(), dir_visitor, false);
}

bool fs_directory::sync(const std::string& name) NOEXCEPT {
  try {
    utf8_path path;

    (path/=dir_)/=name;

    if (file_utils::file_sync(path.c_str())) {
      return true;
    }

    #ifdef _WIN32
      auto error = GetLastError();
    #else
      auto error = errno;
    #endif

    IR_FRMT_ERROR("Failed to sync file, error: %d, path: %s", error, path.utf8().c_str());
  } catch (...) {
    IR_LOG_EXCEPTION();
  }

  return false;
}

MSVC_ONLY(__pragma(warning(pop)))
NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
