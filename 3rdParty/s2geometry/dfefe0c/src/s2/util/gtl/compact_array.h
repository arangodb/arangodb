// Copyright 2005 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


// compact_array is a more memory-efficient implementation of std::vector.
// It uses a pointer with an integer that stores both size and capacity.
//
// Implementation details:
//
// compact_array is a small-overhead STL-like Collection, but can only be used
// for types with trivial copy, assign, and destructor. It only takes 16
// bytes for 64-bit binary (instead of the typical 24-bytes for vector).
// Its size can grow to 2^24 (16M) elements. compact_array is memory
// efficient when it is small, and CPU-efficient for growing a large array.
// It does this by keeping both the size and capacity.  When the size is less
// than 64, the capacity is exactly as reserved, and grows linearly. Once the
// size grows bigger than 64, the capacity grows exponentially.
//
// IMPORTANT: compact_array_base does not support a constructor and destructor
// because it is designed to be used in "union". The replacements are
// Construct() and Destruct() which MUST be called explicitly. If you need
// constructor and destructor, use compact_array instead.
//

#ifndef S2_UTIL_GTL_COMPACT_ARRAY_H_
#define S2_UTIL_GTL_COMPACT_ARRAY_H_

#include <cstddef>
#include <cstring>
#include <sys/types.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <ostream>  // NOLINT
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/base/port.h"
#include "s2/third_party/absl/meta/type_traits.h"
#include "s2/util/bits/bits.h"
#include "s2/util/gtl/container_logging.h"
#include "s2/util/gtl/libc_allocator_with_realloc.h"

namespace gtl {

template <typename T,
          typename A =
              gtl::libc_allocator_with_realloc<T> >
class compact_array_base {
 private:
  // The number of bits for the variable size_ and capacity_
  static const int kSizeNumBits = 24;
  static const int kCapacityNumBits = 6;
  // Where capacity_ becomes an exponent (of 2) instead of the exact value
  static const int kExponentStart = (1 << kCapacityNumBits);
  // kMaxSize is the maximum size this array can grow.
  static const int kMaxSize = (1 << kSizeNumBits) - 1;

#ifdef IS_LITTLE_ENDIAN
  uint32 size_        : kSizeNumBits;      // number of valid items in the array
  uint32 capacity_    : kCapacityNumBits;  // allocated array size
  uint32 is_exponent_ : 1;                 // whether capacity_ is an exponent

  // This object might share memory representation (ie. union) with
  // other data structures. We reserved the DO_NOT_USE (32nd bit in
  // little endian format) to be used as a tag.
  uint32 DO_NOT_USE   : 1;
#else
  uint32 DO_NOT_USE   : 1;
  uint32 is_exponent_ : 1;
  uint32 capacity_    : kCapacityNumBits;
  uint32 size_        : kSizeNumBits;
#endif

  // Opportunistically consider allowing inlined elements.
#if defined(_LP64) && defined(__GNUC__)
  // With 64-bit pointers, our approach is to form a 16-byte struct:
  //   [5 bytes for size, capacity, is_exponent and is_inlined]
  //   [3 bytes of padding or inlined elements]
  //   [8 bytes of more inlined elements or a pointer]
  // We require 0-length arrays to take 0 bytes, and no strict aliasing. There
  // should be no compiler-inserted padding between any of our members.
  enum {
    kMaxInlinedBytes = 11,
    kInlined = kMaxInlinedBytes / sizeof(T),
    kActualInlinedBytes = kInlined * sizeof(T),
    kUnusedPaddingBytes = (kMaxInlinedBytes - kActualInlinedBytes) > 3 ?
        3 : (kMaxInlinedBytes - kActualInlinedBytes)
  };

  T* Array() { return IsInlined() ? InlinedSpace() : pointer_; }
  void SetArray(T* p) {
    static_assert(sizeof(*this) == 16, "size assumption");
    static_assert(sizeof(this) == 8, "pointer size assumption");
    is_inlined_ = false;
    pointer_ = p;
  }
  void SetInlined() {
    S2_DCHECK_LE(capacity(), kInlined);
    is_inlined_ = true;
  }
  T* InlinedSpace() { return reinterpret_cast<T*>(inlined_elements_); }

  bool is_inlined_;  // If false, the last 8 bytes of *this are a pointer.

