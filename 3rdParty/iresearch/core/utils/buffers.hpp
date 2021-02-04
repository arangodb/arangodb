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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_BUFFERS_H
#define IRESEARCH_BUFFERS_H

#include "shared.hpp"
#include "string.hpp"
#include "utils/math_utils.hpp"

// -------------------------------------------------------------------
// @brief data buffers used internally and not exported via public API
// -------------------------------------------------------------------

namespace iresearch {

// -------------------------------------------------------------------
// basic_allocator
// -------------------------------------------------------------------

template<
  typename Elem,
  typename Alloc = std::allocator <Elem>
> class basic_allocator: compact<0, Alloc> {
 public:
  typedef compact<0, Alloc> allocator_t;
  typedef typename allocator_t::type allocator_type;
  typedef typename allocator_type::pointer pointer;

  basic_allocator() = default;
  basic_allocator(const basic_allocator&) = default;
  basic_allocator(basic_allocator&& rhs) noexcept
    : allocator_t(std::move(rhs)) { }

  basic_allocator& operator=(const basic_allocator&) = default;
  basic_allocator& operator=(basic_allocator&& rhs) noexcept {
    if (this != &rhs) {
      allocator_t::operator=(std::move(rhs));
    }
    return *this;
  }

  pointer allocate(size_t len) {
    static_assert(std::is_nothrow_move_constructible_v<std::remove_pointer_t<decltype(this)>>);
    return allocator_t::get().allocate(len);
  }
  void deallocate(pointer ptr, size_t size) {
    allocator_t::get().deallocate(ptr, size);
  }
  const typename allocator_t::type& get_allocator() const {
    return allocator_t::get();
  }
};

template<>
class basic_allocator<char, std::allocator<char>>:
  compact<0, std::allocator<char>> {
 public:
  typedef compact<0, std::allocator<char>> allocator_t;
  typedef allocator_t::type allocator_type;
  typedef allocator_type::pointer pointer;

  basic_allocator() = default;
  basic_allocator(const basic_allocator&) = default;
  basic_allocator(basic_allocator&& rhs) noexcept
    : allocator_t(std::move(rhs)) { }

  basic_allocator& operator=(const basic_allocator&) = default;
  basic_allocator& operator=(basic_allocator&& rhs) noexcept {
    if (this != &rhs) {
      allocator_t::operator=(std::move(rhs));
    }
    return *this;
  }

  pointer allocate(size_t size) {
    auto ptr = allocator_t::get().allocate(size + 1);
    ptr[size] = 0;
    return ptr;
  }
  void deallocate(pointer ptr, size_t size) {
    allocator_t::get().deallocate(ptr, size + 1);
  }
  const allocator_t::type& get_allocator() const { return allocator_t::get(); }
};

// -------------------------------------------------------------------
// basic_const_str
// -------------------------------------------------------------------

inline size_t oversize(
    size_t chunk_size, size_t size, size_t min_size
) noexcept {
  assert(chunk_size);
  assert(min_size > size);

  typedef math::math_traits<size_t> math_traits;

  return size + math_traits::ceil(min_size-size, chunk_size);
}

// -------------------------------------------------------------------
// basic_str_builder
// -------------------------------------------------------------------

template<
  typename Elem,
  typename Traits = std::char_traits<Elem>,
  typename Alloc = std::allocator<Elem>
> class basic_str_builder: public basic_string_ref<Elem, Traits> {
 public:
  typedef basic_string_ref<Elem, Traits> ref_type;
  typedef basic_allocator <Elem, Alloc> allocator_type;
  typedef typename ref_type::traits_type traits_type;
  typedef typename traits_type::char_type char_type;

  static const size_t DEF_CAPACITY = 32;
  static const size_t DEF_ALIGN = 8;

  explicit basic_str_builder(
    size_t capacity = DEF_CAPACITY, const allocator_type& alloc = allocator_type()
  ): rep_(capacity, alloc) {
    if (capacity) {
      this->data_ = allocator().allocate(capacity);
    }
  }

  explicit basic_str_builder(
    const ref_type& ref, const allocator_type& alloc = allocator_type()
  ): rep_(0, alloc) {
    *this += ref;
  }

  basic_str_builder(basic_str_builder&& rhs) noexcept
    : ref_type(rhs.data_, rhs.size_), rep_(std::move(rhs.rep_)) {
    rhs.data_ = nullptr;
    rhs.size_ = 0;
  }

  basic_str_builder(const basic_str_builder& rhs):
    ref_type(nullptr, rhs.size_), rep_(rhs.rep_) {
    if (capacity()) {
      this->data_ = allocator().allocate(capacity());
      traits_type::copy(data(), rhs.data_, rhs.size_);
    }
  }

  basic_str_builder& operator=(const basic_str_builder& rhs) {
    if (this != &rhs) {
      oversize(rhs.capacity());

      if (capacity()) {
        traits_type::copy(data(), rhs.data_, this->size_ = rhs.size_);
      }
    }

    return *this;
  }

  basic_str_builder& operator=(basic_str_builder&& rhs) noexcept {
    if (this != &rhs) {
      this->data_ = rhs.data_;
      rhs.data_ = nullptr;
      this->size_ = rhs.size_;
      rhs.size_ = 0;
      rep_ = std::move(rhs.rep_);
    }

    return *this;
  }

  virtual ~basic_str_builder() {
    destroy();
  }

  char_type& at(size_t i) {
    assert(i < capacity());
    return data()[i];
  }

  char_type& operator[](size_t i) {
    assert(i < capacity());
    return data()[i];
  }

  const char_type& operator[](size_t i) const {
    assert(i < capacity());
    return this->data_[i];
  }

  char_type* data() { return const_cast<char_type*>(this->data_); }
  size_t capacity() const { return rep_.first(); }
  size_t remain() const { return capacity() - this->size(); }

  void reset(size_t size = 0) {
    assert(size <= capacity());
    this->size_ = size;
  }

  basic_str_builder& append(
    const char_type* b, size_t size, size_t align = DEF_ALIGN
  ) {
    oversize(this->size() + size, align);
    traits_type::copy(data() + this->size(), b, size);
    this->size_ += size;
    return *this;
  }

  basic_str_builder& append(
    const basic_string_ref<char_type>& ref, size_t align = DEF_ALIGN
  ) {
    return append(ref.c_str(), ref.size(), align);
  }

  basic_str_builder& append(char_type b, size_t align = DEF_ALIGN) {
    oversize(this->size() + 1, align);
    data()[this->size()] = b;
    ++this->size_;
    return *this;
  }

  inline basic_str_builder& operator=(const basic_string_ref<char_type>& ref) {
    reset();
    return (*this += ref);
  }

  inline basic_str_builder& operator+=(char_type b) {
    return append(b);
  }

  inline basic_str_builder& operator+=(
    const basic_string_ref<char_type>& ref
  ) {
    return append(ref.c_str(), ref.size());
  }

  inline void oversize(size_t minsize, size_t chunksize = DEF_ALIGN) {
    if (minsize > capacity()) {
      reserve(irs::oversize(chunksize, capacity(), minsize));
    }
  }

  inline size_t max_size() const noexcept {
    const size_t size = allocator().max_size();
    return (size <= 1U ? 1U : size - 1);
  }

  void reserve(size_t size) {
    assert(this->capacity() >= this->size());

    if (size > capacity()) {
      char_type* newdata = allocator().allocate(size);
      traits_type::copy(newdata, this->data_, this->size());
      destroy();
      this->data_ = newdata;
      capacity(size);
    }
  }

 private:
  void capacity(size_t capacity) { rep_.first() = capacity; }

  inline const allocator_type& allocator() const {
    return rep_.second();
  }

  inline allocator_type& allocator() {
    return rep_.second();
  }

  inline void destroy() noexcept {
    allocator().deallocate(data(), capacity());
  }

  compact_pair<size_t, allocator_type> rep_;
};

typedef basic_str_builder<byte_type> bytes_builder;
typedef basic_str_builder<char> string_builder;

// -------------------------------------------------------------------
// basic_string_builder
// the basic_string_ref points to valid data portion of the buffer
// the internal buffer size >= basic_string_ref size
// -------------------------------------------------------------------

template<
  typename Elem,
  typename Traits = std::char_traits<Elem>,
  typename Alloc = std::allocator<Elem>
> class basic_string_builder: public basic_string_ref<Elem, Traits> {
 public:
  typedef basic_string_ref<Elem, Traits> ref_type;
  typedef basic_allocator<Elem, Alloc> allocator_type;
  typedef typename ref_type::traits_type traits_type;
  typedef typename traits_type::char_type char_type;
  typedef std::basic_string<Elem, Traits, Alloc> string_type;

  // 31 == 32 - 1: because std::basic_string reserves a \0 at the end
  static const size_t DEF_CAPACITY = 31;

  explicit basic_string_builder(size_t capacity = DEF_CAPACITY) {
    reserve(capacity);
    this->data_ = buf_.data();
    this->size_ = 0;
  }

  explicit basic_string_builder(const ref_type& ref) {
    *this = ref;
  }

  explicit basic_string_builder(const string_type& data) {
    *this = data;
  }

  explicit basic_string_builder(basic_string_builder&& other) noexcept
    : buf_(std::move(other.buf_)) {
    this->data_ = buf_.data();
    this->size_ = other.size();
    other.resize();
  }

  explicit basic_string_builder(string_type&& data):
    buf_(std::move(data)) {
    this->data_ = buf_.data();
    this->size_ = buf_.size();
  }

  virtual ~basic_string_builder() = default;

  operator string_type() const { return buf_.substr(0, this->size()); }

  basic_string_builder& operator=(const ref_type& ref) {
    if (this != &ref) {
      reserve(ref.size());
      buf_.replace(0, ref.size(), ref.c_str(), ref.size());
      this->data_ = buf_.data();
      this->size_ = ref.size();
    }

    return *this;
  }

  basic_string_builder& operator=(const string_type& data) {
    if (&buf_ != &data) {
      reserve(data.size());
      buf_.replace(0, data.size(), data);
      this->data_ = buf_.data();
      this->size_ = data.size();
    }

    return *this;
  }

  basic_string_builder& operator=(basic_string_builder&& other) noexcept {
    if (this != &other) {
      buf_ = std::move(other.buf_);
      this->data_ = buf_.data();
      this->size_ = other.size();
      other.resize();
    }

    return *this;
  }

  inline basic_string_builder& operator+=(char_type ch) {
    return append(ch);
  }

  inline basic_string_builder& operator+=(const ref_type& ref) {
    return append(ref);
  }

  char_type& operator[](size_t i) { return buf_[i]; }

  const char_type& operator[](size_t i) const { return buf_[i]; }

  basic_string_builder& append(char_type ch) {
    reserve(this->size() + 1);
    buf_.replace(this->size(), 1, 1, ch);
    ++this->size_;
    return *this;
  }

  basic_string_builder& append(const char_type* data, size_t size) {
    reserve(this->size() + size);
    buf_.replace(this->size(), size, data, size);
    this->size_ += size;
    return *this;
  }

  basic_string_builder& append(const ref_type& ref) {
    return append(ref.c_str(), ref.size());
  }

  char_type& at(size_t i) { return buf_.at(i); }

  size_t capacity() const { return buf_.size(); } // buf_ always kept at capacity

  char_type* data() { return &(buf_[0]); }

  inline size_t max_size() const noexcept { return buf_.max_size(); }

  size_t remain() const { return capacity() - this->size(); }

  void reserve(size_t size) {
    if (size > buf_.size()) {
      oversize(buf_, size);
    }
  }

  void resize(size_t size = 0) {
    reserve(size);
    this->data_ = buf_.data();
    this->size_ = size;
  }

 private:
  string_type buf_;
};

typedef basic_string_builder<byte_type> bstring_builder; // byte string
typedef basic_string_builder<char> cstring_builder;      // char string

}

#endif
