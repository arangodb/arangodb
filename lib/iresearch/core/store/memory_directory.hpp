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

#pragma once

#include <mutex>
#include <shared_mutex>

#include "directory.hpp"
#include "resource_manager.hpp"
#include "store/directory_attributes.hpp"
#include "utils/async_utils.hpp"
#include "utils/attributes.hpp"
#include "utils/string.hpp"

#include <absl/container/flat_hash_map.h>

namespace irs {

class memory_file : public container_utils::raw_block_vector<16, 8> {
  // total number of levels and size of the first level 2^8
  using raw_block_vector_t = container_utils::raw_block_vector<16, 8>;

 public:
  explicit memory_file(IResourceManager& rm) noexcept : raw_block_vector_t{rm} {
    touch(meta_.mtime);
  }

  memory_file(memory_file&& rhs) noexcept
    : raw_block_vector_t(std::move(rhs)), meta_(rhs.meta_), len_(rhs.len_) {
    rhs.len_ = 0;
  }

  memory_file& operator>>(data_output& out) {
    auto len = len_;

    for (size_t i = 0, count = buffer_count(); i < count && len; ++i) {
      auto& buffer = get_buffer(i);
      auto to_copy = (std::min)(len, buffer.size);

      out.write_bytes(buffer.data, to_copy);
      len -= to_copy;
    }

    IRS_ASSERT(!len);  // everything copied

    return *this;
  }

  size_t length() const noexcept { return len_; }

  void length(size_t length) noexcept {
    len_ = length;
    touch(meta_.mtime);
  }

  // used length of the buffer based on total length
  size_t buffer_length(size_t i) const noexcept {
    auto last_buf = buffer_offset(len_);

    if (i == last_buf) {
      auto& buffer = get_buffer(i);

      // %size for the case if the last buffer is not one of the precomputed
      // buckets
      return (len_ - buffer.offset) % buffer.size;
    }

    return i < last_buf ? get_buffer(i).size : 0;
  }

  std::time_t mtime() const noexcept { return meta_.mtime; }

  void reset() noexcept { len_ = 0; }

  void clear() noexcept {
    raw_block_vector_t::clear();
    reset();
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor) {
    for (size_t i = 0, count = buffer_count(); i < count; ++i) {
      if (!visitor(get_buffer(i).data, buffer_length(i))) {
        return false;
      }
    }
    return true;
  }

 private:
  // metadata for a memory_file
  struct meta {
    std::time_t mtime;
  };

  static void touch(std::time_t& time) noexcept {
    time =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  }

  meta meta_;
  size_t len_{};
};

////////////////////////////////////////////////////////////////////////////////
/// @class memory_index_input
/// @brief in memory input stream
////////////////////////////////////////////////////////////////////////////////
class memory_index_input final : public index_input {
 public:
  explicit memory_index_input(const memory_file& file) noexcept;

  index_input::ptr dup() const final;
  int64_t checksum(size_t offset) const final;
  bool eof() const final;
  byte_type read_byte() final;
  const byte_type* read_buffer(size_t size, BufferHint hint) noexcept final;
  const byte_type* read_buffer(size_t offset, size_t size,
                               BufferHint hint) noexcept final;
  size_t read_bytes(byte_type* b, size_t len) final;
  size_t read_bytes(size_t offset, byte_type* b, size_t len) final {
    seek(offset);
    return read_bytes(b, len);
  }
  index_input::ptr reopen() const final;
  size_t length() const final;

  size_t file_pointer() const final;

  void seek(size_t pos) final;

  int16_t read_short() final;
  int32_t read_int() final;
  int64_t read_long() final;
  uint32_t read_vint() final;
  uint64_t read_vlong() final;

  byte_type operator*() { return read_byte(); }
  memory_index_input& operator++() noexcept { return *this; }
  memory_index_input& operator++(int) noexcept { return *this; }

 private:
  memory_index_input(const memory_index_input&) = default;

  void switch_buffer(size_t pos);

  // returns number of reamining bytes in the buffer
  IRS_FORCE_INLINE size_t remain() const { return std::distance(begin_, end_); }