  // After is_inlined_, the next field may not be sufficiently aligned to store
  // an object of type T. Pad it out with (unaligned) chars.
  char unused_padding_[kUnusedPaddingBytes];

  // inlined_elements_ stores the first N elements, potentially as few as zero.
  char inlined_elements_[3 - kUnusedPaddingBytes];

  // compact_array_base itself is at least as aligned as a T* because of the
  // T* member inside this union. The only reason to split inlined_elements_
  // into two pieces is to have a place to put this T* member.
  union {
    T* pointer_;
    char more_inlined_elements_[sizeof(T*)];
  };
#else
  enum { kInlined = 0, is_inlined_ = false };
  T* Array() { return first_; }
  void SetArray(T* p) { first_ = p; }
  void SetInlined() { S2_LOG(FATAL); }
  T* InlinedSpace() { return nullptr; }

  // The pointer to the actual data array.
  T* first_;
#endif
  bool IsInlined() const { return is_inlined_; }
  const T* ConstArray() const {
    return const_cast<compact_array_base<T, A>*>(this)->Array();
  }

  template <class V, class Alloc>
  class alloc_impl : public Alloc {
   public:
    V* do_realloc(V* ptr, size_t old_size, size_t new_size) {
      V* new_ptr = this->allocate(new_size);
      memcpy(new_ptr, ptr, old_size * sizeof(V));
      this->deallocate(ptr, old_size);
      return new_ptr;
    }
  };

  // A template specialization of alloc_impl for
  // libc_allocator_with_realloc that supports reallocate.
  template <class V, class Alloc>
  class alloc_impl<V,
                   gtl::
                   libc_allocator_with_realloc<Alloc> >
      : public gtl::
               libc_allocator_with_realloc<Alloc> {
   public:
    V* do_realloc(V* ptr, size_t old_size, size_t new_size) {
      return this->reallocate(ptr, new_size);
    }
  };

  typedef typename A::template rebind<T>::other internal_alloc;
  typedef alloc_impl<T, internal_alloc> value_allocator_type;

 public:
  typedef T                                     value_type;
  typedef A                                     allocator_type;
  typedef value_type*                           pointer;
  typedef const value_type*                     const_pointer;
  typedef value_type&                           reference;
  typedef const value_type&                     const_reference;
  typedef uint32                                size_type;
  typedef ptrdiff_t                             difference_type;

  typedef value_type*                           iterator;
  typedef const value_type*                     const_iterator;
  typedef std::reverse_iterator<iterator>       reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  // Init() replace the default constructors; so it can be used in "union".
  // This means Init() must be called for every new compact_array_base
  void Init() noexcept { memset(this, 0, sizeof(*this)); }

  // Construct an array of size n and initialize the values to v.
  // Any old contents, if heap-allocated, will be leaked.
  void Construct(size_type n, const value_type& v = value_type()) {
    Init();
    value_init(n, v);
  }

  // See 23.1.1/9 in the C++ standard for an explanation.
  template <typename Iterator>
  void Copy(Iterator first, Iterator last) {
    Init();
    typedef typename std::is_integral<Iterator>::type Int;
    initialize(first, last, Int());
  }

  void CopyFrom(const compact_array_base& v) {
    Init();
    initialize(v.begin(), v.end(), std::false_type());
  }

  compact_array_base& AssignFrom(const compact_array_base& v) {
    // Safe for self-assignment, which is rare.
    // Optimized to use existing allocated space.
    // Also to use assignment instead of copying where possible.
    if (size() < v.size()) {  // grow
      reserve(v.size());
      std::copy(v.begin(), v.begin() + size(), begin());
      insert(end(), v.begin() + size(), v.end());
    } else {  // maybe shrink
      erase(begin() + v.size(), end());
      std::copy(v.begin(), v.end(), begin());
    }
    return *this;
  }

  // Deallocate the whole array.
  void Destruct() {
    if (!MayBeInlined() || Array() != InlinedSpace()) {
      value_allocator_type allocator;
      allocator.deallocate(Array(), capacity());
    }
    Init();
  }

  // Safe against self-swapping.
  // copying/destruction of compact_array_base is fairly trivial as the type
  // was designed to be useable in a C++98 union.
  void swap(compact_array_base& v) noexcept {
    compact_array_base tmp = *this;
    *this = v;
    v = tmp;
  }

