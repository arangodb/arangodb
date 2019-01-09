// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: inlined_vector.h
// -----------------------------------------------------------------------------
//
// This header file contains the declaration and definition of an "inlined
// vector" which behaves in an equivalent fashion to a `std::vector`, except
// that storage for small sequences of the vector are provided inline without
// requiring any heap allocation.

// An `absl::InlinedVector<T,N>` specifies the size N at which to inline as one
// of its template parameters. Vectors of length <= N are provided inline.
// Typically N is very small (e.g., 4) so that sequences that are expected to be
// short do not require allocations.

// An `absl::InlinedVector` does not usually require a specific allocator; if
// the inlined vector grows beyond its initial constraints, it will need to
// allocate (as any normal `std::vector` would) and it will generally use the
// default allocator in that case; optionally, a custom allocator may be
// specified using an `absl::InlinedVector<T,N,A>` construction.

#ifndef S2_THIRD_PARTY_ABSL_CONTAINER_INLINED_VECTOR_H_
#define S2_THIRD_PARTY_ABSL_CONTAINER_INLINED_VECTOR_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

#include "s2/third_party/absl/algorithm/algorithm.h"
#include "s2/third_party/absl/base/internal/throw_delegate.h"
#include "s2/third_party/absl/base/optimization.h"
#include "s2/third_party/absl/base/port.h"
#include "s2/third_party/absl/memory/memory.h"


namespace absl {

// -----------------------------------------------------------------------------
// InlinedVector
// -----------------------------------------------------------------------------
//
// An `absl::InlinedVector` is designed to be a drop-in replacement for
// `std::vector` for use cases where the vector's size is sufficiently small
// that it can be inlined. If the inlined vector does grow beyond its estimated
// size, it will trigger an initial allocation on the heap, and will behave as a
// `std:vector`. The API of the `absl::InlinedVector` within this file is
// designed to cover the same API footprint as covered by `std::vector`.
template <typename T, size_t N, typename A = std::allocator<T> >
class InlinedVector {
  using AllocatorTraits = std::allocator_traits<A>;

 public:
  using allocator_type = A;
  using value_type = typename allocator_type::value_type;
  using pointer = typename allocator_type::pointer;
  using const_pointer = typename allocator_type::const_pointer;
  using reference = typename allocator_type::reference;
  using const_reference = typename allocator_type::const_reference;
  using size_type = typename allocator_type::size_type;
  using difference_type = typename allocator_type::difference_type;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  InlinedVector() noexcept(noexcept(allocator_type()))
      : allocator_and_tag_(allocator_type()) {}

  explicit InlinedVector(const allocator_type& alloc) noexcept
      : allocator_and_tag_(alloc) {}

  // Create a vector with n copies of value_type().
  explicit InlinedVector(size_type n) : allocator_and_tag_(allocator_type()) {
    InitAssign(n);
  }

  // Create a vector with n copies of elem
  InlinedVector(size_type n, const value_type& elem,
                const allocator_type& alloc = allocator_type())
      : allocator_and_tag_(alloc) {
    InitAssign(n, elem);
  }

  // Create and initialize with the elements [first .. last).
  // The unused enable_if argument restricts this constructor so that it is
  // elided when value_type is an integral type.  This prevents ambiguous
  // interpretation between a call to this constructor with two integral
  // arguments and a call to the preceding (n, elem) constructor.
  template <typename InputIterator>
  InlinedVector(
      InputIterator first, InputIterator last,
      const allocator_type& alloc = allocator_type(),
      typename std::enable_if<!std::is_integral<InputIterator>::value>::type* =
          nullptr)
      : allocator_and_tag_(alloc) {
    AppendRange(first, last);
  }

  InlinedVector(std::initializer_list<value_type> init,
                const allocator_type& alloc = allocator_type())
      : allocator_and_tag_(alloc) {
    AppendRange(init.begin(), init.end());
  }

  InlinedVector(const InlinedVector& v);
  InlinedVector(const InlinedVector& v, const allocator_type& alloc);

  // This move constructor does not allocate and only moves the underlying
  // objects, so its `noexcept` specification depends on whether moving the
  // underlying objects can throw or not. We assume
  //  a) move constructors should only throw due to allocation failure and
  //  b) if `value_type`'s move constructor allocates, it uses the same
  //     allocation function as the `InlinedVector`'s allocator, so the move
  //     constructor is non-throwing if the allocator is non-throwing or
  //     `value_type`'s move constructor is specified as `noexcept`.
  InlinedVector(InlinedVector&& v) noexcept(
      absl::allocator_is_nothrow<allocator_type>::value ||
      std::is_nothrow_move_constructible<value_type>::value);

  // This move constructor allocates and also moves the underlying objects, so
  // its `noexcept` specification depends on whether the allocation can throw
  // and whether moving the underlying objects can throw. Based on the same
  // assumptions above, the `noexcept` specification is dominated by whether the
  // allocation can throw regardless of whether `value_type`'s move constructor
  // is specified as `noexcept`.
  InlinedVector(InlinedVector&& v, const allocator_type& alloc) noexcept(
      absl::allocator_is_nothrow<allocator_type>::value);

  ~InlinedVector() { clear(); }

  InlinedVector& operator=(const InlinedVector& v) {
    if (this == &v) {
      return *this;
    }
    // Optimized to avoid reallocation.
    // Prefer reassignment to copy construction for elements.
    if (size() < v.size()) {  // grow
      reserve(v.size());
      std::copy(v.begin(), v.begin() + size(), begin());
      std::copy(v.begin() + size(), v.end(), std::back_inserter(*this));
    } else {  // maybe shrink
      erase(begin() + v.size(), end());
      std::copy(v.begin(), v.end(), begin());
    }
    return *this;
  }

