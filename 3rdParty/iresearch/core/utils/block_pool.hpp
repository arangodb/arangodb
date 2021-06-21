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

#ifndef IRESEARCH_BLOCK_POOL_H
#define IRESEARCH_BLOCK_POOL_H

#include <vector>
#include <functional>
#include <algorithm>
#include <memory>
#include <cmath>
#include <iterator> 
#include <cassert>
#include <cstring>

#include "memory.hpp"
#include "ebo.hpp"
#include "bytes_utils.hpp"
#include "misc.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_const_iterator
////////////////////////////////////////////////////////////////////////////////
template<typename ContType>
class block_pool_const_iterator {
  public:
  using container = ContType;
  using block_type = typename container::block_type;

  using iterator_category = std::random_access_iterator_tag;
  using value_type = typename container::value_type;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = ptrdiff_t;
  using const_pointer = const value_type*;
  using const_reference = const value_type&;

  explicit block_pool_const_iterator(const container& pool) noexcept
    : pool_(&pool) {
    reset(pool_->value_count());
  }

  block_pool_const_iterator(const container& pool, size_t offset) noexcept
    : pool_(&pool) {
    reset(offset);
  }

  block_pool_const_iterator& operator++() noexcept {
    seek_relative(1);
    return *this;
  }

  block_pool_const_iterator& operator--() noexcept {
    seek_relative(-1);
    return *this;
  }

  block_pool_const_iterator operator--(int) noexcept {
    block_pool_const_iterator state;
    --*this;
    return state;
  }

  block_pool_const_iterator operator++(int) noexcept {
    block_pool_const_iterator state;
    ++*this;
    return state;
  }

  const_reference operator*() const noexcept { return *pos_; }

  const_reference operator[](difference_type offset) const noexcept {
    assert(block_);

    const auto pos = pos_ + offset;
    if (pos < block_->begin || pos >= block_->end) {
      return parent().at(block_offset() + std::distance(block_->begin, pos));
    }

    return *pos;
  }

  block_pool_const_iterator& operator+=(difference_type offset) noexcept {
    seek_relative(offset);
    return *this;
  }

  block_pool_const_iterator& operator-=(difference_type offset) noexcept {
    seek_relative(offset);
    return *this;
  }

  block_pool_const_iterator operator+(difference_type offset) const noexcept {
    return block_pool_const_iterator(pool_, pool_offset() + offset);
  }

  block_pool_const_iterator operator-(difference_type offset) const noexcept {
    return block_pool_const_iterator(pool_, pool_offset() - offset);
  }

  difference_type operator-(const block_pool_const_iterator& rhs) const noexcept {
    assert(pool_ == rhs.pool_);
    return pool_offset() - rhs.pool_offset();
  }

  bool operator==(const block_pool_const_iterator& rhs) const noexcept {
    assert(pool_ == rhs.pool_);
    return pool_offset() == rhs.pool_offset();
  }

  bool operator!=(const block_pool_const_iterator& rhs) const noexcept {
    return !(*this == rhs);
  }

  bool operator>(const block_pool_const_iterator& rhs) const noexcept {
    assert(pool_ == rhs.pool_);
    return pool_offset() > rhs.pool_offset();
  }

  bool operator<(const block_pool_const_iterator& rhs) const noexcept {
    assert(pool_ == rhs.pool_);
    return pool_offset() < rhs.pool_offset();
  }

  bool operator<=(const block_pool_const_iterator& rhs) const noexcept {
    return !(*this > rhs);
  }

  bool operator>= (const block_pool_const_iterator& rhs) const noexcept {
    return !(*this < rhs);
  }

  bool eof() const noexcept {
    return pool_offset() == parent().value_count();
  }

  const_pointer buffer() const noexcept {
    return pos_;
  }

  size_t remain() const noexcept {
    return parent().block_size() - offset();
  }

  size_t offset() const noexcept {
    assert(block_);
    return std::distance(block_->begin, pos_);
  }

  size_t block_offset() const noexcept { return block_start_; }

  size_t pool_offset() const noexcept { return block_offset() + offset(); }

  void refresh() noexcept {
    const auto pos = this->offset();
    block_ = parent().get_blocks()[block_offset() / block_type::SIZE].get();
    pos_ = block_->begin + pos;
  }

