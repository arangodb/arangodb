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

#include <array>
#include <memory>

#include "shared.hpp"
#include "math_utils.hpp"
#include "memory.hpp"
#include "object_pool.hpp"
#include "noncopyable.hpp"

NS_ROOT
NS_BEGIN(container_utils)

//////////////////////////////////////////////////////////////////////////////
/// @class array
/// @brief same as 'std::array' but this implementation capable of storing
///        objects without default constructor
//////////////////////////////////////////////////////////////////////////////
template<typename T, size_t Size>
class array
  : private irs::memory::aligned_storage<sizeof(T)*Size, ALIGNOF(T)>,
    private util::noncopyable {
 private:
  typedef irs::memory::aligned_storage<sizeof(T)*Size, ALIGNOF(T)> buffer_t;

 public:
  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* iterator;
  typedef const T* const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  static const size_t SIZE = Size;

  template<typename... Args>
  explicit array(Args&&... args) {
    auto begin = this->begin();
    auto end = this->end();

    for (;begin != end; ++begin) {
      new(begin) T(std::forward<Args>(args)...);
    }
  }

  ~array() NOEXCEPT {
    auto begin = this->begin();
    auto end = this->end();

    for (; begin != end; ++begin) {
      begin->~T();
    }
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  // before c++14 constexpr member function
  // gets const implicitly
  CONSTEXPR
#endif
  reference operator[](size_t i) NOEXCEPT {
    assert(i < Size);
    return *(begin() + i);
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  // before c++14 constexpr member function
  // gets const implicitly
  CONSTEXPR
#endif
  const_reference operator[](size_t i) const NOEXCEPT {
    return const_cast<array&>(*this)[i];
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  // before c++14 constexpr member function
  // gets const implicitly
  CONSTEXPR
#endif
  reference back() NOEXCEPT {
    return *(end()-1);
  }

  CONSTEXPR const_reference back() const NOEXCEPT {
    return const_cast<array*>(this)->back();
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  // before c++14 constexpr member function
  // gets const implicitly
  CONSTEXPR
#endif
  reference front() NOEXCEPT {
    return *begin();
  }

  CONSTEXPR const_reference front() const NOEXCEPT {
    return const_cast<array*>(this)->front();
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  // before c++14 constexpr member function
  // gets const implicitly
  CONSTEXPR
#endif
  iterator begin() NOEXCEPT {
    return reinterpret_cast<T*>(buffer_t::data);
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  // before c++14 constexpr member function
  // gets const implicitly
  CONSTEXPR
#endif
  iterator end() NOEXCEPT {
    return this->begin() + Size;
  }

  CONSTEXPR const_iterator begin() const NOEXCEPT {
    return const_cast<array*>(this)->begin();
  }

  CONSTEXPR const_iterator end() const NOEXCEPT {
    return const_cast<array*>(this)->end();
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  // before c++14 constexpr member function
  // gets const implicitly
  CONSTEXPR
#endif
  reverse_iterator rbegin() NOEXCEPT {
    return reverse_iterator(end());
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  // before c++14 constexpr member function
  // gets const implicitly
  CONSTEXPR
#endif
  reverse_iterator rend() NOEXCEPT {
    return reverse_iterator(begin());
  }

  CONSTEXPR const_reverse_iterator rbegin() const NOEXCEPT {
    return const_reverse_iterator(end());
  }

  CONSTEXPR const_reverse_iterator rend() const NOEXCEPT {
    return const_reverse_iterator(begin());
  }

  CONSTEXPR size_t size() const NOEXCEPT {
    return Size;
  }

  CONSTEXPR bool empty() const NOEXCEPT {
    return 0 == size();
  }
}; // array

struct bucket_size_t {
  bucket_size_t* next; // next bucket
  size_t offset; // sum of bucket sizes up to but excluding this bucket
  size_t size; // size of this bucket
  size_t index; // bucket index
}; // bucket_size_t

//////////////////////////////////////////////////////////////////////////////
/// @brief compute individual sizes and offsets of exponentially sized buckets
/// @param NumBuckets the number of bucket descriptions to generate
/// @param SkipBits 2^SkipBits is the size of the first bucket, consequently
///        the number of bits from a 'position' value to place into 1st bucket
//////////////////////////////////////////////////////////////////////////////
MSVC_ONLY(__pragma(warning(push)))
MSVC_ONLY(__pragma(warning(disable:4127))) // constexp conditionals are intended to be optimized out
template<size_t NumBuckets, size_t SkipBits>
class bucket_meta {
 public:
  static const std::array<bucket_size_t, NumBuckets>& get() NOEXCEPT {
    static const bucket_meta buckets;
    return buckets.buckets_;
  }

 private:
  bucket_meta() NOEXCEPT {
    if (buckets_.empty()) {
      return;
    }

    buckets_[0].size = 1 << SkipBits;
    buckets_[0].offset = 0;
    buckets_[0].index = 0;

    for (size_t i = 1; i < NumBuckets; ++i) {
      buckets_[i - 1].next = &buckets_[i];
      buckets_[i].offset = buckets_[i - 1].offset + buckets_[i - 1].size;
      buckets_[i].size = buckets_[i - 1].size << 1;
      buckets_[i].index = i;
    }

    // all subsequent buckets_ should have the same meta as the last bucket
    buckets_.back().next = &(buckets_.back());
  }

  std::array<bucket_size_t, NumBuckets> buckets_;
};
MSVC_ONLY(__pragma(warning(pop)))

NS_BEGIN(memory)

template<typename BucketFactory, size_t Size>
class IRESEARCH_API_TEMPLATE bucket_allocator: private util::noncopyable { // noncopyable because of 'pools_' (declaration required by MSVC2017)
 public:
  // number of pools
  static const size_t SIZE = Size;

  typedef typename unbounded_object_pool<BucketFactory>::ptr value_type;

  explicit bucket_allocator(size_t pool_size)
    : pools_(pool_size) {
  }

  value_type allocate(const bucket_size_t& bucket) {
    assert(bucket.index < pools_.size());
    return pools_[bucket.index].emplace(bucket.size);
  }

  void clear() {
    for (auto& pool : pools_) {
      pool.clear();
    }
  }

 private:
  array<unbounded_object_pool<BucketFactory>, Size> pools_;
}; // bucket_allocator

// default stateless allocator
struct default_allocator {
  typedef std::unique_ptr<byte_type[]> value_type;

  value_type allocate(const bucket_size_t& bucket) {
    return irs::memory::make_unique<byte_type[]>(bucket.size);
  }
}; // default_allocator

NS_END // memory

//////////////////////////////////////////////////////////////////////////////
/// @brief a function to calculate the bucket offset of the reqested position
///        for exponentially sized buckets, e.g. as per compute_bucket_meta()
/// @param SkipBits 2^SkipBits is the size of the first bucket, consequently
///        the number of bits from a 'position' value to place into 1st bucket
//////////////////////////////////////////////////////////////////////////////
template<size_t SkipBits>
size_t compute_bucket_offset(size_t position) NOEXCEPT {
  // 63 == 64 bits per size_t - 1 for allignment, +1 == align first value to start of bucket
  return 63 - math::clz64((position >> SkipBits) + 1);
}

template<typename Allocator>
class raw_block_vector_base
    : public compact_ref<0, Allocator>,
      private util::noncopyable {
 public:
  typedef compact_ref<0, Allocator> allocator_ref_t;
  typedef typename allocator_ref_t::type allocator_type;

  struct buffer_t {
    byte_type* data; // pointer at the actual data
    size_t offset; // sum of bucket sizes up to but excluding this buffer
    size_t size; // total buffer size
  };

  explicit raw_block_vector_base(const Allocator& alloc) NOEXCEPT
    : allocator_ref_t(alloc) {
  }

  raw_block_vector_base(raw_block_vector_base&& rhs) NOEXCEPT
    : allocator_ref_t(std::move((allocator_ref_t&)rhs)),
      buffers_(std::move(rhs.buffers_)) {
  }

  FORCE_INLINE size_t buffer_count() const NOEXCEPT {
    return buffers_.size();
  }

  FORCE_INLINE bool empty() const NOEXCEPT {
    return buffers_.empty();
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

  FORCE_INLINE void pop_buffer() {
    buffers_.pop_back();
  }

 protected:
  struct buffer_entry_t : buffer_t, util::noncopyable {
    buffer_entry_t(
        size_t bucket_offset,
        size_t bucket_size,
        typename Allocator::value_type&& ptr) NOEXCEPT
      : ptr(std::move(ptr)) {
      buffer_t::data = this->ptr.get();
      buffer_t::offset = bucket_offset;
      buffer_t::size = bucket_size;
    }

    buffer_entry_t(buffer_entry_t&& other) NOEXCEPT
      : buffer_t(std::move(other)),
        ptr(std::move(other.ptr)) {
    }

    typename Allocator::value_type ptr;
  };

  buffer_t& push_buffer(size_t offset, const bucket_size_t& bucket) {
    auto& allocator = allocator_ref_t::get();
    buffers_.emplace_back(offset, bucket.size, allocator.allocate(bucket));
    return buffers_.back();
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::vector<buffer_entry_t> buffers_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // raw_block_vector_base

//////////////////////////////////////////////////////////////////////////////
/// @brief a container allowing raw access to internal storage, and
///        using an allocation strategy similar to an std::deque
//////////////////////////////////////////////////////////////////////////////
template<
  size_t NumBuckets,
  size_t SkipBits,
  typename Allocator = memory::default_allocator
> class IRESEARCH_API_TEMPLATE raw_block_vector : public raw_block_vector_base<Allocator> {
 public:
  static const size_t NUM_BUCKETS = NumBuckets; // total number of buckets
  static const size_t FIRST_BUCKET_SIZE = 1 << SkipBits; // size of the first bucket

  typedef raw_block_vector_base<Allocator> base_t;
  typedef typename base_t::allocator_ref_t allocator_ref_t;
  typedef typename base_t::allocator_type allocator_type;

  explicit raw_block_vector(const Allocator& alloc /*= Allocator()*/) NOEXCEPT // MSVC fails to build 'shared' if the allocator does not define a no-arg constructor
    : base_t(alloc) {
  }

  raw_block_vector(raw_block_vector&& other) NOEXCEPT
    : base_t(std::move(other)) {
  }

  FORCE_INLINE size_t buffer_offset(size_t position) const NOEXCEPT {
    // non-precomputed bucket size is the same as the last precomputed bucket size
    return position < LAST_BUFFER.offset
      ? compute_bucket_offset<SkipBits>(position)
      : (LAST_BUFFER_ID + (position - LAST_BUFFER.offset) / LAST_BUFFER.size);
  }

  typename base_t::buffer_t& push_buffer() {
    if (base_t::buffers_.size() < META.size()) { // one of the precomputed buckets
      const auto& bucket = META[base_t::buffers_.size()];
      return base_t::push_buffer(bucket.offset, bucket);
    }

    // non-precomputed buckets, offset is the sum of previous buckets
    assert(!base_t::buffers_.empty()); // otherwise do not know what size buckets to create
    const auto& bucket = base_t::buffers_.back(); // most of the meta from last computed bucket
    return base_t::push_buffer(bucket.offset + bucket.size, META.back());
  }

 private:
  static const std::array<bucket_size_t, NumBuckets>& META;
  static const bucket_size_t& LAST_BUFFER;
  static const size_t LAST_BUFFER_ID;
};

template<size_t NumBuckets, size_t SkipBits, typename Allocator>
/*static*/ const std::array<bucket_size_t, NumBuckets>&
raw_block_vector<NumBuckets, SkipBits, Allocator>::META
  = bucket_meta<NumBuckets, SkipBits>::get();

template<size_t NumBuckets, size_t SkipBits, typename Allocator>
/*static*/ const bucket_size_t&
raw_block_vector<NumBuckets, SkipBits, Allocator>::LAST_BUFFER
  = bucket_meta<NumBuckets, SkipBits>::get().back();

template<size_t NumBuckets, size_t SkipBits, typename Allocator>
/*static*/ const size_t
raw_block_vector<NumBuckets, SkipBits, Allocator>::LAST_BUFFER_ID
  = bucket_meta<NumBuckets, SkipBits>::get().size() - 1;

NS_END // container_utils
NS_END // NS_ROOT

#endif