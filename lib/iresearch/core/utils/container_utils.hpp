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

#include <array>
#include <memory>

#include "resource_manager.hpp"
#include "shared.hpp"
#include "utils/memory.hpp"
#include "utils/misc.hpp"
#include "utils/noncopyable.hpp"

namespace irs::container_utils {

//////////////////////////////////////////////////////////////////////////////
/// @brief compute individual sizes and offsets of exponentially sized buckets
/// @param NumBuckets the number of bucket descriptions to generate
/// @param SkipBits 2^SkipBits is the size of the first bucket, consequently
///        the number of bits from a 'position' value to place into 1st bucket
//////////////////////////////////////////////////////////////////////////////
struct BucketInfo {
  size_t offset = 0;
  size_t size = 0;
};

template<size_t NumBuckets, size_t SkipBits>
struct BucketMeta {
  static_assert(NumBuckets > 0);

  constexpr BucketMeta() noexcept {
    buckets_[0].size = size_t{1} << SkipBits;
    buckets_[0].offset = 0;

    for (size_t i = 1; i < NumBuckets; ++i) {
      buckets_[i].offset = buckets_[i - 1].offset + buckets_[i - 1].size;
      buckets_[i].size = buckets_[i - 1].size << size_t{1};
    }
  }

  constexpr BucketInfo operator[](size_t i) const noexcept {
    return buckets_[i];
  }

 private:
  std::array<BucketInfo, NumBuckets> buckets_;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief a function to calculate the bucket offset of the reqested position
///        for exponentially sized buckets, e.g. as in BucketMeta
/// @param SkipBits 2^SkipBits is the size of the first bucket, consequently
///        the number of bits from a 'position' value to place into 1st bucket
//////////////////////////////////////////////////////////////////////////////
template<size_t SkipBits>
size_t ComputeBucketOffset(size_t position) noexcept {
  // 63 == 64 bits per size_t - 1 for allignment,
  // +1 == align first value to start of bucket
  return size_t{63} - std::countl_zero((position >> SkipBits) + size_t{1});
}

class raw_block_vector_base : private util::noncopyable {
 public:
  // TODO(MBkkt) We could store only data pointer,
  // everything else could be computed from position in the buffers_
  struct buffer_t {
    // sum of bucket sizes up to but excluding this buffer
    size_t offset{};
    // data is just pointer to speedup vector reallocation
    byte_type* data{};  // pointer at the actual data
    size_t size{};      // total buffer size
  };

  explicit raw_block_vector_base(IResourceManager& rm) noexcept
    : alloc_{rm}, buffers_{{rm}} {}

  raw_block_vector_base(raw_block_vector_base&& rhs) noexcept = default;
  raw_block_vector_base& operator=(raw_block_vector_base&& rhs) = delete;

  ~raw_block_vector_base() { clear(); }

  IRS_FORCE_INLINE size_t buffer_count() const noexcept {
    return buffers_.size();
  }

  IRS_FORCE_INLINE bool empty() const noexcept { return buffers_.empty(); }

  IRS_FORCE_INLINE void clear() noexcept {
    for (auto& buffer : buffers_) {
      alloc_.deallocate(buffer.data, buffer.size);
    }
    buffers_.clear();
  }

  IRS_FORCE_INLINE const buffer_t& get_buffer(size_t i) const noexcept {
    return buffers_[i];
  }

#ifdef IRESEARCH_TEST
  void pop_buffer() noexcept {
    const auto& bucket = buffers_.back();
    alloc_.deallocate(bucket.data, bucket.size);
    buffers_.pop_back();
  }
#endif

 protected:
  IRS_NO_UNIQUE_ADDRESS ManagedTypedAllocator<byte_type> alloc_;
  ManagedVector<buffer_t> buffers_;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief a container allowing raw access to internal storage, and
///        using an allocation strategy similar to an std::deque
//////////////////////////////////////////////////////////////////////////////
template<size_t NumBuckets, size_t SkipBits>
class raw_block_vector : public raw_block_vector_base {
 public:
  static constexpr BucketMeta<NumBuckets, SkipBits> kMeta{};
  static constexpr BucketInfo kLast = kMeta[NumBuckets - 1];

  explicit raw_block_vector(IResourceManager& rm) noexcept
    : raw_block_vector_base{rm} {}

  IRS_FORCE_INLINE size_t buffer_offset(size_t position) const noexcept {
    return position < kLast.offset
             ? ComputeBucketOffset<SkipBits>(position)
             : (NumBuckets - 1 + (position - kLast.offset) / kLast.size);
  }

  const buffer_t& push_buffer() {
    auto v = CreateValue();
    Finally f = [&]() noexcept {
      if (v.data) {
        alloc_.deallocate(v.data, v.size);
      }
    };
    const auto& buffer = buffers_.emplace_back(v);
    v.data = nullptr;
    return buffer;
  }

 private:
  buffer_t CreateValue() {
    if (buffers_.size() < NumBuckets) {
      const auto& bucket = kMeta[buffers_.size()];
      return {bucket.offset, alloc_.allocate(bucket.size), bucket.size};
    }
    const auto& bucket = buffers_.back();
    IRS_ASSERT(bucket.size == kLast.size);
    return {bucket.offset + kLast.size, alloc_.allocate(kLast.size),
            kLast.size};
  }
};

}  // namespace irs::container_utils
