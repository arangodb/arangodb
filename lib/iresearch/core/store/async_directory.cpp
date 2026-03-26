////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "async_directory.hpp"

#include "liburing.h"
#include "store_utils.hpp"
#include "utils/crc.hpp"
#include "utils/file_utils.hpp"
#include "utils/memory.hpp"

#include <absl/strings/str_cat.h>

namespace irs {
namespace {

constexpr size_t kNumPages = 128;
constexpr size_t kPageSize = 4096;
constexpr size_t kPageAlignment = 4096;

struct BufferDeleter {
  void operator()(byte_type* memory) const noexcept { ::free(memory); }
};

std::unique_ptr<byte_type, BufferDeleter> allocate() {
  constexpr size_t kBufSize = kNumPages * kPageSize;

  void* mem = nullptr;
  if (posix_memalign(&mem, kPageAlignment, kBufSize) != 0) {
    throw std::bad_alloc();
  }

  return std::unique_ptr<byte_type, BufferDeleter>{
    static_cast<byte_type*>(mem)};
}

class SegregatedBuffer {
 public:
  using node_type = concurrent_stack<byte_type*>::node_type;

  SegregatedBuffer();

  node_type* pop() noexcept { return free_pages_.pop(); }

  void push(node_type& node) noexcept { free_pages_.push(node); }

  constexpr size_t size() const noexcept { return kNumPages * kPageSize; }

  byte_type* data() const noexcept { return buffer_.get(); }

 private:
  std::unique_ptr<byte_type, BufferDeleter> buffer_;
  node_type pages_[kNumPages];
  concurrent_stack<byte_type*> free_pages_;
};

SegregatedBuffer::SegregatedBuffer() : buffer_{allocate()} {
  auto* begin = buffer_.get();
  for (auto& page : pages_) {
    page.value = begin;
    free_pages_.push(page);
    begin += kPageSize;
  }
}

class URing {
 public:
  URing(size_t queue_size, unsigned flags) {
    if (io_uring_queue_init(queue_size, &ring, flags) != 0) {
      throw not_supported{};
    }
  }

  ~URing() { io_uring_queue_exit(&ring); }

  void reg_buffer(byte_type* b, size_t size);

  size_t size() const noexcept { return ring.sq.ring_sz; }

  io_uring_sqe* get_sqe() noexcept { return io_uring_get_sqe(&ring); }

  size_t sq_ready() noexcept { return io_uring_sq_ready(&ring); }

  void submit();
  bool deque(bool wait, uint64_t* data);

  io_uring ring;
};

void URing::reg_buffer(byte_type* b, size_t size) {
  iovec iovec{.iov_base = b, .iov_len = size};
  if (io_uring_register_buffers(&ring, &iovec, 1) != 0) {
    throw illegal_state("failed to register buffer");
  }
}

void URing::submit() {
  const int ret = io_uring_submit(&ring);
  if (ret < 0) {
    throw io_error{
      absl::StrCat("failed to submit write request, error ", -ret)};
  }
}

bool URing::deque(bool wait, uint64_t* data) {
  io_uring_cqe* cqe = nullptr;
  const int ret =
    wait ? io_uring_wait_cqe(&ring, &cqe) : io_uring_peek_cqe(&ring, &cqe);

  if (ret < 0) {
    if (ret != -EAGAIN) {
      throw io_error{absl::StrCat("Failed to peek a request, error ", -ret)};
    }

    return false;
  }

  IRS_ASSERT(cqe != nullptr);
  if (cqe->res < 0) {
    throw io_error{
      absl::StrCat("Async i/o operation failed, error ", -cqe->res)};
  }

  *data = cqe->user_data;
  io_uring_cqe_seen(&ring, cqe);

  return true;
}

}  // namespace

class AsyncFile {
 public:
  AsyncFile(size_t queue_size, unsigned flags) : ring_(queue_size, flags) {
    ring_.reg_buffer(buffer_.data(), buffer_.size());
  }

  io_uring_sqe* get_sqe();
  void submit() { return ring_.submit(); }
  void drain(bool wait);

  SegregatedBuffer::node_type* get_buffer();
  void release_buffer(SegregatedBuffer::node_type& node) noexcept {
    buffer_.push(node);
  }

