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

#ifndef IRESEARCH_STRING_H
#define IRESEARCH_STRING_H

#include "shared.hpp"

#include <cmath>
#include <cstring>
#include <cassert>
#include <algorithm>

// ----------------------------------------------------------------------------
// --SECTION--                                                   std extensions
// ----------------------------------------------------------------------------

NS_BEGIN(std)

// MSVC++ > v14.0 (Visual Studio >2015) already implements this in <xstring>
// MacOS requires this definition to be before first usage (i.e. in bytes_ref)
#if !defined(_MSC_VER) || (_MSC_VER <= 1900)
template<>
struct char_traits<::iresearch::byte_type> {
  typedef ::iresearch::byte_type char_type;
  typedef int int_type;
  typedef std::streamoff off_type;
  typedef std::streampos pos_type;

  static void assign(char_type& dst, const char_type& src) NOEXCEPT {
    dst = src;
  }

  static char_type* assign(char_type* ptr, size_t count, char_type ch) NOEXCEPT {
    assert(ptr);
    return reinterpret_cast<char_type*>(std::memset(ptr, ch, count));
  }

  static int compare(const char_type* lhs, const char_type* rhs, size_t count) NOEXCEPT {
    if (0 == count) {
      return 0;
    }

    assert(lhs && rhs);
    return std::memcmp(lhs, rhs, count);
  }

  static char_type* copy(char_type* dst, const char_type* src, size_t count) NOEXCEPT {
    if (0 == count) {
      return dst;
    }

    assert(dst && src);
    return reinterpret_cast<char_type*>(std::memcpy(dst, src, count));
  }

  static int_type eof() NOEXCEPT { return -1; }

  static bool eq(char_type lhs, char_type rhs) NOEXCEPT { return lhs == rhs; }

  static bool eq_int_type(int_type lhs, int_type rhs) NOEXCEPT { return lhs == rhs; }

  static const char_type* find(const char_type* ptr, size_t count, const char_type& ch) NOEXCEPT {
    if (0 == count) {
      return nullptr;
    }

    assert(ptr);
    return reinterpret_cast<char_type const*>(std::memchr(ptr, ch, count));
  }

  static size_t length(const char_type* /*ptr*/) NOEXCEPT {
    // binary string length cannot be determined from binary content
    assert(false);
    return (std::numeric_limits<size_t>::max)();
  }

  static bool lt(char_type lhs, char_type rhs) NOEXCEPT { return lhs < rhs; }

  static char_type* move(char_type* dst, const char_type* src, size_t count) NOEXCEPT {
    if (0 == count) {
      return dst;
    }

    assert(dst && src);
    return reinterpret_cast<char_type*>(std::memmove(dst, src, count));
  }

  static int_type not_eof(int_type i) NOEXCEPT { return i != eof(); }

  static char_type to_char_type(int_type i) NOEXCEPT {
    assert(int_type(char_type(i)) == i);
    return char_type(i);
  }

  static int_type to_int_type(char_type ch) NOEXCEPT { return ch; }

  MSVC_ONLY(static void _Copy_s(char_type* /*dst*/, size_t /*dst_size*/, const char_type* /*src*/, size_t /*src_size*/) { assert(false); });
}; // char_traits
#endif

NS_END // std

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                               binary std::string
// ----------------------------------------------------------------------------

typedef std::basic_string<byte_type> bstring;

//////////////////////////////////////////////////////////////////////////////
/// @class basic_string_ref
//////////////////////////////////////////////////////////////////////////////
template<typename Elem, typename Traits = std::char_traits<Elem>>
class basic_string_ref {
 public:
  typedef Traits traits_type;
  typedef Elem char_type;

  // beware of performing comparison against NIL value,
  // it may cause undefined behaviour in std::char_traits<Elem>
  // (e.g. becuase of memcmp function)
  IRESEARCH_HELPER_DLL_LOCAL static const basic_string_ref NIL; // null string
  IRESEARCH_HELPER_DLL_LOCAL static const basic_string_ref EMPTY; // empty string

  CONSTEXPR basic_string_ref() NOEXCEPT
    : data_(nullptr), size_(0) {
  }

