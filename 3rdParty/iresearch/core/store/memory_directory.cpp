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
#include "memory_directory.hpp"

#include "error/error.hpp"
#include "utils/directory_utils.hpp"
#include "utils/log.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"
#include "utils/std.hpp"
#include "utils/utf8_path.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/numeric_utils.hpp"
#include "utils/crc.hpp"

#include <cassert>
#include <cstring>
#include <algorithm>
  
namespace iresearch {

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
    auto lock = make_lock_guard(parent->llock_);
    return parent->locks_.insert(name).second;
  }

  virtual bool is_locked(bool& result) const noexcept override {
    try {
      auto lock = make_lock_guard(parent->llock_);

      result = parent->locks_.find(name) != parent->locks_.end();

      return true;
    } catch (...) {
    }

    return false;
  }

  virtual bool unlock() noexcept override{
    try {
      auto lock = make_lock_guard(parent->llock_);

      return parent->locks_.erase(name) > 0;
    } catch (...) {
    }

    return false;
  }

 private:
  std::string name;
  memory_directory* parent;
}; // single_instance_lock

// -----------------------------------------------------------------------------
// --SECTION--                                 memory_index_imput implementation
// -----------------------------------------------------------------------------

memory_index_input::memory_index_input(const memory_file& file) noexcept
  : file_(&file) {
}

index_input::ptr memory_index_input::dup() const {
  return ptr(new memory_index_input(*this));
}

int64_t memory_index_input::checksum(size_t offset) const {
  if (!file_->length()) {
    return 0;
  }

  crc32c crc;

  auto buffer_idx = file_->buffer_offset(file_pointer());
  size_t to_process;

  // process current buffer if exists
  if (begin_) {
    to_process = std::min(offset, remain());
    crc.process_bytes(begin_, to_process);
    offset -= to_process;
    ++buffer_idx;
  }

  // process intermediate buffers
  auto last_buffer_idx = file_->buffer_count();

  if (last_buffer_idx) {
    --last_buffer_idx;
  }

  for (; offset && buffer_idx < last_buffer_idx; ++buffer_idx) {
    auto& buf = file_->get_buffer(buffer_idx);
    to_process = std::min(offset, buf.size);
    crc.process_bytes(buf.data, to_process);
    offset -= to_process;
  }

  // process the last buffer
  if (offset && buffer_idx == last_buffer_idx) {
    auto& buf = file_->get_buffer(last_buffer_idx);
    to_process = std::min(offset, file_->length() - buf.offset);
    crc.process_bytes(buf.data, to_process);
    offset -= to_process;
  }

  return crc.checksum();
}

bool memory_index_input::eof() const {
  return file_pointer() >= file_->length();
}

index_input::ptr memory_index_input::reopen() const {
  return dup(); // memory_file pointers are thread-safe
}

