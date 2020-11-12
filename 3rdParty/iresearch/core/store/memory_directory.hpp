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

#ifndef IRESEARCH_MEMORYDIRECTORY_H
#define IRESEARCH_MEMORYDIRECTORY_H

#include "directory.hpp"
#include "directory_attributes.hpp"
#include "utils/attributes.hpp"
#include "utils/string.hpp"
#include "utils/async_utils.hpp"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace iresearch {

// <16, 8> => buffer sizes 256B, 512B, 1K, 2K, 4K, 8K, 16K, 32K, 64K, 128K, 256K, 512K, 1M, 2M, 4M, 8M
MSVC_ONLY(template class IRESEARCH_API container_utils::raw_block_vector<
  memory_allocator::allocator_type::SIZE, // total number of levels
  8, // size of the first level 2^8
  memory_allocator::allocator_type
>;)

////////////////////////////////////////////////////////////////////////////////
/// @class memory_file
/// @brief in memory file
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API memory_file
    : public container_utils::raw_block_vector<16, 8, memory_allocator::allocator_type> {
 private:
  typedef container_utils::raw_block_vector<
    memory_allocator::allocator_type::SIZE, // total number of levels
    8, // size of the first level 2^8
    memory_allocator::allocator_type
  > raw_block_vector_t;

 public:
  memory_file(const memory_allocator& alloc) noexcept
    : raw_block_vector_t(alloc) {
    touch(meta_.mtime);
  }

  memory_file(memory_file&& rhs) noexcept
    : raw_block_vector_t(std::move(rhs)),
      meta_(rhs.meta_),
      len_(rhs.len_) {
    rhs.len_ = 0;
  }

  memory_file& operator>>(data_output& out) {
    auto length = len_;

    for (size_t i = 0, count = buffer_count(); i < count && length; ++i) {
      auto& buffer = get_buffer(i);
      auto to_copy = (std::min)(length, buffer.size);

      out.write_bytes(buffer.data, to_copy);
      length -= to_copy;
    }

    assert(!length); // everything copied

    return *this;
  }

  size_t length() const noexcept {
    return len_;
  }

  void length(size_t length) noexcept {
    len_ = length; 
    touch(meta_.mtime);
  }

  // used length of the buffer based on total length
  size_t buffer_length(size_t i) const noexcept {
    auto last_buf = buffer_offset(len_);

    if (i == last_buf) {
      auto& buffer = get_buffer(i);

      // %size for the case if the last buffer is not one of the precomputed buckets
      return (len_ - buffer.offset) % buffer.size;
    }

    return i < last_buf ? get_buffer(i).size : 0;
  }

  std::time_t mtime() const noexcept {
    return meta_.mtime;
  }

  void reset() noexcept {
    len_ = 0;
  }

  void reset(const memory_allocator& alloc) noexcept {
    reset();
    // change internal allocator
    static_cast<allocator_ref_t&>(*this) = static_cast<allocator_type&>(alloc);
  }

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
  static_assert(
    raw_block_vector_t::NUM_BUCKETS == memory_allocator::allocator_type::SIZE,
    "memory allocator is not compatible with a file"
  );

  // metadata for a memory_file
  struct meta {
    std::time_t mtime;
  };

  static void touch(std::time_t& time) noexcept {
    time = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now()
    );
  }

  meta meta_;
  size_t len_{};
}; // memory_file

////////////////////////////////////////////////////////////////////////////////
/// @class memory_index_input
/// @brief in memory input stream
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API memory_index_input final : public index_input {
 public:
  explicit memory_index_input(const memory_file& file) noexcept;

  virtual index_input::ptr dup() const override;
  virtual int64_t checksum(size_t offset) const override;
  virtual bool eof() const override;
  virtual byte_type read_byte() override;
  virtual const byte_type* read_buffer(size_t size, BufferHint hint) noexcept override;
  virtual size_t read_bytes(byte_type* b, size_t len) override;
  virtual index_input::ptr reopen() const override;
  virtual size_t length() const override;

  virtual size_t file_pointer() const override;

  virtual void seek(size_t pos) override;

  virtual int32_t read_int() override;
  virtual int64_t read_long() override;
  virtual uint32_t read_vint() override;
  virtual uint64_t read_vlong() override;

 private:
  memory_index_input(const memory_index_input&) = default;

  void switch_buffer(size_t pos);

  // returns number of reamining bytes in the buffer
  FORCE_INLINE size_t remain() const {
    return std::distance(begin_, end_);
  }

  const memory_file* file_; // underline file
  const byte_type* buf_{}; // current buffer
  const byte_type* begin_{ buf_ }; // current position
  const byte_type* end_{ buf_ }; // end of the valid bytes
  size_t start_{}; // buffer offset in file
}; // memory_index_input