  // Constructs a string reference object from a ref and a size.
  basic_string_ref(const basic_string_ref& ref, size_t size) NOEXCEPT
    : data_(ref.data_), size_(size) {
    IRS_ASSERT(size <= ref.size_);
  }

  // Constructs a string reference object from a C string and a size.
  CONSTEXPR basic_string_ref(const char_type* s, size_t size) NOEXCEPT
    : data_(s), size_(size) {
  }

  // Constructs a string reference object from a C string computing
  // the size with ``std::char_traits<Char>::length``.
  CONSTEXPR basic_string_ref(const char_type* s) NOEXCEPT
    : data_(s), size_(s ? traits_type::length(s) : 0) {
  }

  CONSTEXPR basic_string_ref(const std::basic_string<char_type>& s) NOEXCEPT
    : data_(s.c_str()), size_(s.size()) {
  }

  CONSTEXPR basic_string_ref(const std::basic_string<char_type>& str, size_t size) NOEXCEPT
    : data_(str.c_str()), size_(size) {
  }

  CONSTEXPR const char_type& operator[](size_t i) const NOEXCEPT {
    return IRS_ASSERT(i < size_), data_[i];
  }

  // Returns the pointer to a C string.
  CONSTEXPR const char_type* c_str() const NOEXCEPT { return data_; }

  // Returns the string size.
  CONSTEXPR size_t size() const NOEXCEPT { return size_; }

  CONSTEXPR bool null() const NOEXCEPT { return nullptr == data_; }
  CONSTEXPR bool empty() const NOEXCEPT { return null() || 0 == size_; }
  CONSTEXPR const char_type* begin() const NOEXCEPT{ return data_; }
  CONSTEXPR const char_type* end() const NOEXCEPT{ return data_ + size_; }

  CONSTEXPR operator std::basic_string<char_type>() const {
    return std::basic_string<char_type>(data_, size_);
  }

  // friends
  friend int compare(
      const basic_string_ref& lhs,
      const char_type* rhs,
      size_t rhs_size
  ) {
    const size_t lhs_size = lhs.size();
    int r = traits_type::compare( 
      lhs.c_str(), rhs, 
      (std::min)(lhs_size, rhs_size)
    );

    if (!r) {
      r = int(lhs_size - rhs_size);
    }

    return r;
  }

  friend int compare(
      const basic_string_ref& lhs, const std::basic_string<char_type>& rhs
  ) {
    return compare(lhs, rhs.c_str(), rhs.size());
  }

  friend int compare(const basic_string_ref& lhs, const char_type* rhs) {
    return compare(lhs, rhs, traits_type::length(rhs));
  }

  friend int compare(const basic_string_ref& lhs, const basic_string_ref& rhs) {
    return compare(lhs, rhs.c_str(), rhs.size());
  }

  friend bool operator<(const basic_string_ref& lhs, const basic_string_ref& rhs) {
    return compare(lhs, rhs) < 0;
  }

  friend bool operator<(
      const std::basic_string<char_type>& lhs,
      const basic_string_ref& rhs
  ) {
    return lhs.compare(0, std::basic_string<char_type>::npos, rhs.c_str(), rhs.size()) < 0;
  }
 
  friend bool operator>=(const basic_string_ref& lhs, const basic_string_ref& rhs) {
    return !(lhs < rhs);
  }

  friend bool operator>(const basic_string_ref& lhs, const basic_string_ref& rhs) {
    return compare(lhs, rhs) > 0;
  }

  friend bool operator<=(const basic_string_ref& lhs, const basic_string_ref& rhs) {
    return !(lhs > rhs);
  }

  friend bool operator==(const basic_string_ref& lhs, const basic_string_ref& rhs) {
    return 0 == compare(lhs, rhs);
  }

  friend bool operator!=(const basic_string_ref& lhs, const basic_string_ref& rhs) {
    return !(lhs == rhs);
  }

  friend std::basic_ostream<char_type, std::char_traits<char_type>>& operator<<(
      std::basic_ostream<char_type,
      std::char_traits<char_type>>& os,
      const basic_string_ref& d
  ) {
    return os.write( d.c_str(), d.size() );
  }

 protected:
  const char_type* data_;
  size_t size_;
}; // basic_string_ref

template<typename Elem, typename Traits>
/*static*/ const basic_string_ref<Elem, Traits> basic_string_ref<Elem, Traits >::NIL(
  nullptr, 0
);