  InlinedVector& operator=(InlinedVector&& v) {
    if (this == &v) {
      return *this;
    }
    if (v.allocated()) {
      clear();
      tag().set_allocated_size(v.size());
      init_allocation(v.allocation());
      v.tag() = Tag();
    } else {
      if (allocated()) clear();
      // Both are inlined now.
      if (size() < v.size()) {
        auto mid = std::make_move_iterator(v.begin() + size());
        std::copy(std::make_move_iterator(v.begin()), mid, begin());
        UninitializedCopy(mid, std::make_move_iterator(v.end()), end());
      } else {
        auto new_end = std::copy(std::make_move_iterator(v.begin()),
                                 std::make_move_iterator(v.end()), begin());
        Destroy(new_end, end());
      }
      tag().set_inline_size(v.size());
    }
    return *this;
  }

  InlinedVector& operator=(std::initializer_list<value_type> init) {
    AssignRange(init.begin(), init.end());
    return *this;
  }

  // InlinedVector::assign()
  //
  // Replaces the contents of the inlined vector with copies of those in the
  // iterator range [first, last).
  template <typename InputIterator>
  void assign(
      InputIterator first, InputIterator last,
      typename std::enable_if<!std::is_integral<InputIterator>::value>::type* =
          nullptr) {
    AssignRange(first, last);
  }

  // Overload of `InlinedVector::assign()` to take values from elements of an
  // initializer list
  void assign(std::initializer_list<value_type> init) {
    AssignRange(init.begin(), init.end());
  }

  // Overload of `InlinedVector::assign()` to replace the first `n` elements of
  // the inlined vector with `elem` values.
  void assign(size_type n, const value_type& elem) {
    if (n <= size()) {  // Possibly shrink
      std::fill_n(begin(), n, elem);
      erase(begin() + n, end());
      return;
    }
    // Grow
    reserve(n);
    std::fill_n(begin(), size(), elem);
    if (allocated()) {
      UninitializedFill(allocated_space() + size(), allocated_space() + n,
                        elem);
      tag().set_allocated_size(n);
    } else {
      UninitializedFill(inlined_space() + size(), inlined_space() + n, elem);
      tag().set_inline_size(n);
    }
  }

  // InlinedVector::size()
  //
  // Returns the number of elements in the inlined vector.
  size_type size() const noexcept { return tag().size(); }

  // InlinedVector::empty()
  //
  // Checks if the inlined vector has no elements.
  bool empty() const noexcept { return (size() == 0); }

  // InlinedVector::capacity()
  //
  // Returns the number of elements that can be stored in an inlined vector
  // without requiring a reallocation of underlying memory. Note that for
  // most inlined vectors, `capacity()` should equal its initial size `N`; for
  // inlined vectors which exceed this capacity, they will no longer be inlined,
  // and `capacity()` will equal its capacity on the allocated heap.
  size_type capacity() const noexcept {
    return allocated() ? allocation().capacity() : N;
  }

  // InlinedVector::max_size()
  //
  // Returns the maximum number of elements the vector can hold.
  size_type max_size() const noexcept {
    // One bit of the size storage is used to indicate whether the inlined
    // vector is allocated; as a result, the maximum size of the container that
    // we can express is half of the max for our size type.
    return std::numeric_limits<size_type>::max() / 2;
  }

  // InlinedVector::data()
  //
  // Returns a const T* pointer to elements of the inlined vector. This pointer
  // can be used to access (but not modify) the contained elements.
  // Only results within the range `[0,size())` are defined.
  const_pointer data() const noexcept {
    return allocated() ? allocated_space() : inlined_space();
  }

  // Overload of InlinedVector::data() to return a T* pointer to elements of the
  // inlined vector. This pointer can be used to access and modify the contained
  // elements.
  pointer data() noexcept {
    return allocated() ? allocated_space() : inlined_space();
  }

  // InlinedVector::clear()
  //
  // Removes all elements from the inlined vector.
  void clear() noexcept {
    size_type s = size();
    if (allocated()) {
      Destroy(allocated_space(), allocated_space() + s);
      allocation().Dealloc(allocator());
    } else if (s != 0) {  // do nothing for empty vectors
      Destroy(inlined_space(), inlined_space() + s);
    }
    tag() = Tag();
  }

  // InlinedVector::at()
  //
  // Returns the ith element of an inlined vector.
  const value_type& at(size_type i) const {
    if (ABSL_PREDICT_FALSE(i >= size())) {
      base_internal::ThrowStdOutOfRange(
          "InlinedVector::at failed bounds check");
    }
    return data()[i];
  }

  // InlinedVector::operator[]
  //
  // Returns the ith element of an inlined vector using the array operator.
  const value_type& operator[](size_type i) const {
    assert(i < size());
    return data()[i];
  }

  // Overload of InlinedVector::at() to return the ith element of an inlined
  // vector.
  value_type& at(size_type i) {
    if (i >= size()) {
      base_internal::ThrowStdOutOfRange(
          "InlinedVector::at failed bounds check");
    }
    return data()[i];
  }

  // Overload of InlinedVector::operator[] to return the ith element of an
  // inlined vector.
  value_type& operator[](size_type i) {
    assert(i < size());
    return data()[i];
  }

  // InlinedVector::back()
  //
  // Returns a reference to the last element of an inlined vector.
  value_type& back() {
    assert(!empty());
    return at(size() - 1);
  }

  // Overload of InlinedVector::back() returns a reference to the last element
  // of an inlined vector of const values.
  const value_type& back() const {
    assert(!empty());
    return at(size() - 1);
  }

  // InlinedVector::front()
  //
  // Returns a reference to the first element of an inlined vector.
  value_type& front() {
    assert(!empty());
    return at(0);
  }