  const memory_file* file_;       // underline file
  const byte_type* buf_{};        // current buffer
  const byte_type* begin_{buf_};  // current position
  const byte_type* end_{buf_};    // end of the valid bytes
  size_t start_{};                // buffer offset in file
};

////////////////////////////////////////////////////////////////////////////////
/// @class memory_index_output
/// @brief in memory output stream
////////////////////////////////////////////////////////////////////////////////
class memory_index_output : public index_output {
 public:
  explicit memory_index_output(memory_file& file) noexcept;
  memory_index_output(const memory_index_output&) = default;
  memory_index_output& operator=(const memory_index_output&) = delete;

  void reset() noexcept;

  // data_output

  void write_byte(byte_type b) final;

  void write_bytes(const byte_type* b, size_t len) final;

  // index_output

  void flush() override;  // deprecated

  size_t file_pointer() const final;

  int64_t checksum() const override;

  void operator>>(data_output& out);

  void write_int(int32_t v) final;

  void write_long(int64_t v) final;

  void write_vint(uint32_t v) final;

  void write_vlong(uint64_t v) final;

  void truncate(size_t pos);

  memory_index_output& operator=(byte_type b) {
    write_byte(b);
    return *this;
  }
  memory_index_output& operator*() noexcept { return *this; }
  memory_index_output& operator++() noexcept { return *this; }
  memory_index_output& operator++(int) noexcept { return *this; }

 protected:
  size_t CloseImpl() final;

  virtual void switch_buffer();

 private:
  // returns number of reamining bytes in the buffer
  IRS_FORCE_INLINE size_t remain() const { return std::distance(pos_, end_); }

 protected:
  memory_file::buffer_t buf_;  // current buffer
  byte_type* pos_;             // position in current buffer

 private:
  memory_file& file_;  // underlying file
  byte_type* end_;
};

////////////////////////////////////////////////////////////////////////////////
/// @class memory_directory
/// @brief in memory index directory
////////////////////////////////////////////////////////////////////////////////
class memory_directory final : public directory {
 public:
  explicit memory_directory(
    directory_attributes attributes = directory_attributes{},
    const ResourceManagementOptions& rm = ResourceManagementOptions::kDefault);

  ~memory_directory() noexcept final;

  directory_attributes& attributes() noexcept final { return attrs_; }

  index_output::ptr create(std::string_view name) noexcept final;

  bool exists(bool& result, std::string_view name) const noexcept final;

  bool length(uint64_t& result, std::string_view name) const noexcept final;

  index_lock::ptr make_lock(std::string_view name) noexcept final;

  bool mtime(std::time_t& result, std::string_view name) const noexcept final;

  index_input::ptr open(std::string_view name,
                        IOAdvice advice) const noexcept final;

  bool remove(std::string_view name) noexcept final;

  bool rename(std::string_view src, std::string_view dst) noexcept final;

  bool sync(std::span<const std::string_view>) noexcept final { return true; }

  bool visit(const visitor_f& visitor) const final;

 private:
  friend class single_instance_lock;
  using files_allocator = ManagedTypedAllocator<
    std::pair<const std::string, std::unique_ptr<memory_file>>>;
  using file_map = absl::flat_hash_map<
    std::string, std::unique_ptr<memory_file>,
    absl::container_internal::hash_default_hash<std::string>,
    absl::container_internal::hash_default_eq<std::string>,
    files_allocator>;  // unique_ptr because of rename
  using lock_map = absl::flat_hash_set<std::string>;

  directory_attributes attrs_;
  mutable std::shared_mutex flock_;
  std::mutex llock_;
  file_map files_;
  lock_map locks_;
};

////////////////////////////////////////////////////////////////////////////////
/// @struct memory_output
/// @brief memory_file + memory_stream
////////////////////////////////////////////////////////////////////////////////
struct memory_output {
  explicit memory_output(IResourceManager& rm) noexcept : file(rm) {}

  memory_output(memory_output&& rhs) noexcept : file(std::move(rhs.file)) {}

  void reset() noexcept {
    file.reset();
    stream.reset();
  }

  memory_file file;
  memory_index_output stream{file};
};

}  // namespace irs