void memory_index_input::switch_buffer(size_t pos) {
  auto idx = file_->buffer_offset(pos);
  assert(idx < file_->buffer_count());
  auto& buf = file_->get_buffer(idx);

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


const byte_type* memory_index_input::read_buffer(
    size_t size,
    BufferHint /*hint*/) noexcept {
  const auto* begin = begin_ + size;

  if (begin > end_) {
    return nullptr;
  }

  std::swap(begin, begin_);
  return begin;
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

int32_t memory_index_input::read_int() {
  return remain() < sizeof(uint32_t)
    ? data_input::read_int()
    : irs::read<uint32_t>(begin_);
}

int64_t memory_index_input::read_long() {
  return remain() < sizeof(uint64_t)
    ? data_input::read_long()
    : irs::read<uint64_t>(begin_);
}

uint32_t memory_index_input::read_vint() {
  return remain() < bytes_io<uint32_t>::const_max_vsize
    ? data_input::read_vint()
    : irs::vread<uint32_t>(begin_);
}

uint64_t memory_index_input::read_vlong() {
  return remain() < bytes_io<uint64_t>::const_max_vsize
    ? data_input::read_vlong()
    : irs::vread<uint64_t>(begin_);
}

// -----------------------------------------------------------------------------
// --SECTION--                                memory_index_output implementation
// -----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
/// @class checksum_memory_index_output
//////////////////////////////////////////////////////////////////////////////
class checksum_memory_index_output final : public memory_index_output {
 public:
  explicit checksum_memory_index_output(memory_file& file) noexcept
    : memory_index_output(file) {
    crc_begin_ = pos_;
  }

  virtual void flush() override {
    crc_.process_block(crc_begin_, pos_);
    crc_begin_ = pos_;
    memory_index_output::flush();
  }

  virtual int64_t checksum() const noexcept override {
    crc_.process_block(crc_begin_, pos_);
    crc_begin_ = pos_;
    return crc_.checksum();
  }

 protected:
  void switch_buffer() override {
    crc_.process_block(crc_begin_, pos_);
    memory_index_output::switch_buffer();
    crc_begin_ = pos_;
  }

 private:
  mutable byte_type* crc_begin_;
  mutable crc32c crc_;
}; // checksum_memory_index_output

memory_index_output::memory_index_output(memory_file& file) noexcept
  : file_(file) {
  reset();
}

void memory_index_output::reset() noexcept {
  buf_.data = nullptr;
  buf_.offset = 0;
  buf_.size = 0;
  pos_ = nullptr;
  end_ = nullptr;
}

void memory_index_output::seek(size_t pos) {
  auto idx = file_.buffer_offset(pos);

  buf_ = idx < file_.buffer_count() ? file_.get_buffer(idx) : file_.push_buffer();
  pos_ = buf_.data;
  end_ = buf_.data + buf_.size;
}

void memory_index_output::switch_buffer() {
  auto idx = file_.buffer_offset(file_pointer());

  buf_ = idx < file_.buffer_count() ? file_.get_buffer(idx) : file_.push_buffer();
  pos_ = buf_.data;
  end_ = buf_.data + buf_.size;
}

void memory_index_output::write_long(int64_t value) {
  if (remain() < sizeof(uint64_t)) {
    index_output::write_long(value);
  } else {
    irs::write<uint64_t>(pos_, value);
  }
}

void memory_index_output::write_int(int32_t value) {
  if (remain() < sizeof(uint32_t)) {
    index_output::write_int(value);
  } else {
    irs::write<uint32_t>(pos_, value);
  }
}

void memory_index_output::write_vlong(uint64_t v) {
  if (remain() < bytes_io<uint64_t>::const_max_vsize) {
    index_output::write_vlong(v);
  } else {
    irs::vwrite<uint64_t>(pos_, v);
  }
}

void memory_index_output::write_vint(uint32_t v) {
  if (remain() < bytes_io<uint32_t>::const_max_vsize) {
    index_output::write_vint(v);
  } else {
    irs::vwrite<uint32_t>(pos_, v);
  }
}

void memory_index_output::write_byte(byte_type byte) {
  if (pos_ >= end_) {
    switch_buffer();
  }

  *pos_++ = byte;
}

void memory_index_output::write_bytes( const byte_type* b, size_t len ) {
  assert(b || !len);

  for (size_t to_copy = 0; len; len -= to_copy) {
    if (pos_ >= end_) {
      switch_buffer();
    }

    to_copy = std::min(size_t(std::distance(pos_, end_)), len);
    std::memcpy(pos_, b, sizeof(byte_type) * to_copy);
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
  return buf_.offset + std::distance(buf_.data, pos_);
}

int64_t memory_index_output::checksum() const {
  throw not_supported();
}

void memory_index_output::operator>>( data_output& out ) {
  file_ >> out;
}

// -----------------------------------------------------------------------------
// --SECTION--                                   memory_directory implementation
// -----------------------------------------------------------------------------

memory_directory::memory_directory(size_t pool_size /* = 0*/) {
  alloc_ = &directory_utils::ensure_allocator(*this, pool_size);
}

memory_directory::~memory_directory() noexcept {
  async_utils::read_write_mutex::write_mutex mutex(flock_);
  auto lock = make_lock_guard(mutex);

  files_.clear();
}

attribute_store& memory_directory::attributes() noexcept {
  return attributes_;
}

bool memory_directory::exists(
  bool& result, const std::string& name
) const noexcept {
  async_utils::read_write_mutex::read_mutex mutex(flock_);
  auto lock = make_lock_guard(mutex);

  result = files_.find(name) != files_.end();

  return true;
}

index_output::ptr memory_directory::create(const std::string& name) noexcept {
  try {
    async_utils::read_write_mutex::write_mutex mutex(flock_);
    auto lock = make_lock_guard(mutex);

    auto res = files_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(name),
      std::forward_as_tuple()
    );

    auto& file = res.first->second;

    if (res.second) {
      file = memory::make_unique<memory_file>(*alloc_);
    }

    file->reset(*alloc_);

    return index_output::make<checksum_memory_index_output>(*file);
  } catch(...) {
  }

  return nullptr;
}

bool memory_directory::length(
    uint64_t& result, const std::string& name
) const noexcept {
  async_utils::read_write_mutex::read_mutex mutex(flock_);
  auto lock = make_lock_guard(mutex);

  const auto it = files_.find(name);

  if (it == files_.end()) {
    return false;
  }

  result = it->second->length();

  return true;
}

index_lock::ptr memory_directory::make_lock(
    const std::string& name) noexcept {
  try {
    return index_lock::make<single_instance_lock>(name, this);
  } catch (...) {
  }

  assert(false);
  return nullptr;
}

bool memory_directory::mtime(
    std::time_t& result,
    const std::string& name) const noexcept {
  async_utils::read_write_mutex::read_mutex mutex(flock_);
  auto lock = make_lock_guard(mutex);

  const auto it = files_.find(name);

  if (it == files_.end()) {
    return false;
  }

  result = it->second->mtime();

  return true;
}

index_input::ptr memory_directory::open(
    const std::string& name,
    IOAdvice /*advice*/) const noexcept {
  try {
    async_utils::read_write_mutex::read_mutex mutex(flock_);
    auto lock = make_lock_guard(mutex);

    const auto it = files_.find(name);

    if (it != files_.end()) {
      return memory::make_unique<memory_index_input>(*it->second);
    }

    IR_FRMT_ERROR("Failed to open input file, error: File not found, path: %s", name.c_str());

    return nullptr;
  } catch(...) {
    IR_FRMT_ERROR("Failed to open input file, path: %s", name.c_str());
  }

  return nullptr;
}

bool memory_directory::remove(const std::string& name) noexcept {
  try {
    async_utils::read_write_mutex::write_mutex mutex(flock_);
    auto lock = make_lock_guard(mutex);

    return files_.erase(name) > 0;
  } catch (...) {
  }

  return false;
}

bool memory_directory::rename(
    const std::string& src,
    const std::string& dst) noexcept {
  async_utils::read_write_mutex::write_mutex mutex(flock_);

  try {
    auto lock = make_lock_guard(mutex);

    const auto res = files_.try_emplace(dst);
    auto it = files_.find(src);

    if (IRS_LIKELY(it != files_.end())) {
      if (IRS_LIKELY(it != res.first)) {
        res.first->second = std::move(it->second);
        files_.erase(it);
      }
      return true;
    }

    if (res.second) {
      files_.erase(res.first);
    }
  } catch (...) {
  }

  return false;
}

bool memory_directory::sync(const std::string& /*name*/) noexcept {
  return true;
}

bool memory_directory::visit(const directory::visitor_f& visitor) const {
  std::vector<std::string> files;

  // take a snapshot of existing files in directory
  // to avoid potential recursive read locks in visitor
  {
    async_utils::read_write_mutex::read_mutex mutex(flock_);
    auto lock = make_lock_guard(mutex);

    files.reserve(files_.size());

    for (auto& entry : files_) {
      files.emplace_back(entry.first);
    }
  }

  for (auto& file : files) {
    if (!visitor(file)) {
      return false;
    }
  }

  return true;
}

}