  // Overload of InlinedVector::front() returns a reference to the first element
  // of an inlined vector of const values.
  const value_type& front() const {
    assert(!empty());
    return at(0);
  }

  // InlinedVector::emplace_back()
  //
  // Constructs and appends an object to the inlined vector.
  //
  // Returns a reference to the inserted element.
  template <typename... Args>
  value_type& emplace_back(Args&&... args) {
    size_type s = size();
    assert(s <= capacity());
    if (ABSL_PREDICT_FALSE(s == capacity())) {
      return GrowAndEmplaceBack(std::forward<Args>(args)...);
    }
    assert(s < capacity());

    value_type* space;
    if (allocated()) {
      tag().set_allocated_size(s + 1);
      space = allocated_space();
    } else {
      tag().set_inline_size(s + 1);
      space = inlined_space();
    }
    return Construct(space + s, std::forward<Args>(args)...);
  }

  // InlinedVector::push_back()
  //
  // Appends a const element to the inlined vector.
  void push_back(const value_type& t) { emplace_back(t); }

  // Overload of InlinedVector::push_back() to append a move-only element to the
  // inlined vector.
  void push_back(value_type&& t) { emplace_back(std::move(t)); }

  // InlinedVector::pop_back()
  //
  // Removes the last element (which is destroyed) in the inlined vector.
  void pop_back() {
    assert(!empty());
    size_type s = size();
    if (allocated()) {
      Destroy(allocated_space() + s - 1, allocated_space() + s);
      tag().set_allocated_size(s - 1);
    } else {
      Destroy(inlined_space() + s - 1, inlined_space() + s);
      tag().set_inline_size(s - 1);
    }
  }

  // InlinedVector::resize()
  //
  // Resizes the inlined vector to contain `n` elements. If `n` is smaller than
  // the inlined vector's current size, extra elements are destroyed. If `n` is
  // larger than the initial size, new elements are value-initialized.
  void resize(size_type n);

  // Overload of InlinedVector::resize() to resize the inlined vector to contain
  // `n` elements. If `n` is larger than the current size, enough copies of
  // `elem` are appended to increase its size to `n`.
  void resize(size_type n, const value_type& elem);

  // InlinedVector::begin()
  //
  // Returns an iterator to the beginning of the inlined vector.
  iterator begin() noexcept { return data(); }

  // Overload of InlinedVector::begin() for returning a const iterator to the
  // beginning of the inlined vector.
  const_iterator begin() const noexcept { return data(); }

  // InlinedVector::cbegin()
  //
  // Returns a const iterator to the beginning of the inlined vector.
  const_iterator cbegin() const noexcept { return begin(); }

  // InlinedVector::end()
  //
  // Returns an iterator to the end of the inlined vector.
  iterator end() noexcept { return data() + size(); }

  // Overload of InlinedVector::end() for returning a const iterator to the end
  // of the inlined vector.
  const_iterator end() const noexcept { return data() + size(); }

  // InlinedVector::cend()
  //
  // Returns a const iterator to the end of the inlined vector.
  const_iterator cend() const noexcept { return end(); }

  // InlinedVector::rbegin()
  //
  // Returns a reverse iterator from the end of the inlined vector.
  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  // Overload of InlinedVector::rbegin() for returning a const reverse iterator
  // from the end of the inlined vector.
  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }

  // InlinedVector::crbegin()
  //
  // Returns a const reverse iterator from the end of the inlined vector.
  const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  // InlinedVector::rend()
  //
  // Returns a reverse iterator from the beginning of the inlined vector.
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  // Overload of InlinedVector::rend() for returning a const reverse iterator
  // from the beginning of the inlined vector.
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }

  // InlinedVector::crend()
  //
  // Returns a reverse iterator from the beginning of the inlined vector.
  const_reverse_iterator crend() const noexcept { return rend(); }

  // InlinedVector::emplace()
  //
  // Constructs and inserts an object to the inlined vector at the given
  // `position`, returning an iterator pointing to the newly emplaced element.
  template <typename... Args>
  iterator emplace(const_iterator position, Args&&... args);

  // InlinedVector::insert()
  //
  // Inserts an element of the specified value at `position`, returning an
  // iterator pointing to the newly inserted element.
  iterator insert(const_iterator position, const value_type& v) {
    return emplace(position, v);
  }

  // Overload of InlinedVector::insert() for inserting an element of the
  // specified rvalue, returning an iterator pointing to the newly inserted
  // element.
  iterator insert(const_iterator position, value_type&& v) {
    return emplace(position, std::move(v));
  }

  // Overload of InlinedVector::insert() for inserting `n` elements of the
  // specified value at `position`, returning an iterator pointing to the first
  // of the newly inserted elements.
  iterator insert(const_iterator position, size_type n, const value_type& v) {
    return InsertWithCount(position, n, v);
  }

  // Overload of `InlinedVector::insert()` to disambiguate the two
  // three-argument overloads of `insert()`, returning an iterator pointing to
  // the first of the newly inserted elements.
  template <typename InputIterator,
            typename = typename std::enable_if<std::is_convertible<
                typename std::iterator_traits<InputIterator>::iterator_category,
                std::input_iterator_tag>::value>::type>
  iterator insert(const_iterator position, InputIterator first,
                  InputIterator last) {
    using IterType =
        typename std::iterator_traits<InputIterator>::iterator_category;
    return InsertWithRange(position, first, last, IterType());
  }

  // Overload of InlinedVector::insert() for inserting a list of elements at
  // `position`, returning an iterator pointing to the first of the newly
  // inserted elements.
  iterator insert(const_iterator position,
                  std::initializer_list<value_type> init) {
    return insert(position, init.begin(), init.end());
  }

  // InlinedVector::erase()
  //
  // Erases the element at `position` of the inlined vector, returning an
  // iterator pointing to the following element or the container's end if the
  // last element was erased.
  iterator erase(const_iterator position) {
    assert(position >= begin());
    assert(position < end());

    iterator pos = const_cast<iterator>(position);
    std::move(pos + 1, end(), pos);
    pop_back();
    return pos;
  }

  // Overload of InlinedVector::erase() for erasing all elements in the
  // iterator range [first, last) in the inlined vector, returning an iterator
  // pointing to the first element following the range erased, or the
  // container's end if range included the container's last element.
  iterator erase(const_iterator first, const_iterator last);

  // InlinedVector::reserve()
  //
  // Enlarges the underlying representation of the inlined vector so it can hold
  // at least `n` elements. This method does not change `size()` or the actual
  // contents of the vector.
  //
  // Note that if `n` does not exceed the inlined vector's initial size `N`,
  // `reserve()` will have no effect; if it does exceed its initial size,
  // `reserve()` will trigger an initial allocation and move the inlined vector
  // onto the heap. If the vector already exists on the heap and the requested
  // size exceeds it, a reallocation will be performed.
  void reserve(size_type n) {
    if (n > capacity()) {
      // Make room for new elements
      EnlargeBy(n - size());
    }
  }

  // InlinedVector::shrink_to_fit()
  //
  // Reduces memory usage by freeing unused memory.
  // After this call `capacity()` will be equal to `max(N, size())`.
  //
  // If `size() <= N` and the elements are currently stored on the heap, they
  // will be moved to the inlined storage and the heap memory deallocated.
  // If `size() > N` and `size() < capacity()` the elements will be moved to
  // a reallocated storage on heap.
  void shrink_to_fit() {
    const auto s = size();
    if (!allocated() || s == capacity()) {
      // There's nothing to deallocate.
      return;
    }

    if (s <= N) {
      // Move the elements to the inlined storage.
      // We have to do this using a temporary, because inlined_storage and
      // allocation_storage are in a union field.
      auto temp = std::move(*this);
      assign(std::make_move_iterator(temp.begin()),
             std::make_move_iterator(temp.end()));
      return;
    }

    // Reallocate storage and move elements.
    // We can't simply use the same approach as above, because assign() would
    // call into reserve() internally and reserve larger capacity than we need.
    Allocation new_allocation(allocator(), s);
    UninitializedCopy(std::make_move_iterator(allocated_space()),
                      std::make_move_iterator(allocated_space() + s),
                      new_allocation.buffer());
    ResetAllocation(new_allocation, s);
  }

  // InlinedVector::swap()
  //
  // Swaps the contents of this inlined vector with the contents of `other`.
  void swap(InlinedVector& other);

  // InlinedVector::get_allocator()
  //
  // Returns the allocator of this inlined vector.
  allocator_type get_allocator() const { return allocator(); }

 private:
  static_assert(N > 0, "inlined vector with nonpositive size");

  // It holds whether the vector is allocated or not in the lowest bit.
  // The size is held in the high bits:
  //   size_ = (size << 1) | is_allocated;
  class Tag {
   public:
    Tag() : size_(0) {}
    size_type size() const { return size_ >> 1; }
    void add_size(size_type n) { size_ += n << 1; }
    void set_inline_size(size_type n) { size_ = n << 1; }
    void set_allocated_size(size_type n) { size_ = (n << 1) | 1; }
    bool allocated() const { return size_ & 1; }

   private:
    size_type size_;
  };

  // Derives from allocator_type to use the empty base class optimization.
  // If the allocator_type is stateless, we can 'store'
  // our instance of it for free.
  class AllocatorAndTag : private allocator_type {
   public:
    explicit AllocatorAndTag(const allocator_type& a, Tag t = Tag())
        : allocator_type(a), tag_(t) {
    }
    Tag& tag() { return tag_; }
    const Tag& tag() const { return tag_; }
    allocator_type& allocator() { return *this; }
    const allocator_type& allocator() const { return *this; }
   private:
    Tag tag_;
  };

  class Allocation {
   public:
    Allocation(allocator_type& a,  // NOLINT(runtime/references)
               size_type capacity)
        : capacity_(capacity),
          buffer_(AllocatorTraits::allocate(a, capacity_)) {}

    void Dealloc(allocator_type& a) {  // NOLINT(runtime/references)
      AllocatorTraits::deallocate(a, buffer(), capacity());
    }

    size_type capacity() const { return capacity_; }
    const value_type* buffer() const { return buffer_; }
    value_type* buffer() { return buffer_; }

   private:
    size_type capacity_;
    value_type* buffer_;
  };

  const Tag& tag() const { return allocator_and_tag_.tag(); }
  Tag& tag() { return allocator_and_tag_.tag(); }

  Allocation& allocation() {
    return reinterpret_cast<Allocation&>(rep_.allocation_storage.allocation);
  }
  const Allocation& allocation() const {
    return reinterpret_cast<const Allocation&>(
        rep_.allocation_storage.allocation);
  }
  void init_allocation(const Allocation& allocation) {
    new (&rep_.allocation_storage.allocation) Allocation(allocation);
  }

  value_type* inlined_space() {
    return reinterpret_cast<value_type*>(&rep_.inlined_storage.inlined);
  }
  const value_type* inlined_space() const {
    return reinterpret_cast<const value_type*>(&rep_.inlined_storage.inlined);
  }

  value_type* allocated_space() {
    return allocation().buffer();
  }
  const value_type* allocated_space() const {
    return allocation().buffer();
  }

  const allocator_type& allocator() const {
    return allocator_and_tag_.allocator();
  }
  allocator_type& allocator() {
    return allocator_and_tag_.allocator();
  }

  bool allocated() const { return tag().allocated(); }

  // Enlarge the underlying representation so we can store size_ + delta elems.
  // The size is not changed, and any newly added memory is not initialized.
  void EnlargeBy(size_type delta);

  // Shift all elements from position to end() n places to the right.
  // If the vector needs to be enlarged, memory will be allocated.
  // Returns iterators pointing to the start of the previously-initialized
  // portion and the start of the uninitialized portion of the created gap.
  // The number of initialized spots is pair.second - pair.first;
  // the number of raw spots is n - (pair.second - pair.first).
  //
  // Updates the size of the InlinedVector internally.
  std::pair<iterator, iterator> ShiftRight(const_iterator position,
                                           size_type n);

  void ResetAllocation(Allocation new_allocation, size_type new_size) {
    if (allocated()) {
      Destroy(allocated_space(), allocated_space() + size());
      assert(begin() == allocated_space());
      allocation().Dealloc(allocator());
      allocation() = new_allocation;
    } else {
      Destroy(inlined_space(), inlined_space() + size());
      init_allocation(new_allocation);  // bug: only init once
    }
    tag().set_allocated_size(new_size);
  }

  template <typename... Args>
  value_type& GrowAndEmplaceBack(Args&&... args) {
    assert(size() == capacity());
    const size_type s = size();

    Allocation new_allocation(allocator(), 2 * capacity());

    value_type& new_element =
        Construct(new_allocation.buffer() + s, std::forward<Args>(args)...);
    UninitializedCopy(std::make_move_iterator(data()),
                      std::make_move_iterator(data() + s),
                      new_allocation.buffer());

    ResetAllocation(new_allocation, s + 1);

    return new_element;
  }

  void InitAssign(size_type n);
  void InitAssign(size_type n, const value_type& t);

  template <typename... Args>
  value_type& Construct(pointer p, Args&&... args) {
    AllocatorTraits::construct(allocator(), p, std::forward<Args>(args)...);
    return *p;
  }

  template <typename Iter>
  void UninitializedCopy(Iter src, Iter src_last, value_type* dst) {
    for (; src != src_last; ++dst, ++src) Construct(dst, *src);
  }

  template <typename... Args>
  void UninitializedFill(value_type* dst, value_type* dst_last,
                         const Args&... args) {
    for (; dst != dst_last; ++dst) Construct(dst, args...);
  }

  // Destroy [ptr, ptr_last) in place.
  void Destroy(value_type* ptr, value_type* ptr_last);

  template <typename Iter>
  void AppendRange(Iter first, Iter last, std::input_iterator_tag) {
    std::copy(first, last, std::back_inserter(*this));
  }

  // Faster path for forward iterators.
  template <typename Iter>
  void AppendRange(Iter first, Iter last, std::forward_iterator_tag);

  template <typename Iter>
  void AppendRange(Iter first, Iter last) {
    using IterTag = typename std::iterator_traits<Iter>::iterator_category;
    AppendRange(first, last, IterTag());
  }

  template <typename Iter>
  void AssignRange(Iter first, Iter last, std::input_iterator_tag);

  // Faster path for forward iterators.
  template <typename Iter>
  void AssignRange(Iter first, Iter last, std::forward_iterator_tag);

  template <typename Iter>
  void AssignRange(Iter first, Iter last) {
    using IterTag = typename std::iterator_traits<Iter>::iterator_category;
    AssignRange(first, last, IterTag());
  }

  iterator InsertWithCount(const_iterator position, size_type n,
                           const value_type& v);

  template <typename InputIter>
  iterator InsertWithRange(const_iterator position, InputIter first,
                           InputIter last, std::input_iterator_tag);
  template <typename ForwardIter>
  iterator InsertWithRange(const_iterator position, ForwardIter first,
                           ForwardIter last, std::forward_iterator_tag);

  AllocatorAndTag allocator_and_tag_;

  // Either the inlined or allocated representation
  union Rep {
    // Use struct to perform indirection that solves a bizarre compilation
    // error on Visual Studio (all known versions).
    struct {
      typename std::aligned_storage<sizeof(value_type),
                                    alignof(value_type)>::type inlined[N];
    } inlined_storage;
    struct {
      typename std::aligned_storage<sizeof(Allocation),
                                    alignof(Allocation)>::type allocation;
    } allocation_storage;
  } rep_;
};