  void reset(size_t offset) noexcept {
    if (offset >= pool_->value_count()) {
      block_start_ = pool_->value_count();
      block_ = &EMPTY_BLOCK;
      pos_ = block_->begin;
      return;
    }

    auto& blocks = parent().get_blocks();
    const size_t idx = offset / block_type::SIZE;
    assert(idx < blocks.size());

    const size_t pos = offset % block_type::SIZE;
    assert(pos < block_type::SIZE);

    block_ = blocks[idx].get();
    block_start_ = block_->start;
    pos_ = block_->begin + pos;
  }

  const container& parent() const noexcept {
    assert(pool_);
    return *pool_;
  }

 protected:
  void seek_relative(difference_type offset) noexcept {
    assert(block_);

    pos_ += offset;
    if (pos_ < block_->begin || pos_ >= block_->end) {
      reset(pool_offset());
    }
  }

 private:
  static block_type EMPTY_BLOCK;

  const container* pool_;
  block_type* block_;
  pointer pos_;
  size_t block_start_;
}; // block_pool_const_iterator

template<typename ContType>
typename ContType::block_type  block_pool_const_iterator<ContType>::EMPTY_BLOCK(0);

template<typename ContType>
block_pool_const_iterator<ContType> operator+(
    size_t offset,
    const block_pool_const_iterator<ContType>& it) noexcept {
  return it + offset;
}

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_iterator
////////////////////////////////////////////////////////////////////////////////
template<typename ContType>
class block_pool_iterator : public block_pool_const_iterator<ContType> {
 public:
  typedef ContType container;
  typedef typename container::block_type block_type;
  typedef block_pool_const_iterator<container> base;
  typedef typename base::pointer pointer;
  typedef typename base::difference_type difference_type;
  typedef typename base::reference reference;

  using base::operator-;
  using base::parent;
  using base::buffer;

  explicit block_pool_iterator(container& pool) noexcept
    : base(pool) {
  }

  block_pool_iterator(container& pool, size_t offset) noexcept
    : base(pool, offset) {
  }

  container& parent() noexcept {
    return const_cast<container&>(base::parent());
  }

  block_pool_iterator& operator++() noexcept {
    return static_cast<block_pool_iterator&>(base::operator++());
  }

  block_pool_iterator& operator--() noexcept {
    return static_cast<block_pool_iterator&>(base::operator--());
  }

  block_pool_iterator operator--(int) noexcept {
    block_pool_iterator state(*this);
    --*this;
    return state;
  }

  block_pool_iterator operator++(int) noexcept {
    block_pool_iterator state(*this);
    ++*this;
    return state;
  }

  reference operator*() noexcept {
    return const_cast<reference>(*static_cast<base&>(*this));
  }

  reference operator[](difference_type offset) noexcept {
    return const_cast<reference>(static_cast<base&>(*this)[offset]);
  }

  block_pool_iterator& operator+=(difference_type offset) noexcept {
    return static_cast<block_pool_iterator&>(base::operator+=(offset));
  }

  block_pool_iterator& operator-=(difference_type offset) noexcept {
    return static_cast<block_pool_iterator&>(base::operator-=(offset));
  }

  block_pool_iterator operator+(difference_type offset) const noexcept {
    return block_pool_iterator(
      const_cast<block_pool_iterator*>(this)->parent(),
      this->pool_offset() + offset
    );
  }

  block_pool_iterator operator-(difference_type offset) const noexcept {
    return block_pool_iterator(
      const_cast<block_pool_iterator*>(this)->parent(),
      this->pool_offset() - offset
    );
  }

  pointer buffer() noexcept { return const_cast<pointer>(base::buffer()); }
}; // block_pool_iterator

template<typename ContType>
block_pool_iterator<ContType> operator+(
    size_t offset,
    const block_pool_iterator<ContType>& it) noexcept {
  return it + offset;
}

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_reader
////////////////////////////////////////////////////////////////////////////////
template<typename ContType>
class block_pool_reader {
 public:
  using container = ContType;
  using block_type = typename container::block_type;
  using const_iterator = typename container::const_iterator;

  using iterator_category = std::input_iterator_tag;
  using value_type = typename container::value_type;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = void;
  using const_pointer = const value_type*;
  using const_reference = const value_type&;

  block_pool_reader(const container& pool, size_t offset) noexcept
    : where_(pool, offset) {
  }

  explicit block_pool_reader(const const_iterator& where) noexcept
    : where_(where) {
  }

