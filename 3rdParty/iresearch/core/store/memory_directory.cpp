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

#include "shared.hpp"
#include "memory_directory.hpp"
#include "checksum_io.hpp"

#include "error/error.hpp"
#include "utils/log.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"
#include "utils/std.hpp"
#include "utils/utf8_path.hpp"

#include <cassert>
#include <cstring>
#include <algorithm>

#if defined(_MSC_VER)
  #pragma warning(disable : 4244)
  #pragma warning(disable : 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

#include <boost/crc.hpp>

#if defined(_MSC_VER)
  #pragma warning(default: 4244)
  #pragma warning(default: 4245)
#elif defined (__GNUC__)
  // NOOP
#endif
  
NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class single_instance_lock
//////////////////////////////////////////////////////////////////////////////
class single_instance_lock : public index_lock {
 public:
  single_instance_lock(
      const std::string& name, 
      memory_directory* parent)
    : name(name), parent(parent) {
    assert(parent);
  }

  virtual bool lock() override {
    SCOPED_LOCK(parent->llock_);
    return parent->locks_.insert(name).second;
  }

  virtual bool is_locked(bool& result) const NOEXCEPT override {
    try {
      SCOPED_LOCK(parent->llock_);

      result = parent->locks_.find(name) != parent->locks_.end();

      return true;
    } catch (...) {
      IR_EXCEPTION();
    }

    return false;
  }

  virtual bool unlock() NOEXCEPT override{
    try {
      SCOPED_LOCK(parent->llock_);

      return parent->locks_.erase(name) > 0;
    } catch (...) {
      IR_EXCEPTION();
    }

    return false;
  }

 private:
  std::string name;
  memory_directory* parent;
}; // single_instance_lock

/* -------------------------------------------------------------------
 * memory_index_input 
 * ------------------------------------------------------------------*/

memory_index_input::memory_index_input(const memory_file& file) NOEXCEPT
  : file_(&file) {
}

index_input::ptr memory_index_input::dup() const NOEXCEPT {
  try {
    PTR_NAMED(memory_index_input, ptr, *this);
    return ptr;
  } catch(...) {
    IR_EXCEPTION();
  }

  return nullptr;
}

bool memory_index_input::eof() const {
  return file_pointer() >= file_->length();
}

index_input::ptr memory_index_input::reopen() const NOEXCEPT {
  return dup(); // memory_file pointers are thread-safe
}

void memory_index_input::switch_buffer(size_t pos) {
  auto idx = file_->buffer_offset(pos);
  assert(idx < file_->buffer_count());
  auto buf = file_->get_buffer(idx);

  if (buf.data != buf_) {
    buf_ = buf.data;
    start_ = buf.offset;
    end_ = buf_ + std::min(buf.size, file_->length() - start_);
  }

  assert(start_ <= pos && pos < start_ + std::distance(buf_, end_));
  begin_ = buf_ + (pos - start_);
}

size_t memory_index_input::length() const {
  return file_->length();
}

size_t memory_index_input::file_pointer() const {
  return start_ + std::distance(buf_, begin_);
}

void memory_index_input::seek(size_t pos) {
  // allow seeking past eof(), set to eof
  if (pos >= file_->length()) {
    buf_ = nullptr;
    begin_ = nullptr;
    start_ = file_->length();
    return;
  }

  switch_buffer(pos);
}

byte_type memory_index_input::read_byte() {
  if (begin_ >= end_) {
    switch_buffer(file_pointer());
  }

  return *begin_++;
}

size_t memory_index_input::read_bytes(byte_type* b, size_t left) {  
  const size_t length = left; // initial length
  size_t copied;
  while (left) {
    if (begin_ >= end_) {
      if (eof()) {
        break;
      }
      switch_buffer(file_pointer());
    }

    copied = std::min(size_t(std::distance(begin_, end_)), left);
    std::memcpy(b, begin_, sizeof(byte_type) * copied);

    left -= copied;
    begin_ += copied;
    b += copied;
  }
  return length - left;
}

/* -------------------------------------------------------------------
 * memory_index_output
 * ------------------------------------------------------------------*/

memory_index_output::memory_index_output(memory_file& file) NOEXCEPT
  : file_(file) {
  reset();
}

void memory_index_output::reset() {
  buf_.data = nullptr;
  buf_.offset = 0;
  buf_.size = 0;
  pos_ = 0;
}

void memory_index_output::switch_buffer() {
  auto idx = file_.buffer_offset(file_pointer());

  buf_ = idx < file_.buffer_count() ? file_.get_buffer(idx) : file_.push_buffer();
  pos_ = 0;
}

void memory_index_output::write_byte( byte_type byte ) {
  if (pos_ >= buf_.size) {
    switch_buffer();
  }

  buf_.data[pos_++] = byte;
}

