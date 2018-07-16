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

#ifndef IRESEARCH_CONTAINER_UTILS_H
#define IRESEARCH_CONTAINER_UTILS_H

#include <memory>
#include <vector>

#include "shared.hpp"
#include "math_utils.hpp"
#include "memory.hpp"
#include "noncopyable.hpp"

NS_ROOT
NS_BEGIN(container_utils)

struct bucket_size_t {
  bucket_size_t* next; // next bucket
  size_t offset; // sum of bucket sizes up to but excluding this bucket
  size_t size; // size of this bucket
};

//////////////////////////////////////////////////////////////////////////////
/// @brief compute individual sizes and offsets of exponentially sized buckets
/// @param num_buckets the number of bucket descriptions to generate
/// @param skip_bits 2^skip_bits is the size of the first bucket, consequently
///        the number of bits from a 'position' value to place into 1st bucket
//////////////////////////////////////////////////////////////////////////////
MSVC_ONLY(__pragma(warning(push)))
MSVC_ONLY(__pragma(warning(disable:4127))) // constexp conditionals are intended to be optimized out
template<size_t num_buckets, size_t skip_bits>
std::vector<bucket_size_t> compute_bucket_meta() {
  std::vector<bucket_size_t> bucket_meta(num_buckets);

  if (!num_buckets) {
    return bucket_meta;
  }

  bucket_meta[0].size = 1 << skip_bits;
  bucket_meta[0].offset = 0;

  for (size_t i = 1; i < num_buckets; ++i) {
    bucket_meta[i - 1].next = &bucket_meta[i];
    bucket_meta[i].offset = bucket_meta[i - 1].offset + bucket_meta[i - 1].size;
    bucket_meta[i].size = bucket_meta[i - 1].size << 1;
  }

  // all subsequent buckets should have the same meta as the last bucket
  bucket_meta.back().next = &(bucket_meta.back());

  return bucket_meta;
}
MSVC_ONLY(__pragma(warning(pop)))

//////////////////////////////////////////////////////////////////////////////
/// @brief a function to calculate the bucket offset of the reqested position
///        for exponentially sized buckets, e.g. as per compute_bucket_meta()
/// @param skip_bits 2^skip_bits is the size of the first bucket, consequently
///        the number of bits from a 'position' value to place into 1st bucket
//////////////////////////////////////////////////////////////////////////////
template<size_t skip_bits>
size_t compute_bucket_offset(size_t position) NOEXCEPT {
  // 63 == 64 bits per size_t - 1 for allignment, +1 == align first value to start of bucket
  return 63 - math::clz64((position>>skip_bits) + 1);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief a container allowing raw access to internal storage, and
///        using an allocation strategy similar to an std::deque
//////////////////////////////////////////////////////////////////////////////
template<typename T, size_t num_buckets, size_t skip_bits>
class IRESEARCH_API_TEMPLATE raw_block_vector: util::noncopyable {
 public:
  typedef raw_block_vector<T, num_buckets, skip_bits> raw_block_vector_t;

  struct buffer_t {
    byte_type* data; // pointer at the actual data
    size_t offset; // sum of bucket sizes up to but excluding this buffer
    size_t size; // total buffer size
  };

  raw_block_vector() = default;

  raw_block_vector(raw_block_vector&& other) NOEXCEPT
    : buffers_(std::move(other.buffers_)) {
  }

  FORCE_INLINE size_t buffer_count() const NOEXCEPT {
    return buffers_.size();
  }

  FORCE_INLINE size_t buffer_offset(size_t position) const NOEXCEPT {
    static const auto& last_buffer = get_bucket_meta().back();
    static const auto last_buffer_id = get_bucket_meta().size() - 1;

    // non-precomputed bucket size is the same as the last precomputed bucket size
    return position < last_buffer.offset
      ? get_bucket_offset(position)
      : (last_buffer_id + (position - last_buffer.offset) / last_buffer.size)
      ;
  }

  FORCE_INLINE void clear() NOEXCEPT {
    buffers_.clear();
  }

  FORCE_INLINE const buffer_t& get_buffer(size_t i) const NOEXCEPT {
    return buffers_[i];
  }

  FORCE_INLINE buffer_t& get_buffer(size_t i) NOEXCEPT {
    return buffers_[i];
  }

  buffer_t& push_buffer() {
    static const auto& meta = get_bucket_meta();

    if (buffers_.size() < meta.size()) { // one of the precomputed buckets
      auto& bucket = meta[buffers_.size()];
      buffers_.emplace_back(bucket.offset, bucket.size);
    } else { // non-precomputed buckets, offset is the sum of previous buckets
      assert(!buffers_.empty()); // otherwise do not know what size buckets to create
      auto& bucket = buffers_.back(); // most of the meta from last computed bucket
      auto offset = bucket.offset + bucket.size;
      auto size = bucket.size;
      buffers_.emplace_back(offset, size);
    }

    return buffers_.back();
  }

 private:
   struct buffer_entry_t: buffer_t, util::noncopyable {
     std::unique_ptr<T[]> ptr;
     buffer_entry_t(size_t bucket_offset, size_t bucket_size)
       : ptr(memory::make_unique<T[]>(bucket_size)) {
       buffer_t::data = ptr.get();
       buffer_t::offset = bucket_offset;
       buffer_t::size = bucket_size;
     }
     buffer_entry_t(buffer_entry_t&& other) NOEXCEPT
       : buffer_t(std::move(other)), ptr(std::move(other.ptr)) {
     }
   };

   IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
   std::vector<buffer_entry_t> buffers_;
   IRESEARCH_API_PRIVATE_VARIABLES_END

  static const std::vector<bucket_size_t>& get_bucket_meta() {
    static auto bucket_meta = compute_bucket_meta<num_buckets, skip_bits>();
    return bucket_meta;
  }

  FORCE_INLINE static size_t get_bucket_offset(size_t position) NOEXCEPT {
    return compute_bucket_offset<skip_bits>(position);
  }
};

NS_END // container_utils
NS_END // NS_ROOT

#endif