  const_reference operator*() const noexcept {
    assert(!where_.eof());
    return *where_;
  }

  const_pointer operator->() const noexcept {
    return &(operator*());
  }

  block_pool_reader& operator++() noexcept {
    assert(!eof());

    ++where_;
    return *this;
  }

  block_pool_reader operator++(int) noexcept {
    assert(!eof());

    block_pool_reader tmp = *this;
    ++where_;
    return tmp;
  }

  bool eof() const noexcept { return where_.pool_offset() == where_.parent().size(); }

  size_t read(pointer b, size_t len) noexcept {
    assert(!eof());
    assert(b != nullptr);

    size_t items_read = 0;
    size_t to_copy = 0;
    const size_t block_size = where_.parent().block_size();

    while (len) {
      if (where_.eof()) {
        break;
      }

      to_copy = std::min(block_size - where_.offset(), len);
      memcpy(b, where_.buffer(), to_copy * sizeof(value_type));

      len -= to_copy;
      where_ += to_copy;
      b += to_copy;
      items_read += to_copy;
    }

    return items_read;
  }

 private:
  const_iterator where_;
}; // block_pool_reader

namespace detail {

struct level {
  size_t next; // next level
  size_t size; // size of the level in bytes
}; // level

constexpr const level LEVELS[] = {
  { 1, 5 }, { 2, 14 },
  { 3, 20 }, { 4, 30 },
  { 5, 40 }, { 6, 40 },
  { 7, 80 }, { 8, 80 },
  { 9, 120 }, { 9, 200 }
};

constexpr const size_t LEVEL_MAX = IRESEARCH_COUNTOF(LEVELS);
constexpr const size_t LEVEL_SIZE_MAX = 200; // FIXME compile time

} // detail

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_sliced_reader_base
////////////////////////////////////////////////////////////////////////////////
template<typename ReaderType, typename ContType>
class block_pool_sliced_reader_base {
 public:
  using reader = ReaderType;
  using container = ContType;
  using block_type = typename container::block_type ;
  using const_iterator = typename container::const_iterator;

  using iterator_category = std::input_iterator_tag;
  using value_type = typename container::value_type;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = void;
  using const_pointer = const value_type*;
  using const_reference = const value_type&;

  const_reference operator*() const noexcept {
    assert(!impl().eof());
    return *where_;
  }

  const_pointer operator->() const noexcept {
    return &(operator*());
  }

  reader& operator++() noexcept {
    impl().next();
    return impl();
  }

  reader operator++(int) noexcept {
    reader tmp = impl();
    impl().next();
    return tmp;
  }

  const_iterator& position() const noexcept { return where_; }

  size_t pool_offset() const noexcept { return where_.pool_offset(); }

  container& parent() noexcept { return where_.parent(); }

  const container& parent() const noexcept { return where_.parent(); }

  size_t read(pointer b, size_t len) noexcept {
    assert(!impl().eof());
    assert(b != nullptr);

    size_t to_copy = 0;
    size_t items_read = 0;

    while (len) {
      to_copy = std::min(len, size_t(left_));
      memcpy(b, where_.buffer(), to_copy * sizeof(value_type));

      b += to_copy;
      where_ += to_copy;
      len -= to_copy;
      left_ -= to_copy;
      items_read += to_copy;

      if (!left_) {
        impl().next_slice();
      }
    }

    return items_read;
  }

 protected:
  explicit block_pool_sliced_reader_base(const container& pool) noexcept
    : where_(pool.end()) {
  }

  block_pool_sliced_reader_base(const container& pool, size_t offset) noexcept
    : where_(pool, offset) {
  }

 private:
  FORCE_INLINE const reader& impl() const noexcept { return static_cast<const reader&>(*this) ;}
  FORCE_INLINE reader& impl() noexcept { return static_cast<reader&>(*this) ;}

  void next() noexcept {
    assert(!impl().eof());
    ++where_;
    --left_;

    if (!left_) {
      impl().next_slice();
    }
  }