void memory_index_output::write_bytes( const byte_type* b, size_t len ) {
  assert(b || !len);

  for (size_t to_copy = 0; len; len -= to_copy) {
    if (pos_ >= buf_.size) {
      switch_buffer();
    }

    to_copy = std::min(buf_.size - pos_, len);
    std::memcpy(buf_.data + pos_, b, sizeof(byte_type) * to_copy);
    b += to_copy;
    pos_ += to_copy;
  }
}

void memory_index_output::close() {
  flush();
}

void memory_index_output::flush() {
  file_.length(memory_index_output::file_pointer());
}

size_t memory_index_output::file_pointer() const {
  return buf_.offset + pos_;
}

int64_t memory_index_output::checksum() const {
  throw not_supported();
}

void memory_index_output::operator>>( data_output& out ) {
  file_ >> out;
}

/* -------------------------------------------------------------------
 * memory_directory
 * ------------------------------------------------------------------*/

memory_directory::~memory_directory() { }

attribute_store& memory_directory::attributes() NOEXCEPT {
  return attributes_;
}

void memory_directory::close() NOEXCEPT {
  async_utils::read_write_mutex::write_mutex mutex(flock_);
  SCOPED_LOCK(mutex);

  files_.clear();
}

bool memory_directory::exists(
  bool& result, const std::string& name
) const NOEXCEPT {
  async_utils::read_write_mutex::read_mutex mutex(flock_);
  SCOPED_LOCK(mutex);

  result = files_.find(name) != files_.end();

  return true;
}

index_output::ptr memory_directory::create(const std::string& name) NOEXCEPT {
  typedef checksum_index_output<boost::crc_32_type> checksum_output_t;

  try {
    async_utils::read_write_mutex::write_mutex mutex(flock_);
    SCOPED_LOCK(mutex);

    auto res = files_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(name),
      std::forward_as_tuple()
    );
    auto& it = res.first;
    auto& file = it->second;

    if (!res.second) { // file exists
      file->reset();
    } else {
      file = memory::make_unique<memory_file>();
    }

    PTR_NAMED(memory_index_output, out, *file);

    return index_output::make<checksum_output_t>(std::move(out));
  } catch(...) {
    IR_EXCEPTION();
  }

  return nullptr;
}

bool memory_directory::length(
    uint64_t& result, const std::string& name
) const NOEXCEPT {
  async_utils::read_write_mutex::read_mutex mutex(flock_);
  SCOPED_LOCK(mutex);

  const auto it = files_.find(name);

  if (it == files_.end() || !it->second) {
    return false;
  }

  result = it->second->length();

  return true;
}

index_lock::ptr memory_directory::make_lock(
    const std::string& name
) NOEXCEPT {
  try {
    return index_lock::make<single_instance_lock>(name, this);
  } catch (...) {
    IR_EXCEPTION();
  }

  assert(false);
  return nullptr;
}

bool memory_directory::mtime(
    std::time_t& result,
    const std::string& name
) const NOEXCEPT {
  async_utils::read_write_mutex::read_mutex mutex(flock_);
  SCOPED_LOCK(mutex);

  const auto it = files_.find(name);

  if (it == files_.end() || !it->second) {
    return false;
  }

  result = it->second->mtime();

  return true;
}

index_input::ptr memory_directory::open(
    const std::string& name
) const NOEXCEPT {
  try {
    async_utils::read_write_mutex::read_mutex mutex(flock_);
    SCOPED_LOCK(mutex);

    const auto it = files_.find(name);

    if (it != files_.end()) {
      return index_input::make<memory_index_input>(*it->second);
    }

    IR_FRMT_ERROR("Failed to open input file, error: File not found, path: %s", name.c_str());

    return nullptr;
  } catch(...) {
    IR_EXCEPTION();
  }

  return nullptr;
}

bool memory_directory::remove(const std::string& name) NOEXCEPT {
  try {
    async_utils::read_write_mutex::write_mutex mutex(flock_);
    SCOPED_LOCK(mutex);

    return files_.erase(name) > 0;
  } catch (...) {
    IR_EXCEPTION();
  }

  return false;
}

bool memory_directory::rename(
    const std::string& src, const std::string& dst
) NOEXCEPT {
  try {
    async_utils::read_write_mutex::write_mutex mutex(flock_);
    SCOPED_LOCK(mutex);

    auto it = files_.find(src);

    if (it == files_.end()) {
      return false;
    }

    files_.erase(dst); // emplace() will not overwrite as per spec
    files_.emplace(dst, std::move(it->second));
    files_.erase(it);

    return true;
  } catch (...) {
    IR_EXCEPTION();
  }

  return false;
}

bool memory_directory::sync(const std::string& /*name*/) NOEXCEPT {
  return true;
}

bool memory_directory::visit(const directory::visitor_f& visitor) const {
  std::string filename;

  // note that using non const functions in 'visitor' will cuase deadlock
  async_utils::read_write_mutex::read_mutex mutex(flock_);
  SCOPED_LOCK(mutex);

  for (auto& entry : files_) {
    filename = entry.first;
    if (!visitor(filename)) {
      return false;
    }
  }

  return true;
}

NS_END