  // The number of active items in the array.
  size_type size() const { return size_; }
  bool empty() const { return size() == 0; }

  // Maximum size that this data structure can hold.
  static size_type max_size() { return kMaxSize; }

  static bool MayBeInlined() { return kInlined > 0; }

 public:                                // Container interface (tables 65,66).
  iterator begin() { return Array(); }
  iterator end()   { return Array() + size(); }
  const_iterator begin() const { return ConstArray(); }
  const_iterator end() const   { return ConstArray() + size(); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend()   { return reverse_iterator(Array()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(ConstArray());
  }

 private:
  // This Insert() is private because it might return the end().
  iterator Insert(const_iterator p, const value_type& v) {
    if (size() >= kMaxSize) {
      throw std::length_error("compact_array size exceeded");
    }
    iterator r = make_hole(p, 1);
    *r = v;
    return r;
  }

 public:                                // Sequence operations, table 67.
  iterator insert(const_iterator p, const value_type& v) {
    return Insert(p, v);
  }

  void insert(const_iterator p, size_type n, const value_type& v) {
    if (n + size() > kMaxSize) {
      throw std::length_error("compact_array size exceeded");
    }
    value_insert(p, n, v);
  }

  // See 23.1.1/9 in the C++ standard for an explanation.
  template <typename Iterator>
  void insert(const_iterator p, Iterator first, Iterator last) {
    typedef typename std::is_integral<Iterator>::type Int;
    insert(p, first, last, Int());
  }

  iterator erase(const_iterator p) {
    size_type index = p - begin();
    erase_aux(p, 1);
    return begin() + index;
  }

  iterator erase(const_iterator first, const_iterator last) {
    size_type index = first - begin();
    erase_aux(first, last - first);
    return begin() + index;
  }

  // clear just resets the size to 0, without deallocating the storage.
  // To deallocate the array, use Destruct().
  void clear() {
    set_size(0);
  }

  reference front() { return begin()[0]; }
  const_reference front() const { return begin()[0]; }
  reference back() { return end()[-1]; }
  const_reference back() const { return end()[-1]; }

  void push_back(const value_type& v) {
    iterator p = make_hole(end(), 1);
    *p = v;
  }
  void pop_back() {
    erase_aux(end()-1, 1);
  }

  reference operator[](size_type n) {
    S2_DCHECK_LT(n, size_);
    return Array()[n];
  }

  const_reference operator[](size_type n) const {
    S2_DCHECK_LT(n, size_);
    return ConstArray()[n];
  }

  reference at(size_type n) {
    if (n >= size_) {
      throw std::out_of_range("compact_array index out of range");
    }
    return Array()[n];
  }

  const_reference at(size_type n) const {
    if (n >= size_) {
      throw std::out_of_range("compact_array index out of range");
    }
    return ConstArray()[n];
  }

  // Preallocate the array of size n. Only changes the capacity, not size.
  void reserve(int n) {
    reallocate(n);
  }

  size_type capacity() const {
    return is_exponent_ ? (1 << capacity_) : capacity_;
  }

  void resize(size_type n) {
    if (n > capacity()) reserve(n);
    // resize(n) is the only place in the class that exposes uninitialized
    // memory as live elements, so call a constructor for each element if
    // needed.
    // Destroying elements on shrinking resize isn't a concern, since the
    // value_type must be trivially destructible.
    if (n > size() &&
        !absl::is_trivially_default_constructible<value_type>::value) {
      // Increasing size would expose unconstructed elements.
      value_type *new_end = Array() + n;
      for (value_type *p = Array() + size(); p != new_end; ++p)
        new (p) value_type();
    }
    set_size(n);
  }

 private:                               // Low-level helper functions.
  void set_size(size_type n) {
    S2_DCHECK_LE(n, capacity());
    size_ = n;
  }

  void set_capacity(size_type n) {
    S2_DCHECK_LE(size(), n);
    is_exponent_ = (n >= kExponentStart);
    capacity_ = is_exponent_ ? Bits::Log2Ceiling(n) : n;
    // A tiny optimization here would be to set capacity_ to kInlined if
    // it's currently less. We don't bother, because doing so would require
    // changing the existing comments and unittests that say that, for small n,
    // capacity() will be exactly n if one calls reserve(n).
    S2_DCHECK(n == capacity() || n > kInlined);
  }

  // Make capacity n or more. Reallocate and copy data as necessary.
  void reallocate(size_type n) {
    size_type old_capacity = capacity();
    if (n <= old_capacity)  return;
    set_capacity(n);
    if (MayBeInlined()) {
      if (!IsInlined() && n <= kInlined) {
        SetInlined();
        return;
      } else if (IsInlined()) {
        if (n > kInlined) {
          value_allocator_type allocator;
          value_type* new_array = allocator.allocate(capacity());
          memcpy(new_array, InlinedSpace(), size() * sizeof(T));
          SetArray(new_array);
        }
        return;
      }
    }
    value_allocator_type allocator;
    SetArray(allocator.do_realloc(Array(), old_capacity, capacity()));
  }

  value_type* lastp() { return Array() + size(); }

  void move(const value_type* first, const value_type* last, value_type* out) {
    memmove(out, first, (last - first) * sizeof(value_type));
  }

  iterator make_hole(const_iterator p, size_type n) {
    iterator q = const_cast<iterator>(p);
    if (n != 0) {
      size_type new_size = size() + n;
      size_type index = q - Array();
      reallocate(new_size);
      q = Array() + index;
      move(q, Array() + new_size - n, q + n);
      set_size(new_size);
    }
    return q;
  }

  void erase_aux(const_iterator p, size_type n) {
    iterator q = const_cast<iterator>(p);
    size_type new_size = size() - n;
    move(q + n, lastp(), q);
    reallocate(new_size);
    set_size(new_size);
  }

 private:                               // Helper functions for range/value.
  void value_init(size_type n, const value_type& v) {
    reserve(n);
    set_size(n);
    std::fill(Array(), lastp(), v);
  }

  template <typename InputIter>
  void range_init(InputIter first, InputIter last, std::input_iterator_tag) {
    for ( ; first != last; ++first)
      push_back(*first);
  }

  template <typename ForwIter>
  void range_init(ForwIter first, ForwIter last, std::forward_iterator_tag) {
    size_type n = std::distance(first, last);
    reserve(n);
    set_size(n);
    std::copy(first, last, Array());
  }

  template <typename Integer>
  void initialize(Integer n, Integer v, std::true_type) {
    value_init(n, v);
  }

  template <typename Iterator>
  void initialize(Iterator first, Iterator last, std::false_type) {
    typedef typename std::iterator_traits<Iterator>::iterator_category Cat;
    range_init(first, last, Cat());
  }

  void value_insert(const_iterator p, size_type n, const value_type& v) {
    if (n + size() > kMaxSize) {
      throw std::length_error("compact_array size exceeded");
    }
    iterator hole = make_hole(p, n);
    std::fill(hole, hole + n, v);
  }

  template <typename InputIter>
  void range_insert(const_iterator p, InputIter first, InputIter last,
                    std::input_iterator_tag) {
    size_type pos = p - begin();
    size_type old_size = size();
    for (; first != last; ++first)
      push_back(*first);
    std::rotate(begin() + pos, begin() + old_size, end());
  }

  template <typename ForwIter>
  void range_insert(const_iterator p, ForwIter first, ForwIter last,
                    std::forward_iterator_tag) {
    size_type n = std::distance(first, last);
    if (n + size() > kMaxSize) {
      throw std::length_error("compact_array size exceeded");
    }
    std::copy(first, last, make_hole(p, n));
  }

  template <typename Integer>
  void insert(const_iterator p, Integer n, Integer v, std::true_type) {
    value_insert(p, n, v);
  }

  template <typename Iterator>
  void insert(const_iterator p, Iterator first, Iterator last,
              std::false_type) {
    typedef typename std::iterator_traits<Iterator>::iterator_category Cat;
    range_insert(p, first, last, Cat());
  }
  static_assert(absl::is_trivially_copy_constructible<value_type>::value &&
                absl::is_trivially_copy_assignable<value_type>::value &&
                absl::is_trivially_destructible<value_type>::value,
                "Requires trivial copy, assignment, and destructor.");
};

// Allocates storage for constants in compact_array_base<T>
template <typename T, typename A>
    const int compact_array_base<T, A>::kSizeNumBits;
template <typename T, typename A>
    const int compact_array_base<T, A>::kCapacityNumBits;
template <typename T, typename A>
    const int compact_array_base<T, A>::kMaxSize;
template <typename T, typename A>
    const int compact_array_base<T, A>::kExponentStart;

// compact_array:  Wrapper for compact_array_base that provides the
//  constructors and destructor.

template <class T,
          class A = gtl::
                    libc_allocator_with_realloc<T> >
class compact_array : public compact_array_base<T, A> {
 private:
  typedef compact_array_base<T, A> Base;

 public:
  typedef typename Base::value_type value_type;
  typedef typename Base::allocator_type allocator_type;
  typedef typename Base::pointer pointer;
  typedef typename Base::const_pointer const_pointer;
  typedef typename Base::reference reference;
  typedef typename Base::const_reference const_reference;
  typedef typename Base::size_type size_type;

  typedef typename Base::iterator iterator;
  typedef typename Base::const_iterator const_iterator;
  typedef typename Base::reverse_iterator reverse_iterator;
  typedef typename Base::const_reverse_iterator const_reverse_iterator;

  compact_array() noexcept(noexcept(std::declval<Base&>().Init())) {
    Base::Init();
  }

  explicit compact_array(size_type n) {
    Base::Construct(n, value_type());
  }

  compact_array(size_type n, const value_type& v) {
    Base::Construct(n, v);
  }

  // See 23.1.1/9 in the C++ standard for an explanation.
  template <typename Iterator>
  compact_array(Iterator first, Iterator last) {
    Base::Copy(first, last);
  }

  compact_array(const compact_array& v) {
    Base::CopyFrom(v);
  }

  compact_array(compact_array&& v) noexcept(
      noexcept(compact_array()) && noexcept(std::declval<Base&>().swap(v)))
      : compact_array() {
    Base::swap(v);
  }

  compact_array& operator=(const compact_array& v) {
    Base::AssignFrom(v);
    return *this;
  }

  compact_array& operator=(compact_array&& v) {
    // swap is only right here because the objects are trivially destructible
    // and thus there are no side effects on their destructor.
    // Otherwise we must destroy the objects on `this`.
    Base::swap(v);
    return *this;
  }

  ~compact_array() {
    Base::Destruct();
  }
};


// Comparison operators
template <typename T, typename A>
bool operator==(const compact_array<T, A>& x, const compact_array<T, A>& y) {
  return x.size() == y.size() &&
         std::equal(x.begin(), x.end(), y.begin());
}

template <typename T, typename A>
bool operator!=(const compact_array<T, A>& x, const compact_array<T, A>& y) {
  return !(x == y);
}

template <typename T, typename A>
bool operator<(const compact_array<T, A>& x, const compact_array<T, A>& y) {
  return std::lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());
}

template <typename T, typename A>
bool operator>(const compact_array<T, A>& x, const compact_array<T, A>& y) {
  return y < x;
}

template <typename T, typename A>
bool operator<=(const compact_array<T, A>& x, const compact_array<T, A>& y) {
  return !(y < x);
}

template <typename T, typename A>
bool operator>=(const compact_array<T, A>& x, const compact_array<T, A>& y) {
  return !(x < y);
}

// Swap
template <typename T, typename A>
inline void swap(compact_array<T, A>& x, compact_array<T, A>& y) {
  x.swap(y);
}

namespace compact_array_internal {
struct LogArray : public gtl::LogLegacyUpTo100 {
  template <typename ElementT>
  void Log(std::ostream& out, const ElementT& element) const {  // NOLINT
    out << element;
  }
  void Log(std::ostream& out, int8 c) const {  // NOLINT
    out << static_cast<int32>(c);
  }
  void Log(std::ostream& out, uint8 c) const {  // NOLINT
    out << static_cast<uint32>(c);
  }

  void LogOpening(std::ostream& out) const { out << "["; }  // NOLINT
  void LogClosing(std::ostream& out) const { out << "]"; }  // NOLINT
};
}  // namespace compact_array_internal

// Output operator for compact_array<T>. Requires that T has an
// operator<< for std::ostream.  Note that
// compact_array_internal::LogArray ensures that "signed char" and
// "unsigned char" types print as integers.
template <class T, class A>
std::ostream& operator<<(std::ostream& out, const compact_array<T, A>& array) {
  gtl::LogRangeToStream(out, array.begin(), array.end(),
                        compact_array_internal::LogArray());
  return out;
}

}  // namespace gtl

#endif  // S2_UTIL_GTL_COMPACT_ARRAY_H_