 protected:
  const_iterator where_;
  size_t left_{};
}; // block_pool_sliced_reader_base

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_sliced_reader
////////////////////////////////////////////////////////////////////////////////
template<typename ContType>
class block_pool_sliced_reader
    : public block_pool_sliced_reader_base<block_pool_sliced_reader<ContType>, ContType> {
 public:
  typedef ContType container;
  typedef typename container::block_type block_type;
  typedef typename container::const_iterator const_iterator;
  typedef typename container::pointer pointer;
  typedef typename container::const_pointer const_pointer;
  typedef typename container::const_reference const_reference;
  typedef typename container::value_type value_type;

  explicit block_pool_sliced_reader(const container& pool) noexcept;

  block_pool_sliced_reader(const container& pool, size_t offset, size_t end) noexcept;

  inline bool eof() const noexcept;

 private:
  typedef block_pool_sliced_reader_base<block_pool_sliced_reader<ContType>, ContType> base;
  friend class block_pool_sliced_reader_base<block_pool_sliced_reader<ContType>, ContType>;

  void next_slice() noexcept {
    // TODO: check for overflow. max_size = MAX(uint32_t)-address_size-1
    // base case where last slice of pool which does not have address footer
    if (this->where_.pool_offset() + sizeof(uint32_t) >= end_) {
      this->left_ = end_ - this->where_.pool_offset();
    } else {
      level_ = detail::LEVELS[level_].next;

      const size_t next_address = irs::read<uint32_t>(this->where_);

      this->where_.reset(next_address);
      this->left_ = std::min(end_ - this->where_.pool_offset(),
                       detail::LEVELS[level_].size - sizeof(uint32_t));
    }
  }

  void init() noexcept {
    assert(end_ >= 0 && this->where_.pool_offset() <= end_);

    this->left_ = std::min(end_ - this->where_.pool_offset(),
                     detail::LEVELS[level_].size - sizeof(uint32_t));
  }

  size_t level_{};
  size_t end_{};
}; // block_pool_sliced_reader

template<typename ContType>
block_pool_sliced_reader<ContType>::block_pool_sliced_reader(
    const typename block_pool_sliced_reader<ContType>::container& pool
) noexcept
  : base(pool), end_(this->pool_offset()) {
}

template<typename ContType>
block_pool_sliced_reader<ContType>::block_pool_sliced_reader(
    const typename block_pool_sliced_reader<ContType>::container& pool,
    size_t offset,
    size_t end
) noexcept
  : base(pool, offset), end_(end) {
  init();
}

template<typename ContType>
bool block_pool_sliced_reader<ContType>::eof() const noexcept {
  assert(this->where_.pool_offset() <= end_);
  return this->where_.pool_offset() == end_;
}

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_sliced_greedy_reader
////////////////////////////////////////////////////////////////////////////////
template<typename ContType>
class block_pool_sliced_greedy_reader
    : public block_pool_sliced_reader_base<block_pool_sliced_greedy_reader<ContType>, ContType> {
 public:
  typedef ContType container;
  typedef typename container::block_type block_type;
  typedef typename container::const_iterator const_iterator;
  typedef typename container::pointer pointer;
  typedef typename container::const_pointer const_pointer;
  typedef typename container::const_reference const_reference;
  typedef typename container::value_type value_type;

  explicit block_pool_sliced_greedy_reader(const container& pool) noexcept;

  block_pool_sliced_greedy_reader(
    const container& pool,
    size_t slice_offset,
    size_t offset) noexcept;

 private:
  typedef block_pool_sliced_reader_base<block_pool_sliced_greedy_reader<ContType>, ContType> base;
  friend class block_pool_sliced_reader_base<block_pool_sliced_greedy_reader<ContType>, ContType>;

  bool eof() const noexcept {
    // we don't track eof()
    return false;
  }

  void next_slice() noexcept {
    level_ = detail::LEVELS[level_].next; // next level
    const size_t next_address = 1 + irs::read<uint32_t>(this->where_); // +1 for slice header
    this->where_.reset(next_address);
    this->left_ = detail::LEVELS[level_].size - sizeof(uint32_t) - 1;
  }

  void init(size_t offset) noexcept {
    level_ = *this->where_;
    assert(level_ < detail::LEVEL_MAX);
    this->where_ += offset;
    assert(detail::LEVELS[level_].size > offset);
    this->left_ = detail::LEVELS[level_].size - offset - sizeof(uint32_t);
  }

  size_t level_{};
}; // block_pool_sliced_greedy_reader

template<typename ContType>
block_pool_sliced_greedy_reader<ContType>::block_pool_sliced_greedy_reader(
    const typename block_pool_sliced_greedy_reader<ContType>::container& pool) noexcept
  : base(pool) {
}

