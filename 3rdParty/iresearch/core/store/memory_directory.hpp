//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_MEMORYDIRECTORY_H
#define IRESEARCH_MEMORYDIRECTORY_H

#include "directory.hpp"
#include "utils/attributes.hpp"
#include "utils/container_utils.hpp"
#include "utils/string.hpp"
#include "utils/async_utils.hpp"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

NS_LOCAL

inline void touch(std::time_t& time) {
  time = std::chrono::system_clock::to_time_t(
    std::chrono::system_clock::now()
  );
}

NS_END

NS_ROOT

// -------------------------------------------------------------------
// metadata for a memory_file
// -------------------------------------------------------------------
struct memory_file_meta {
  std::time_t mtime;
};

/* -------------------------------------------------------------------
* memory_file
* ------------------------------------------------------------------*/

MSVC_ONLY(template class IRESEARCH_API iresearch::container_utils::raw_block_vector<iresearch::byte_type, 16, 8>);

// <16, 8> => buffer sizes 256B, 512B, 1K, 2K, 4K, 8K, 16K, 32K, 64K, 128K, 256K, 512K, 1M, 2M, 4M, 8M
class IRESEARCH_API memory_file:
  public container_utils::raw_block_vector<byte_type, 16, 8> {
 public:
  DECLARE_PTR(memory_file);

  explicit memory_file() {
    touch(meta_.mtime);
  }

  memory_file(memory_file&& rhs) NOEXCEPT
    : raw_block_vector_t(std::move(rhs)), len_(rhs.len_) {
    rhs.len_ = 0;
  }

  memory_file& operator>>(data_output& out) {
    auto length = len_;

    for (size_t i = 0, count = buffer_count(); i < count && length; ++i) {
      auto buffer = get_buffer(i);
      auto to_copy = (std::min)(length, buffer.size);

      out.write_bytes(buffer.data, to_copy);
      length -= to_copy;
    }

    assert(!length); // everything copied

    return *this;
  }

  size_t length() const { return len_; }

  void length(size_t length) { 
    len_ = length; 
    touch(meta_.mtime);
  }

  // used length of the buffer based on total length
  size_t buffer_length(size_t i) const {
    auto last_buf = buffer_offset(len_);

    if (i == last_buf) {
      auto buffer = get_buffer(i);

      // %size for the case if the last buffer is not one of the precomputed buckets
      return (len_ - buffer.offset) % buffer.size;
    }

    return i < last_buf ? get_buffer(i).size : 0;
  }

  std::time_t mtime() const NOEXCEPT { return meta_.mtime; }

  void reset() { len_ = 0; }

  void clear() {
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
  memory_file_meta meta_;
  size_t len_{};
};

/* -------------------------------------------------------------------
 * memory_index_input 
 * ------------------------------------------------------------------*/

struct memory_buffer;

class IRESEARCH_API memory_index_input final : public index_input {
 public:
  explicit memory_index_input(const memory_file& file) NOEXCEPT;

  virtual ptr dup() const NOEXCEPT override;
  virtual bool eof() const override;
  virtual byte_type read_byte() override;
  virtual size_t read_bytes(byte_type* b, size_t len) override;
  virtual ptr reopen() const NOEXCEPT override;
  virtual size_t length() const override;

  virtual size_t file_pointer() const override;

  virtual void seek(size_t pos) override;

 private:
  memory_index_input(const memory_index_input&) = default;

  void switch_buffer(size_t pos);

  const memory_file* file_; // underline file
  const byte_type* buf_{}; // current buffer
  const byte_type* begin_{ buf_ }; // current position
  const byte_type* end_{ buf_ }; // end of the valid bytes
  size_t start_{}; // buffer offset in file
};

/* -------------------------------------------------------------------
 * memory_index_output
 * ------------------------------------------------------------------*/

class IRESEARCH_API memory_index_output final : public index_output {
 public:
  explicit memory_index_output(memory_file& file) NOEXCEPT;
  memory_index_output(const memory_index_output&) = default; 
  memory_index_output& operator=(const memory_index_output&) = delete;

  void reset();

  // data_output

  virtual void close() override;

  virtual void write_byte( byte_type b ) override;

  virtual void write_bytes( const byte_type* b, size_t len ) override;

  // index_output

  virtual void flush() override; // deprecated

  virtual size_t file_pointer() const override;

  virtual int64_t checksum() const override;

  void operator>>( data_output& out );

 private:
  void switch_buffer();

  memory_file& file_; // underlying file
  memory_file::buffer_t buf_; // current buffer
  size_t pos_; /* position in current buffer */
};

// -------------------------------------------------------------------
//                                                    memory_directory
// -------------------------------------------------------------------
class IRESEARCH_API memory_directory final : public directory {
 public:
  virtual ~memory_directory();

  using directory::attributes;

  virtual attribute_store& attributes() NOEXCEPT override;

  virtual void close() NOEXCEPT override;

  virtual index_output::ptr create(const std::string& name) NOEXCEPT override;

  virtual bool exists(
    bool& result, const std::string& name
  ) const NOEXCEPT override;

  virtual bool length(
    uint64_t& result, const std::string& name
  ) const NOEXCEPT override;

  virtual index_lock::ptr make_lock(const std::string& name) NOEXCEPT override;

  virtual bool mtime(
    std::time_t& result, const std::string& name
  ) const NOEXCEPT override;

  virtual index_input::ptr open(
    const std::string& name
  ) const NOEXCEPT override;

  virtual bool remove(const std::string& name) NOEXCEPT override;

  virtual bool rename(
    const std::string& src, const std::string& dst
  ) NOEXCEPT override;

  virtual bool sync(const std::string& name) NOEXCEPT override;

  virtual bool visit(const visitor_f& visitor) const override;

 private:
  friend class single_instance_lock;
  typedef std::unordered_map<std::string, memory_file::ptr> file_map;
  typedef std::unordered_set<std::string> lock_map;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  mutable async_utils::read_write_mutex flock_;
  std::mutex llock_;
  attribute_store attributes_;
  file_map files_;
  lock_map locks_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

/* -------------------------------------------------------------------
 * memory_output
 * ------------------------------------------------------------------*/

struct IRESEARCH_API memory_output {
  memory_output() = default;

  memory_output(memory_output&& rhs) NOEXCEPT
    : file(std::move(rhs.file)) {
  }

  void reset() {
    file.reset();
    stream.reset();
  }

  memory_file file;
  memory_index_output stream{ file };
};

NS_END

#endif