 private:
  SegregatedBuffer buffer_;
  URing ring_;
};

AsyncFileBuilder::ptr AsyncFileBuilder::make(size_t queue_size,
                                             unsigned flags) {
  return AsyncFileBuilder::ptr(new AsyncFile(queue_size, flags));
}

void AsyncFileDeleter::operator()(AsyncFile* p) noexcept { delete p; }

SegregatedBuffer::node_type* AsyncFile::get_buffer() {
  uint64_t data = 0;
  while (ring_.deque(false, &data) && data == 0) {
  }

  if (data != 0) {
    return reinterpret_cast<SegregatedBuffer::node_type*>(data);
  }

  auto* node = buffer_.pop();

  if (node != nullptr) {
    return node;
  }

  while (ring_.deque(true, &data) && data == 0) {
  }
  return reinterpret_cast<SegregatedBuffer::node_type*>(data);
}

io_uring_sqe* AsyncFile::get_sqe() {
  io_uring_sqe* sqe = ring_.get_sqe();

  if (sqe == nullptr) {
    uint64_t data = 0;

    if (!ring_.deque(false, &data)) {
      IRS_ASSERT(ring_.sq_ready());
      ring_.submit();
      ring_.deque(true, &data);
    }

    sqe = ring_.get_sqe();

    if (data != 0) {
      buffer_.push(*reinterpret_cast<SegregatedBuffer::node_type*>(&data));
    }
  }

  IRS_ASSERT(sqe);
  return sqe;
}

void AsyncFile::drain(bool wait) {
  io_uring_sqe* sqe = get_sqe();
  IRS_ASSERT(sqe);

  io_uring_prep_nop(sqe);
  sqe->flags |= (IOSQE_IO_LINK | IOSQE_IO_DRAIN);
  sqe->user_data = 0;

  ring_.submit();

  if (!wait) {
    return;
  }

  uint64_t data = 0;
  for (;;) {
    ring_.deque(true, &data);

    if (data == 0) {
      return;
    }

    buffer_.push(*reinterpret_cast<SegregatedBuffer::node_type*>(data));
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @class async_index_output
//////////////////////////////////////////////////////////////////////////////
class AsyncIndexOutput final : public index_output {
 public:
  DEFINE_FACTORY_INLINE(AsyncIndexOutput);

  static index_output::ptr open(const path_char_t* name,
                                AsyncFilePtr&& async) noexcept;

  void write_int(int32_t value) final;
  void write_long(int64_t value) final;
  void write_vint(uint32_t v) final;
  void write_vlong(uint64_t v) final;
  void write_byte(byte_type b) final;
  void write_bytes(const byte_type* b, size_t length) final;
  size_t file_pointer() const final {
    IRS_ASSERT(buf_->value <= pos_);
    return start_ + static_cast<size_t>(std::distance(buf_->value, pos_));
  }
  void flush() final;

  int64_t checksum() const final {
    const_cast<AsyncIndexOutput*>(this)->flush();
    return crc_.checksum();
  }

 private:
  size_t CloseImpl() final {
    Finally reset = [this]() noexcept {
      async_->release_buffer(*buf_);
      handle_.reset(nullptr);
    };

    flush();

    // FIXME(gnusi): we can avoid waiting here in case
    // if we'll keep track of all unsynced files
    async_->drain(true);
    return file_pointer();
  }

  using node_type = concurrent_stack<byte_type*>::node_type;

  AsyncIndexOutput(AsyncFilePtr&& async, file_utils::handle_t&& handle) noexcept
    : async_{std::move(async)},
      handle_{std::move(handle)},
      buf_{async_->get_buffer()} {
    reset(buf_->value);
  }

  // returns number of reamining bytes in the buffer
  IRS_FORCE_INLINE size_t remain() const noexcept {
    return std::distance(pos_, end_);
  }

  void reset(byte_type* buf) noexcept {
    pos_ = buf;
    end_ = buf + kPageSize;
  }

  AsyncFilePtr async_;
  file_utils::handle_t handle_;
  node_type* buf_;
  crc32c crc_;

  byte_type* pos_{};  // current position in the buffer
  byte_type* end_{};  // end of the valid bytes in the buffer
  size_t start_{};    // position of the buffer in file
};

index_output::ptr AsyncIndexOutput::open(const path_char_t* name,
                                         AsyncFilePtr&& async) noexcept {
  IRS_ASSERT(name);

  if (!async) {
    return nullptr;
  }

  file_utils::handle_t handle(
    file_utils::open(name, file_utils::OpenMode::Write, IR_FADVICE_NORMAL));

  if (nullptr == handle) {
    IRS_LOG_ERROR(
      absl::StrCat("Failed to open output file, error: ", GET_ERROR(),
                   ", path: ", file_utils::ToStr(name)));

    return nullptr;
  }

  try {
    return index_output::make<AsyncIndexOutput>(std::move(async),
                                                std::move(handle));
  } catch (...) {
  }

  return nullptr;
}

void AsyncIndexOutput::write_int(int32_t value) {
  if (remain() < sizeof(uint32_t)) {
    irs::write<uint32_t>(*this, value);
  } else {
    irs::write<uint32_t>(pos_, value);
  }
}

void AsyncIndexOutput::write_long(int64_t value) {
  if (remain() < sizeof(uint64_t)) {
    irs::write<uint64_t>(*this, value);
  } else {
    irs::write<uint64_t>(pos_, value);
  }
}

void AsyncIndexOutput::write_vint(uint32_t v) {
  if (remain() < bytes_io<uint32_t>::const_max_vsize) {
    irs::vwrite<uint32_t>(*this, v);
  } else {
    irs::vwrite<uint32_t>(pos_, v);
  }
}

void AsyncIndexOutput::write_vlong(uint64_t v) {
  if (remain() < bytes_io<uint64_t>::const_max_vsize) {
    irs::vwrite<uint64_t>(*this, v);
  } else {
    irs::vwrite<uint64_t>(pos_, v);
  }
}

void AsyncIndexOutput::write_byte(byte_type b) {
  if (pos_ >= end_) {
    flush();
  }

  *pos_++ = b;
}

void AsyncIndexOutput::flush() {
  IRS_ASSERT(handle_);

  auto* buf = buf_->value;
  const auto size = static_cast<size_t>(std::distance(buf, pos_));

  if (size == 0) {
    return;
  }

  io_uring_sqe* sqe = async_->get_sqe();

  io_uring_prep_write_fixed(sqe, handle_cast(handle_.get()), buf, size, start_,
                            0);
  sqe->user_data = reinterpret_cast<uint64_t>(buf_);

  async_->submit();

  start_ += size;
  crc_.process_bytes(buf_->value, size);

  buf_ = async_->get_buffer();

  IRS_ASSERT(buf_);
  reset(buf_->value);
}

void AsyncIndexOutput::write_bytes(const byte_type* b, size_t length) {
  IRS_ASSERT(pos_ <= end_);
  auto left = static_cast<size_t>(std::distance(pos_, end_));

  if (left > length) {
    std::memcpy(pos_, b, length);
    pos_ += length;
  } else {
    auto* buf = buf_->value;

    std::memcpy(pos_, b, left);
    pos_ += left;
    {
      auto* buf = buf_->value;
      io_uring_sqe* sqe = async_->get_sqe();
      io_uring_prep_write_fixed(sqe, handle_cast(handle_.get()), buf, kPageSize,
                                start_, 0);
      sqe->user_data = reinterpret_cast<uint64_t>(buf_);
    }
    length -= left;
    b += left;
    start_ += kPageSize;

    const auto* borig = b;
    const size_t count = length / kPageSize;
    for (size_t i = count; i != 0; --i) {
      buf_ = async_->get_buffer();
      IRS_ASSERT(buf_);
      pos_ = buf_->value;
      std::memcpy(pos_, b, kPageSize);
      {
        auto* buf = buf_->value;
        io_uring_sqe* sqe = async_->get_sqe();
        io_uring_prep_write_fixed(sqe, handle_cast(handle_.get()), buf,
                                  kPageSize, start_, 0);
        sqe->user_data = reinterpret_cast<uint64_t>(buf_);
      }
      b += kPageSize;
      start_ += kPageSize;
    }

    async_->submit();

    const size_t processed = kPageSize * count;
    crc_.process_bytes(buf, kPageSize);
    crc_.process_bytes(borig, count * kPageSize);

    buf_ = async_->get_buffer();
    reset(buf_->value);

    length -= processed;
    std::memcpy(pos_, b, length);
    pos_ += length;
  }
}

AsyncDirectory::AsyncDirectory(std::filesystem::path dir,
                               directory_attributes attrs,
                               const ResourceManagementOptions& rm,
                               size_t pool_size, size_t queue_size,
                               unsigned flags)
  : MMapDirectory{std::move(dir), std::move(attrs), rm},
    async_pool_{pool_size},
    queue_size_{queue_size},
    flags_{flags} {}

index_output::ptr AsyncDirectory::create(std::string_view name) noexcept {
  std::filesystem::path path;

  try {
    (path /= directory()) /= name;

    return AsyncIndexOutput::open(path.c_str(),
                                  async_pool_.emplace(queue_size_, flags_));
  } catch (...) {
  }

  return nullptr;
}

bool AsyncDirectory::sync(std::span<const std::string_view> names) noexcept {
  std::filesystem::path path;

  try {
    std::vector<file_utils::handle_t> handles(names.size());
    path /= directory();

    auto async = async_pool_.emplace(queue_size_, flags_);

    for (auto name = names.begin(); auto& handle : handles) {
      std::filesystem::path full_path(path);
      full_path /= (*name);
      ++name;

      const int fd = ::open(full_path.c_str(), O_WRONLY, S_IRWXU);

      if (fd < 0) {
        return false;
      }

      handle.reset(reinterpret_cast<void*>(fd));

      auto* sqe = async->get_sqe();

      while (sqe == nullptr) {
        async->submit();
        sqe = async->get_sqe();
      }

      io_uring_prep_fsync(sqe, fd, 0);
      sqe->user_data = 0;
    }

    // FIXME(gnusi): or submit one-by-one?
    async->submit();
    async->drain(true);
  } catch (...) {
    return false;
  }

  return true;
}

}  // namespace irs