template<typename ContType>
block_pool_sliced_greedy_reader<ContType>::block_pool_sliced_greedy_reader(
    const typename block_pool_sliced_greedy_reader<ContType>::container& pool,
    size_t slice_offset,
    size_t offset) noexcept
  : base(pool, slice_offset) {
  init(offset);
}

template<typename ContType> class block_pool_sliced_inserter;
template<typename ContType> class block_pool_sliced_greedy_inserter;

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_inserter
////////////////////////////////////////////////////////////////////////////////
template<typename ContType>
class block_pool_inserter {
 public:
  using container = ContType;
  using block_type = typename container::block_type;

  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using pointer = void;
  using reference = void;
  using difference_type = void;

  // intentionally implicit
  block_pool_inserter(const typename container::iterator& where) noexcept
    : where_(where) {
  }

  size_t pool_offset() const noexcept { return where_.pool_offset(); }

  typename container::iterator& position() noexcept { return where_; }

  container& parent() noexcept { return where_.parent(); }

  const container& parent() const noexcept { return where_.parent(); }

  block_pool_inserter& operator=(typename container::const_reference value) {
    resize();
    *where_ = value;
    ++where_;
    return *this;
  }

  block_pool_inserter& operator*() noexcept { return *this; }

  block_pool_inserter& operator++(int) noexcept { return *this; }

  block_pool_inserter& operator++() noexcept { return *this; }

  void write(typename container::const_pointer b, size_t len) {
    assert(b || !len);

    size_t to_copy = 0;

    while (len) {
      resize();

      to_copy = std::min(block_type::SIZE - where_.offset(), len);

      std::memcpy(where_.buffer(), b, to_copy * sizeof(typename container::value_type));
      len -= to_copy;
      where_ += to_copy;
      b += to_copy;
    }
  }

  void seek(size_t offset) {
    if (offset >= where_.parent().value_count()) {
      where_.parent().alloc_buffer();
    }

    where_.reset(offset);
  }

  void skip(size_t offset) { seek(where_.pool_offset() + offset); }

  // returns offset of the beginning of the allocated slice in the pool
  size_t alloc_slice(size_t level = 0) {
    return alloc_first_slice<false>(level);
  }

  // returns offset of the beginning of the allocated slice in the pool
  size_t alloc_greedy_slice(size_t level = 1) {
    assert(level >= 1);
    return alloc_first_slice<true>(level);
  }

 private:
  friend class block_pool_sliced_inserter<container>;
  friend class block_pool_sliced_greedy_inserter<container>;
  friend class block_pool_sliced_greedy_reader<container>;

  void resize() {
    if (where_.eof()) {
      where_.parent().alloc_buffer();
      where_.refresh();
    }
  }

  void alloc_slice_of_size(size_t size) {
    auto& parent = where_.parent();
    auto pool_size = parent.value_count();
    auto slice_start = where_.pool_offset() + size;
    auto next_block_start = where_.block_offset() + block_type::SIZE;

    // if need to increase pool size
    if (slice_start >= pool_size) {
      parent.alloc_buffer();

      if (slice_start == pool_size) {
        where_.refresh();
      } else {
        // do not span slice over 2 blocks, start slice at start of block allocated above
        where_.reset(parent.block_offset(parent.block_count() - 1));
      }
    } else if (slice_start > next_block_start) {
      // can reuse existing pool but slice is not in the current buffer
      // ensure iterator points to the correct buffer in the pool
      where_.reset(next_block_start);
    }

    // initialize slice
    std::memset(where_.buffer(), 0, sizeof(typename container::value_type)*size);
  }

  template<bool Greedy>
  size_t alloc_first_slice(size_t level) {
    assert(level < detail::LEVEL_MAX);
    auto& level_info = detail::LEVELS[level];
    const size_t size = level_info.size;

    alloc_slice_of_size(size); // reserve next slice
    const size_t slice_start = where_.pool_offset();
    if constexpr (Greedy) {
      *where_ = static_cast<byte_type>(level);
    }
    where_ += size;
    assert(level_info.next);
    assert(level_info.next < detail::LEVEL_MAX);
    if constexpr (Greedy) {
      where_[-sizeof(uint32_t)] = static_cast<byte_type>(level_info.next);
    } else {
      where_[-1] = static_cast<byte_type>(level_info.next);
    }
    return slice_start;
  }