template<typename Elem, typename Traits>
/*static*/ const basic_string_ref<Elem, Traits> basic_string_ref<Elem, Traits >::EMPTY(
  reinterpret_cast<const Elem*>(""), 0 // FIXME
);

template class IRESEARCH_API basic_string_ref<char>;
template class IRESEARCH_API basic_string_ref<byte_type>;

template< typename _Elem, typename _Traits >
inline bool starts_with(
    const basic_string_ref<_Elem, _Traits >& first,
    const _Elem* second, size_t second_size) {
  typedef typename basic_string_ref <
    _Elem, _Traits > ::traits_type traits_type;

  return first.size() >= second_size
    && 0 == traits_type::compare(first.c_str(), second, second_size);
}

template< typename _Elem, typename _Traits >
inline bool starts_with(
    const std::basic_string<_Elem>& first, 
    const basic_string_ref<_Elem, _Traits>& second) {
  return 0 == first.compare(0, second.size(), second.c_str(), second.size());
}

template<typename _Elem>
inline bool starts_with(
    const std::basic_string<_Elem>& first,
    const std::basic_string<_Elem>& second) {
  return 0 == first.compare(0, second.size(), second.c_str(), second.size());
}

template< typename _Elem, typename _Traits >
inline bool starts_with( const basic_string_ref< _Elem, _Traits >& first,
                         const _Elem* second ) {
  return starts_with( first, second, _Traits::length( second ) );
}

template<typename Elem, typename Traits>
inline bool starts_with(
  const basic_string_ref<Elem, Traits>& first,
  const std::basic_string<Elem>& second
) {
  return starts_with(first, second.c_str(), second.size());
}

template< typename _Elem, typename _Traits >
inline bool starts_with( const basic_string_ref< _Elem, _Traits >& first,
                         const basic_string_ref< _Elem, _Traits >& second ) {
  return starts_with( first, second.c_str(), second.size() );
}

typedef basic_string_ref<char> string_ref;
typedef basic_string_ref<byte_type> bytes_ref;

template<typename _ElemDst, typename _ElemSrc>
CONSTEXPR inline basic_string_ref<_ElemDst> ref_cast(const basic_string_ref<_ElemSrc>& src) {
  return basic_string_ref<_ElemDst>(reinterpret_cast<const _ElemDst*>(src.c_str()), src.size());
}

template<typename ElemDst, typename ElemSrc>
CONSTEXPR inline basic_string_ref<ElemDst> ref_cast(const std::basic_string<ElemSrc>& src) {
  return basic_string_ref<ElemDst>(reinterpret_cast<const ElemDst*>(src.c_str()), src.size());
}

// ----------------------------------------------------------------------------
// --SECTION--                                        String hashing algorithms
// ----------------------------------------------------------------------------

NS_BEGIN(hash_utils)

IRESEARCH_API size_t hash(const irs::bstring& value);
IRESEARCH_API size_t hash(const char* value);
IRESEARCH_API size_t hash(const wchar_t* value);
IRESEARCH_API size_t hash(const bytes_ref& value);
IRESEARCH_API size_t hash(const string_ref& value);

NS_END // hash_utils

NS_END // NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                                   std extensions
// ----------------------------------------------------------------------------

NS_BEGIN(std)

template<>
struct hash<char*> {
  size_t operator()(const char* value) const {
    return ::iresearch::hash_utils::hash(value);
  }
}; // hash

template<>
struct hash<wchar_t*> {
  size_t operator()(const wchar_t* value) const {
    return ::iresearch::hash_utils::hash(value);
  }
}; // hash

template<>
struct hash<::iresearch::bstring> {
  size_t operator()(const ::iresearch::bstring& value) const {
    return ::iresearch::hash_utils::hash(value);
  }
}; // hash

template<>
struct hash<::iresearch::bytes_ref> {
  size_t operator()(const ::iresearch::bytes_ref& value) const {
    return ::iresearch::hash_utils::hash(value);
  }
}; // hash

template<>
struct hash<::iresearch::string_ref> {
  size_t operator()(const ::iresearch::string_ref& value) const {
    return ::iresearch::hash_utils::hash(value);
  }
}; // hash

NS_END // std

#endif