// -----------------------------------------------------------------------------
// InlinedVector Non-Member Functions
// -----------------------------------------------------------------------------

// swap()
//
// Swaps the contents of two inlined vectors. This convenience function
// simply calls InlinedVector::swap(other_inlined_vector).
template <typename T, size_t N, typename A>
void swap(InlinedVector<T, N, A>& a,
          InlinedVector<T, N, A>& b) noexcept(noexcept(a.swap(b))) {
  a.swap(b);
}

// operator==()
//
// Tests the equivalency of the contents of two inlined vectors.
template <typename T, size_t N, typename A>
bool operator==(const InlinedVector<T, N, A>& a,
                const InlinedVector<T, N, A>& b) {
  return absl::equal(a.begin(), a.end(), b.begin(), b.end());
}

// operator!=()
//
// Tests the inequality of the contents of two inlined vectors.
template <typename T, size_t N, typename A>
bool operator!=(const InlinedVector<T, N, A>& a,
                const InlinedVector<T, N, A>& b) {
  return !(a == b);
}

// operator<()
//
// Tests whether the contents of one inlined vector are less than the contents
// of another through a lexicographical comparison operation.
template <typename T, size_t N, typename A>
bool operator<(const InlinedVector<T, N, A>& a,
               const InlinedVector<T, N, A>& b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

// operator>()
//
// Tests whether the contents of one inlined vector are greater than the
// contents of another through a lexicographical comparison operation.
template <typename T, size_t N, typename A>
bool operator>(const InlinedVector<T, N, A>& a,
               const InlinedVector<T, N, A>& b) {
  return b < a;
}

// operator<=()
//
// Tests whether the contents of one inlined vector are less than or equal to
// the contents of another through a lexicographical comparison operation.
template <typename T, size_t N, typename A>
bool operator<=(const InlinedVector<T, N, A>& a,
                const InlinedVector<T, N, A>& b) {
  return !(b < a);
}

// operator>=()
//
// Tests whether the contents of one inlined vector are greater than or equal to
// the contents of another through a lexicographical comparison operation.
template <typename T, size_t N, typename A>
bool operator>=(const InlinedVector<T, N, A>& a,
                const InlinedVector<T, N, A>& b) {
  return !(a < b);
}

// -----------------------------------------------------------------------------
// Implementation of InlinedVector
// -----------------------------------------------------------------------------
//
// Do not depend on any implementation details below this line.

template <typename T, size_t N, typename A>
InlinedVector<T, N, A>::InlinedVector(const InlinedVector& v)
    : allocator_and_tag_(v.allocator()) {
  reserve(v.size());
  if (allocated()) {
    UninitializedCopy(v.begin(), v.end(), allocated_space());
    tag().set_allocated_size(v.size());
  } else {
    UninitializedCopy(v.begin(), v.end(), inlined_space());
    tag().set_inline_size(v.size());
  }
}

template <typename T, size_t N, typename A>
InlinedVector<T, N, A>::InlinedVector(const InlinedVector& v,
                                      const allocator_type& alloc)
    : allocator_and_tag_(alloc) {
  reserve(v.size());
  if (allocated()) {
    UninitializedCopy(v.begin(), v.end(), allocated_space());
    tag().set_allocated_size(v.size());
  } else {
    UninitializedCopy(v.begin(), v.end(), inlined_space());
    tag().set_inline_size(v.size());
  }
}

template <typename T, size_t N, typename A>
InlinedVector<T, N, A>::InlinedVector(InlinedVector&& v) noexcept(
    absl::allocator_is_nothrow<allocator_type>::value ||
    std::is_nothrow_move_constructible<value_type>::value)
    : allocator_and_tag_(v.allocator_and_tag_) {
  if (v.allocated()) {
    // We can just steal the underlying buffer from the source.
    // That leaves the source empty, so we clear its size.
    init_allocation(v.allocation());
    v.tag() = Tag();
  } else {
    UninitializedCopy(std::make_move_iterator(v.inlined_space()),
                      std::make_move_iterator(v.inlined_space() + v.size()),
                      inlined_space());
  }
}

template <typename T, size_t N, typename A>
InlinedVector<T, N, A>::InlinedVector(
    InlinedVector&& v,
    const allocator_type&
        alloc) noexcept(absl::allocator_is_nothrow<allocator_type>::value)
    : allocator_and_tag_(alloc) {
  if (v.allocated()) {
    if (alloc == v.allocator()) {
      // We can just steal the allocation from the source.
      tag() = v.tag();
      init_allocation(v.allocation());
      v.tag() = Tag();
    } else {
      // We need to use our own allocator
      reserve(v.size());
      UninitializedCopy(std::make_move_iterator(v.begin()),
                        std::make_move_iterator(v.end()), allocated_space());
      tag().set_allocated_size(v.size());
    }
  } else {
    UninitializedCopy(std::make_move_iterator(v.inlined_space()),
                      std::make_move_iterator(v.inlined_space() + v.size()),
                      inlined_space());
    tag().set_inline_size(v.size());
  }
}

template <typename T, size_t N, typename A>
void InlinedVector<T, N, A>::InitAssign(size_type n, const value_type& t) {
  if (n > static_cast<size_type>(N)) {
    Allocation new_allocation(allocator(), n);
    init_allocation(new_allocation);
    UninitializedFill(allocated_space(), allocated_space() + n, t);
    tag().set_allocated_size(n);
  } else {
    UninitializedFill(inlined_space(), inlined_space() + n, t);
    tag().set_inline_size(n);
  }
}

template <typename T, size_t N, typename A>
void InlinedVector<T, N, A>::InitAssign(size_type n) {
  if (n > static_cast<size_type>(N)) {
    Allocation new_allocation(allocator(), n);
    init_allocation(new_allocation);
    UninitializedFill(allocated_space(), allocated_space() + n);
    tag().set_allocated_size(n);
  } else {
    UninitializedFill(inlined_space(), inlined_space() + n);
    tag().set_inline_size(n);
  }
}

template <typename T, size_t N, typename A>
void InlinedVector<T, N, A>::resize(size_type n) {
  size_type s = size();
  if (n < s) {
    erase(begin() + n, end());
    return;
  }
  reserve(n);
  assert(capacity() >= n);

  // Fill new space with elements constructed in-place.
  if (allocated()) {
    UninitializedFill(allocated_space() + s, allocated_space() + n);
    tag().set_allocated_size(n);
  } else {
    UninitializedFill(inlined_space() + s, inlined_space() + n);
    tag().set_inline_size(n);
  }
}

template <typename T, size_t N, typename A>
void InlinedVector<T, N, A>::resize(size_type n, const value_type& elem) {
  size_type s = size();
  if (n < s) {
    erase(begin() + n, end());
    return;
  }
  reserve(n);
  assert(capacity() >= n);

  // Fill new space with copies of 'elem'.
  if (allocated()) {
    UninitializedFill(allocated_space() + s, allocated_space() + n, elem);
    tag().set_allocated_size(n);
  } else {
    UninitializedFill(inlined_space() + s, inlined_space() + n, elem);
    tag().set_inline_size(n);
  }
}

template <typename T, size_t N, typename A>
template <typename... Args>
typename InlinedVector<T, N, A>::iterator InlinedVector<T, N, A>::emplace(
    const_iterator position, Args&&... args) {
  assert(position >= begin());
  assert(position <= end());
  if (position == end()) {
    emplace_back(std::forward<Args>(args)...);
    return end() - 1;
  }

  T new_t = T(std::forward<Args>(args)...);

  auto range = ShiftRight(position, 1);
  if (range.first == range.second) {
    // constructing into uninitialized memory
    Construct(range.first, std::move(new_t));
  } else {
    // assigning into moved-from object
    *range.first = T(std::move(new_t));
  }

  return range.first;
}

template <typename T, size_t N, typename A>
typename InlinedVector<T, N, A>::iterator InlinedVector<T, N, A>::erase(
    const_iterator first, const_iterator last) {
  assert(begin() <= first);
  assert(first <= last);
  assert(last <= end());

  iterator range_start = const_cast<iterator>(first);
  iterator range_end = const_cast<iterator>(last);

  size_type s = size();
  ptrdiff_t erase_gap = std::distance(range_start, range_end);
  if (erase_gap > 0) {
    pointer space;
    if (allocated()) {
      space = allocated_space();
      tag().set_allocated_size(s - erase_gap);
    } else {
      space = inlined_space();
      tag().set_inline_size(s - erase_gap);
    }
    std::move(range_end, space + s, range_start);
    Destroy(space + s - erase_gap, space + s);
  }
  return range_start;
}

template <typename T, size_t N, typename A>
void InlinedVector<T, N, A>::swap(InlinedVector& other) {
  using std::swap;  // Augment ADL with std::swap.
  if (&other == this) {
    return;
  }
  if (allocated() && other.allocated()) {
    // Both out of line, so just swap the tag, allocation, and allocator.
    swap(tag(), other.tag());
    swap(allocation(), other.allocation());
    swap(allocator(), other.allocator());
    return;
  }
  if (!allocated() && !other.allocated()) {
    // Both inlined: swap up to smaller size, then move remaining elements.
    InlinedVector* a = this;
    InlinedVector* b = &other;
    if (size() < other.size()) {
      swap(a, b);
    }

    const size_type a_size = a->size();
    const size_type b_size = b->size();
    assert(a_size >= b_size);
    // 'a' is larger. Swap the elements up to the smaller array size.
    std::swap_ranges(a->inlined_space(),
                     a->inlined_space() + b_size,
                     b->inlined_space());

    // Move the remaining elements: A[b_size,a_size) -> B[b_size,a_size)
    b->UninitializedCopy(a->inlined_space() + b_size,
                         a->inlined_space() + a_size,
                         b->inlined_space() + b_size);
    a->Destroy(a->inlined_space() + b_size, a->inlined_space() + a_size);

    swap(a->tag(), b->tag());
    swap(a->allocator(), b->allocator());
    assert(b->size() == a_size);
    assert(a->size() == b_size);
    return;
  }
  // One is out of line, one is inline.
  // We first move the elements from the inlined vector into the
  // inlined space in the other vector.  We then put the other vector's
  // pointer/capacity into the originally inlined vector and swap
  // the tags.
  InlinedVector* a = this;
  InlinedVector* b = &other;
  if (a->allocated()) {
    swap(a, b);
  }
  assert(!a->allocated());
  assert(b->allocated());
  const size_type a_size = a->size();
  const size_type b_size = b->size();
  // In an optimized build, b_size would be unused.
  (void)b_size;

  // Made Local copies of size(), don't need tag() accurate anymore
  swap(a->tag(), b->tag());

  // Copy b_allocation out before b's union gets clobbered by inline_space.
  Allocation b_allocation = b->allocation();

  b->UninitializedCopy(a->inlined_space(), a->inlined_space() + a_size,
                       b->inlined_space());
  a->Destroy(a->inlined_space(), a->inlined_space() + a_size);

  a->allocation() = b_allocation;

  if (a->allocator() != b->allocator()) {
    swap(a->allocator(), b->allocator());
  }

  assert(b->size() == a_size);
  assert(a->size() == b_size);
}

template <typename T, size_t N, typename A>
void InlinedVector<T, N, A>::EnlargeBy(size_type delta) {
  const size_type s = size();
  assert(s <= capacity());

  size_type target = std::max(static_cast<size_type>(N), s + delta);

  // Compute new capacity by repeatedly doubling current capacity
  // TODO(user): Check and avoid overflow?
  size_type new_capacity = capacity();
  while (new_capacity < target) {
    new_capacity <<= 1;
  }

  Allocation new_allocation(allocator(), new_capacity);

  UninitializedCopy(std::make_move_iterator(data()),
                    std::make_move_iterator(data() + s),
                    new_allocation.buffer());

  ResetAllocation(new_allocation, s);
}

template <typename T, size_t N, typename A>
auto InlinedVector<T, N, A>::ShiftRight(const_iterator position, size_type n)
    -> std::pair<iterator, iterator> {
  iterator start_used = const_cast<iterator>(position);
  iterator start_raw = const_cast<iterator>(position);
  size_type s = size();
  size_type required_size = s + n;

  if (required_size > capacity()) {
    // Compute new capacity by repeatedly doubling current capacity
    size_type new_capacity = capacity();
    while (new_capacity < required_size) {
      new_capacity <<= 1;
    }
    // Move everyone into the new allocation, leaving a gap of n for the
    // requested shift.
    Allocation new_allocation(allocator(), new_capacity);
    size_type index = position - begin();
    UninitializedCopy(std::make_move_iterator(data()),
                      std::make_move_iterator(data() + index),
                      new_allocation.buffer());
    UninitializedCopy(std::make_move_iterator(data() + index),
                      std::make_move_iterator(data() + s),
                      new_allocation.buffer() + index + n);
    ResetAllocation(new_allocation, s);

    // New allocation means our iterator is invalid, so we'll recalculate.
    // Since the entire gap is in new space, there's no used space to reuse.
    start_raw = begin() + index;
    start_used = start_raw;
  } else {
    // If we had enough space, it's a two-part move. Elements going into
    // previously-unoccupied space need an UninitializedCopy. Elements
    // going into a previously-occupied space are just a move.
    iterator pos = const_cast<iterator>(position);
    iterator raw_space = end();
    size_type slots_in_used_space = raw_space - pos;
    size_type new_elements_in_used_space = std::min(n, slots_in_used_space);
    size_type new_elements_in_raw_space = n - new_elements_in_used_space;
    size_type old_elements_in_used_space =
        slots_in_used_space - new_elements_in_used_space;

    UninitializedCopy(std::make_move_iterator(pos + old_elements_in_used_space),
                      std::make_move_iterator(raw_space),
                      raw_space + new_elements_in_raw_space);
    std::move_backward(pos, pos + old_elements_in_used_space, raw_space);

    // If the gap is entirely in raw space, the used space starts where the raw
    // space starts, leaving no elements in used space. If the gap is entirely
    // in used space, the raw space starts at the end of the gap, leaving all
    // elements accounted for within the used space.
    start_used = pos;
    start_raw = pos + new_elements_in_used_space;
  }
  tag().add_size(n);
  return std::make_pair(start_used, start_raw);
}

template <typename T, size_t N, typename A>
void InlinedVector<T, N, A>::Destroy(value_type* ptr, value_type* ptr_last) {
  for (value_type* p = ptr; p != ptr_last; ++p) {
    AllocatorTraits::destroy(allocator(), p);
  }

  // Overwrite unused memory with 0xab so we can catch uninitialized usage.
  // Cast to void* to tell the compiler that we don't care that we might be
  // scribbling on a vtable pointer.
#ifndef NDEBUG
  if (ptr != ptr_last) {
    memset(reinterpret_cast<void*>(ptr), 0xab,
           sizeof(*ptr) * (ptr_last - ptr));
  }
#endif
}

template <typename T, size_t N, typename A>
template <typename Iter>
void InlinedVector<T, N, A>::AppendRange(Iter first, Iter last,
                                         std::forward_iterator_tag) {
  using Length = typename std::iterator_traits<Iter>::difference_type;
  Length length = std::distance(first, last);
  reserve(size() + length);
  if (allocated()) {
    UninitializedCopy(first, last, allocated_space() + size());
    tag().set_allocated_size(size() + length);
  } else {
    UninitializedCopy(first, last, inlined_space() + size());
    tag().set_inline_size(size() + length);
  }
}

template <typename T, size_t N, typename A>
template <typename Iter>
void InlinedVector<T, N, A>::AssignRange(Iter first, Iter last,
                                         std::input_iterator_tag) {
  // Optimized to avoid reallocation.
  // Prefer reassignment to copy construction for elements.
  iterator out = begin();
  for ( ; first != last && out != end(); ++first, ++out)
    *out = *first;
  erase(out, end());
  std::copy(first, last, std::back_inserter(*this));
}

template <typename T, size_t N, typename A>
template <typename Iter>
void InlinedVector<T, N, A>::AssignRange(Iter first, Iter last,
                                         std::forward_iterator_tag) {
  using Length = typename std::iterator_traits<Iter>::difference_type;
  Length length = std::distance(first, last);
  // Prefer reassignment to copy construction for elements.
  if (static_cast<size_type>(length) <= size()) {
    erase(std::copy(first, last, begin()), end());
    return;
  }
  reserve(length);
  iterator out = begin();
  for (; out != end(); ++first, ++out) *out = *first;
  if (allocated()) {
    UninitializedCopy(first, last, out);
    tag().set_allocated_size(length);
  } else {
    UninitializedCopy(first, last, out);
    tag().set_inline_size(length);
  }
}

template <typename T, size_t N, typename A>
auto InlinedVector<T, N, A>::InsertWithCount(const_iterator position,
                                             size_type n, const value_type& v)
    -> iterator {
  assert(position >= begin() && position <= end());
  if (n == 0) return const_cast<iterator>(position);

  value_type copy = v;
  std::pair<iterator, iterator> it_pair = ShiftRight(position, n);
  std::fill(it_pair.first, it_pair.second, copy);
  UninitializedFill(it_pair.second, it_pair.first + n, copy);

  return it_pair.first;
}

template <typename T, size_t N, typename A>
template <typename InputIter>
auto InlinedVector<T, N, A>::InsertWithRange(const_iterator position,
                                             InputIter first, InputIter last,
                                             std::input_iterator_tag)
    -> iterator {
  assert(position >= begin() && position <= end());
  size_type index = position - cbegin();
  size_type i = index;
  while (first != last) insert(begin() + i++, *first++);
  return begin() + index;
}

// Overload of InlinedVector::InsertWithRange()
template <typename T, size_t N, typename A>
template <typename ForwardIter>
auto InlinedVector<T, N, A>::InsertWithRange(const_iterator position,
                                             ForwardIter first,
                                             ForwardIter last,
                                             std::forward_iterator_tag)
    -> iterator {
  assert(position >= begin() && position <= end());
  if (first == last) {
    return const_cast<iterator>(position);
  }
  using Length = typename std::iterator_traits<ForwardIter>::difference_type;
  Length n = std::distance(first, last);
  std::pair<iterator, iterator> it_pair = ShiftRight(position, n);
  size_type used_spots = it_pair.second - it_pair.first;
  ForwardIter open_spot = std::next(first, used_spots);
  std::copy(first, open_spot, it_pair.first);
  UninitializedCopy(open_spot, last, it_pair.second);
  return it_pair.first;
}

}  // namespace absl


#endif  // S2_THIRD_PARTY_ABSL_CONTAINER_INLINED_VECTOR_H_