  size_t alloc_greedy_slice(typename container::iterator& pos) {
    const byte_type next_level = *pos;
    assert(next_level < detail::LEVEL_MAX);
    const auto& next_level_info = detail::LEVELS[next_level];
    const size_t size = next_level_info.size;

    alloc_slice_of_size(size); // reserve next slice

    // write next address at the end of current slice
    irs::write<uint32_t>(pos, uint32_t(where_.pool_offset()));

    pos = where_;
    ++pos; // move to the next byte after the header
    assert(next_level_info.next);
    assert(next_level_info.next < detail::LEVEL_MAX);
    *where_ = next_level; // write slice header
    where_ += size;
    where_[-sizeof(uint32_t)] = static_cast<byte_type>(next_level_info.next);

    return size - sizeof(uint32_t) - 1; // -1 for slice header
  }

  size_t alloc_slice(typename container::iterator& pos) {
    constexpr const size_t ADDR_OFFSET = sizeof(uint32_t)-1;

    const size_t next_level = *pos;
    assert(next_level < detail::LEVEL_MAX);
    const auto& next_level_info = detail::LEVELS[next_level];
    const size_t size = next_level_info.size;

    alloc_slice_of_size(size); // reserve next slice

    // copy data to new slice
    std::copy(pos - ADDR_OFFSET, pos, where_);

    // write next address at the end of current slice
    {
      // write gets non-const reference. need explicit copy here
      block_pool_inserter it(pos - ADDR_OFFSET);
      irs::write<uint32_t>(it, uint32_t(where_.pool_offset()));
    }

    pos.reset(where_.pool_offset() + ADDR_OFFSET);
    where_ += size;
    assert(next_level_info.next);
    assert(next_level_info.next < detail::LEVEL_MAX);
    where_[-1] = static_cast<byte_type>(next_level_info.next);

    return size - sizeof(uint32_t);
  }

  typename container::iterator where_;
}; // block_pool_inserter

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_sliced_inserter
////////////////////////////////////////////////////////////////////////////////
template<typename ContType>
class block_pool_sliced_inserter {
 public:
  using container = ContType;

  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using pointer = void;
  using reference = void;
  using difference_type = void;

  block_pool_sliced_inserter(
      typename container::inserter& writer,
      const typename container::iterator& where) noexcept
    : where_(where),
      writer_(&writer) {
  }

  block_pool_sliced_inserter(
      typename container::inserter& writer,
      size_t offset) noexcept
    : block_pool_sliced_inserter(
        writer, typename container::iterator(writer.parent(), offset)) {
  }

  size_t pool_offset() const noexcept { return where_.pool_offset(); }

  typename container::iterator& position() noexcept { return where_; }

  container& parent() noexcept { return where_.parent(); }

  block_pool_sliced_inserter& operator=(
      typename container::const_reference value) {
    if (*where_) {
      writer_->alloc_slice(where_);
    }

    *where_ = value;
    ++where_;
    return *this;
  }

  block_pool_sliced_inserter& operator*() noexcept { return *this; }

  block_pool_sliced_inserter& operator++(int) noexcept { return *this; }

  block_pool_sliced_inserter& operator++() noexcept { return *this; }

  // MSVC 2017.3 through 2017.9 incorectly count offsets if this function is inlined during optimization
  // MSVC 2017.2 and below work correctly for both debug and release
  MSVC2017_3456789_MSVC2019_OPTIMIZED_WORKAROUND(__declspec(noinline))
  void write(typename container::const_pointer b, size_t len) {
    // find end of the slice
    for (; 0 == *where_ && len; --len, ++where_, ++b) {
      *where_ = *b;
    }

    // chunked copy
    while (len) {
      const size_t size = writer_->alloc_slice(where_);
      const size_t to_copy = std::min(size, len);
      std::memcpy(where_.buffer(), b, to_copy * sizeof(typename container::value_type));
      where_ += to_copy;
      len -= to_copy;
      b += to_copy;
    }
  }

 private:
  typename container::iterator where_;
  typename container::inserter* writer_;
}; // block_pool_sliced_inserter

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool_sliced_greedy_inserter
////////////////////////////////////////////////////////////////////////////////
template<typename ContType>
class block_pool_sliced_greedy_inserter {
 public:
  using container = ContType;

  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using pointer = void;
  using reference = void;
  using difference_type = void;