////////////////////////////////////////////////////////////////////////////////
/// @class memory_index_output
/// @brief in memory output stream
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API memory_index_output : public index_output {
 public:
  explicit memory_index_output(memory_file& file) noexcept;
  memory_index_output(const memory_index_output&) = default; 
  memory_index_output& operator=(const memory_index_output&) = delete;

  void reset() noexcept;

  // data_output

  virtual void close() override final;

  virtual void write_byte(byte_type b) override final;

  virtual void write_bytes(const byte_type* b, size_t len) override final;

  // index_output

  virtual void flush() override; // deprecated

  virtual size_t file_pointer() const override final;

  virtual int64_t checksum() const override;

  void operator>>(data_output& out);

  virtual void write_int(int32_t v) override final;

  virtual void write_long(int64_t v) override final;

  virtual void write_vint(uint32_t v) override final;

  virtual void write_vlong(uint64_t v) override final;

  void seek(size_t pos);

 protected:
  virtual void switch_buffer();

 private:
  // returns number of reamining bytes in the buffer
  FORCE_INLINE size_t remain() const {
    return std::distance(pos_, end_);
  }

 protected:
  memory_file::buffer_t buf_; // current buffer
  byte_type* pos_; // position in current buffer

 private:
  memory_file& file_; // underlying file
  byte_type* end_;
};

////////////////////////////////////////////////////////////////////////////////
/// @class memory_directory
/// @brief in memory index directory
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API memory_directory final : public directory {
 public:
  // 0 == pool_size -> use global allocator, noexcept
  explicit memory_directory(size_t pool_size = 0);

  virtual ~memory_directory() noexcept;

  using directory::attributes;

  virtual attribute_store& attributes() noexcept override;

  virtual index_output::ptr create(const std::string& name) noexcept override;

  virtual bool exists(
    bool& result, const std::string& name
  ) const noexcept override;

  virtual bool length(
    uint64_t& result, const std::string& name
  ) const noexcept override;

  virtual index_lock::ptr make_lock(const std::string& name) noexcept override;

  virtual bool mtime(
    std::time_t& result, const std::string& name
  ) const noexcept override;

  virtual index_input::ptr open(
    const std::string& name,
    IOAdvice advice
  ) const noexcept override;

  virtual bool remove(const std::string& name) noexcept override;

  virtual bool rename(
    const std::string& src,
    const std::string& dst
  ) noexcept override;

  virtual bool sync(const std::string& name) noexcept override;

  virtual bool visit(const visitor_f& visitor) const override;

 private:
  friend class single_instance_lock;
  typedef std::unordered_map<std::string, std::unique_ptr<memory_file>> file_map; // unique_ptr because of rename
  typedef std::unordered_set<std::string> lock_map;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  const memory_allocator* alloc_;
  mutable async_utils::read_write_mutex flock_;
  std::mutex llock_;
  attribute_store attributes_;
  file_map files_;
  lock_map locks_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

////////////////////////////////////////////////////////////////////////////////
/// @struct memory_output
/// @brief memory_file + memory_stream
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API memory_output {
  explicit memory_output(const memory_allocator& alloc) noexcept
    : file(alloc) {
  }

  memory_output(memory_output&& rhs) noexcept
    : file(std::move(rhs.file)) {
  }

  void reset() noexcept {
    file.reset();
    stream.reset();
  }

  void reset(const memory_allocator& alloc) noexcept {
    file.reset(alloc);
    stream.reset();
  }

  memory_file file;
  memory_index_output stream{ file };
}; // memory_output

}

#endif