  block_pool_sliced_greedy_inserter(
      typename container::inserter& writer,
      size_t slice_offset,
      size_t offset) noexcept
    : slice_offset_(slice_offset),
      where_(writer.parent(), offset + slice_offset),
      writer_(&writer) {
    assert(!*where_); // we're not at the address part
  }

  size_t pool_offset() const noexcept { return where_.pool_offset(); }

  size_t slice_offset() const noexcept { return slice_offset_; }

  typename container::iterator& position() noexcept { return where_; }

  container& parent() noexcept { return where_.parent(); }

  // At least MSVC 2017.9 incorectly process increment if this function is inlined during optimization
  // Other MSVC 2017 versions could have similar issue
  MSVC2017_3456789_MSVC2019_OPTIMIZED_WORKAROUND(__declspec(noinline))
  block_pool_sliced_greedy_inserter& operator=(typename container::const_reference value) {
    assert(!*where_); // we're not at the address part

    *where_ = value;
    ++where_;

    if (*where_) {
      alloc_slice();
    }

    assert(!*where_); // we're not at the address part
    return *this;
  }

  block_pool_sliced_greedy_inserter& operator*() noexcept { return *this; }

  block_pool_sliced_greedy_inserter& operator++(int) noexcept { return *this; }

  block_pool_sliced_greedy_inserter& operator++() noexcept { return *this; }

  // MSVC 2017.3 through 2017.9 incorectly count offsets if this function is inlined during optimization
  // MSVC 2017.2 and below work correctly for both debug and release
  MSVC2017_3456789_MSVC2019_OPTIMIZED_WORKAROUND(__declspec(noinline))
  void write(typename container::const_pointer b, size_t len) {
    assert(!*where_); // we're not at the address part

    //FIXME optimize loop since we're aware of current slice
    // find end of the slice
    for (; 0 == *where_ && len; --len, ++where_, ++b) {
      *where_ = *b;
    }

    // chunked copy
    while (len) {
      const size_t size = alloc_slice();
      const size_t to_copy = std::min(size, len);
      std::memcpy(where_.buffer(), b, to_copy * sizeof(typename container::value_type));
      where_ += to_copy;
      len -= to_copy;
      b += to_copy;
    }

    if (*where_) {
      alloc_slice();
    }

    assert(!*where_); // we're not at the address part
  }

 private:
  size_t alloc_slice() {
    assert(*where_);
    const size_t size = writer_->alloc_greedy_slice(where_);
    slice_offset_ = where_.pool_offset() - 1; // -1 for header
    return size;
  }

  size_t slice_offset_;
  typename container::iterator where_;
  typename container::inserter* writer_;
}; // block_pool_sliced_greedy_inserter

////////////////////////////////////////////////////////////////////////////////
/// @struct proxy_block_t
////////////////////////////////////////////////////////////////////////////////
template<typename T, size_t Size>
struct proxy_block_t {
  typedef std::shared_ptr<proxy_block_t> ptr;
  typedef T value_type;

  static const size_t SIZE = Size;

  constexpr explicit proxy_block_t(size_t start) noexcept
    : begin{ 0 },
      end(begin + SIZE),
      start(start) {
  }

  value_type begin[SIZE]; // begin of valid bytes
  value_type* end; // end of valid bytes
  size_t start; // where block starts
}; // proxy_block_t

////////////////////////////////////////////////////////////////////////////////
/// @class block_pool
////////////////////////////////////////////////////////////////////////////////
template<typename T, size_t BlockSize, typename AllocType = std::allocator<T>>
class block_pool {
 public:
  typedef proxy_block_t<T, BlockSize> block_type;
  typedef AllocType allocator;
  typedef typename allocator::value_type value_type;
  typedef typename allocator::reference reference;
  typedef typename allocator::const_reference const_reference;
  typedef typename allocator::pointer pointer;
  typedef typename allocator::const_pointer const_pointer;
  typedef block_pool<value_type, BlockSize, allocator> my_type;

  typedef block_pool_iterator<my_type> iterator;
  typedef block_pool_const_iterator<my_type> const_iterator;

  typedef block_pool_reader<my_type> reader;
  typedef block_pool_sliced_reader<my_type> sliced_reader;

  typedef block_pool_inserter<my_type> inserter;
  typedef block_pool_sliced_inserter<my_type> sliced_inserter;

  typedef block_pool_sliced_greedy_inserter<my_type> sliced_greedy_inserter;
  typedef block_pool_sliced_greedy_reader<my_type> sliced_greedy_reader;

  explicit block_pool(const allocator& alloc = allocator())
    : rep_(blocks_t(block_ptr_allocator(alloc)), alloc) {
    static_assert(block_type::SIZE > 0, "block_type::SIZE == 0");
  }

  void alloc_buffer(size_t count = 1) {
    proxy_allocator proxy_alloc(get_allocator());
    auto& blocks = get_blocks();

    while (count--) {
      blocks.emplace_back(
        std::allocate_shared<block_type>(proxy_alloc, blocks.size() * block_type::SIZE)
      );
    }
  }

  size_t block_count() const noexcept { return get_blocks().size(); }

  size_t value_count() const noexcept { return block_type::SIZE * block_count(); }

  size_t size() const noexcept { return sizeof(value_type) * value_count(); }

  iterator write(iterator where, value_type b) {
    if (where.eof()) {
      alloc_buffer();
      where.refresh();
    }

    *where = b;
    return ++where;
  }

  iterator write(iterator where, const_pointer b, size_t len) {
    assert(b);

    size_t to_copy = 0;
    while (len) {
      if (where.eof()) {
        alloc_buffer();
        where.refresh();
      }

      to_copy = std::min(block_type::SIZE - where.offset(), len);

      memcpy(where.buffer(), b, to_copy * sizeof(value_type));
      len -= to_copy;
      where += to_copy;
      b += to_copy;
    }

    return where;
  }

  iterator read(iterator where, pointer b) noexcept {
    if (where.eof()) {
      return end();
    }

    *b = *where;
    return ++where;
  }

  iterator read(iterator where, pointer b, size_t len) const noexcept {
    assert(b);

    size_t to_copy = 0;

    while (len) {

      if (where.eof()) {
        break;
      }

      to_copy = std::min(block_type::SIZE - where.offset(), len);
      memcpy(b, where.buffer(), to_copy * sizeof(value_type));

      len -= to_copy;
      where += to_copy;
      b += to_copy;
    }

    return where;
  }

  void clear() { get_blocks().clear(); }

  const_reference at(size_t offset) const noexcept {
    return const_cast<block_pool*>(this)->at(offset);
  }

  reference at(size_t offset) noexcept {
    assert(offset < this->value_count());

    const size_t idx = offset / block_type::SIZE;
    const size_t pos = offset % block_type::SIZE;

    return *(get_blocks()[idx]->begin + pos);
  }

  reference operator[](size_t offset) noexcept { return at(offset); }

  const_reference operator[](size_t offset) const noexcept {
    return at(offset);
  }

  const_iterator seek(size_t offset) const noexcept {
    return const_iterator(*const_cast<block_pool*>(this), offset);
  }

  const_iterator begin() const noexcept {
    return const_iterator(*const_cast<block_pool*>(this), 0);
  }

  const_iterator end() const noexcept {
    return const_iterator(*const_cast<block_pool*>(this));
  }

  iterator seek(size_t offset) noexcept {
    return iterator(*this, offset);
  }

  iterator begin() noexcept { return iterator(*this, 0); }

  iterator end() noexcept { return iterator(*this); }

  pointer buffer(size_t i) noexcept {
    assert(i < block_count());
    return get_blocks()[i];
  }

  const_pointer buffer(size_t i) const noexcept {
    assert(i < block_count());
    return get_blocks()[i];
  }

  size_t block_offset(size_t i) const noexcept {
    assert(i < block_count());
    return block_type::SIZE * i;
  }

 private:
  friend iterator;
  friend const_iterator;
  
  typedef typename block_type::ptr block_ptr;
  typedef typename std::allocator_traits<allocator>::template rebind_alloc<block_type> proxy_allocator;
  typedef typename std::allocator_traits<allocator>::template rebind_alloc<block_ptr> block_ptr_allocator;
  typedef std::vector<block_ptr, block_ptr_allocator> blocks_t;

  const blocks_t& get_blocks() const noexcept { return rep_.first(); }
  blocks_t& get_blocks() noexcept { return rep_.first(); }
  const allocator& get_allocator() const noexcept { return rep_.second(); }
  allocator& get_allocator() noexcept { return rep_.second(); }

  compact_pair<blocks_t, allocator> rep_;
}; // block_pool

}

#endif
