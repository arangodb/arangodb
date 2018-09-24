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

#ifndef IRESEARCH_EBO_H
#define IRESEARCH_EBO_H

#include <functional>
#include <type_traits>

#include "shared.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class compact (empty base optimization)
/// @brief Class provides unified way of implementing empty base optimization
///        idiom.
///
///        Example:
///          struct less {
///            bool operator<<(int a, int b) const { return a < b; } 
///          };
///
///          struct binded_comparer : private emo<0, less> {
///            typedef ebo<0, less> my_comparator_t;
///
///            binded_comparer(int a) : a(a) { }
///
///            const my_comparator& get_comparator() const {
///              return my_comparator::get();
///            }
///
///            bool operator(int b) { return get_comparator()(a,b); }
///
///            int a;
///          };
///
///          static_assert(
///            sizeof(binded_comparer)==sizeof(int),
///            "Please, use ebo"
///          );
//////////////////////////////////////////////////////////////////////////////
template<
  size_t I,
  typename T,
#if defined(__cpp_lib_is_final)
  bool = std::is_empty<T>::value && !std::is_final<T>::value
#else
  bool = std::is_empty<T>::value
#endif
> class compact : public T {
 public:
  typedef T type;
  static const size_t index = I;

  compact() = default;
  compact(const compact&) = default;

  template<typename U = T>
  compact(U&&) NOEXCEPT { }

  compact& operator=(const compact&) = default;
  compact& operator=(compact&&) NOEXCEPT { return *this; }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  CONSTEXPR T& get() NOEXCEPT { return *this; }
#else
  // before c++14 constexpr member function
  // gets const implicitly
  T& get() NOEXCEPT { return *this; }
#endif
  CONSTEXPR const T& get() const NOEXCEPT { return *this; }
}; // compact

template<size_t I, typename T>
class compact<I, T, false> {
 public:
  typedef T type;
  static const size_t index = I;

  compact() = default;
  compact(const compact& rhs) NOEXCEPT
    : val_(rhs.val_) {}
  compact(compact&& rhs) NOEXCEPT
    : val_(std::move(rhs.val_)) { }

  template<typename U = T>
  compact(U&& value) NOEXCEPT
    : val_(std::forward<U>(value)) {
  }

  compact& operator=(const compact&) = default;
  compact& operator=(compact&& rhs) NOEXCEPT {
    if (this != &rhs) {
      val_ = std::move(rhs);
    }
    return *this;
  }
  compact& operator=(const T& value) NOEXCEPT {
    val_ = value;
    return *this;
  }
  compact& operator=(T&& value) NOEXCEPT {
    val_ = std::move(value);
    return *this;
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  CONSTEXPR T& get() NOEXCEPT { return val_; }
#else
  // before c++14 constexpr member function
  // gets const implicitly
  T& get() NOEXCEPT { return val_; }
#endif
  CONSTEXPR const T& get() const NOEXCEPT { return val_; }

 private:
  T val_;
}; // compact

//////////////////////////////////////////////////////////////////////////////
/// @class compact_ref (empty base optimization)
/// @brief Class provides unified way of implementing empty base optimization
///        for references (can't use default template argmuments in partial
///         specialization)
//////////////////////////////////////////////////////////////////////////////
template<
  size_t I,
  typename T,
#if defined(__cpp_lib_is_final)
  bool = std::is_empty<T>::value && !std::is_final<T>::value
#else
  bool = std::is_empty<T>::value
#endif
> class compact_ref : public T {
 public:
  typedef T type;
  static const size_t index = I;

  compact_ref() = default;
  compact_ref(const compact_ref&) = default;

  template<typename U = T>
  compact_ref(U&&) NOEXCEPT { }

  compact_ref& operator=(const compact_ref&) = default;

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  CONSTEXPR T& get() NOEXCEPT { return *this; }
#else
  // before c++14 constexpr member function
  // gets const implicitly
  T& get() NOEXCEPT { return *this; }
#endif
  CONSTEXPR const T& get() const NOEXCEPT { return *this; }
}; // compact_ref

template<size_t I, typename T>
class compact_ref<I, T, false> {
 public:
  typedef T type;
  static const size_t index = I;

  compact_ref() = default;
  compact_ref(const compact_ref& rhs) = default;

  template<typename U = T>
  compact_ref(const U& value) NOEXCEPT
    : val_(const_cast<U*>(&value)) {
  }

  template<typename U = T>
  compact_ref(U& value) NOEXCEPT
    : val_(&value) {
  }

  compact_ref& operator=(const compact_ref&) = default;

  template<typename U = T>
  compact_ref& operator=(const U& value) NOEXCEPT {
    val_ = const_cast<U*>(&value);
    return *this;
  }

  template<typename U = T>
  compact_ref& operator=(U&& value) NOEXCEPT {
    val_ = &value;
    return *this;
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  CONSTEXPR T& get() NOEXCEPT { return *val_; }
#else
  // before c++14 constexpr member function
  // gets const implicitly
  T& get() NOEXCEPT { return *val_; }
#endif
  CONSTEXPR const T& get() const NOEXCEPT { return *val_; }

 private:
  T* val_;
}; // compact_ref

//////////////////////////////////////////////////////////////////////////////
/// @class compact_pair
//////////////////////////////////////////////////////////////////////////////
template<typename T0, typename T1>
class compact_pair : private compact<0, T0>, private compact<1, T1> {
 public:
  typedef compact<0, T0> first_compressed_t;
  typedef compact<1, T1> second_compressed_t;
  typedef typename first_compressed_t::type first_type;
  typedef typename second_compressed_t::type second_type;

  compact_pair() = default;
  compact_pair(const compact_pair&) = default;
  compact_pair(compact_pair&& rhs) NOEXCEPT
    : first_compressed_t(std::move(rhs.first())),
      second_compressed_t(std::move(rhs.second())) {
  }

  template<typename U0 = T0, typename U1 = T1>
  compact_pair(U0&& v0, U1&& v1) 
    : first_compressed_t(std::forward<U0>(v0)), 
      second_compressed_t(std::forward<U1>(v1)) { 
  }

  compact_pair& operator=(const compact_pair&) = default;
  compact_pair& operator=(compact_pair&& rhs) NOEXCEPT {
    if (this != &rhs) {
      first_compressed_t::operator=(std::move(rhs.first()));
      second_compressed_t::operator=(std::move(rhs.second()));
    }
    return *this;
  }

  const first_type& first() const NOEXCEPT {
    return first_compressed_t::get();
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  first_type& first() NOEXCEPT {
    return first_compressed_t::get();
  }
#else
  first_type& first() NOEXCEPT {
    // force the c++11 compiler to choose constexpr version of "get"
    return const_cast<first_type&>(
      const_cast<const compact_pair&>(*this).first()
    );
  }
#endif

  const second_type& second() const NOEXCEPT {
    return second_compressed_t::get();
  }

#if IRESEARCH_CXX > IRESEARCH_CXX_11
  second_type& second() NOEXCEPT {
    return second_compressed_t::get();
  }
#else
  second_type& second() NOEXCEPT {
    // force the c++11 compiler to choose constexpr version of "get"
    return const_cast<second_type&>(
      const_cast<const compact_pair&>(*this).second()
    );
  }
#endif
}; // compact_pair

template<typename T0, typename T1>
compact_pair<T0, T1> make_compact_pair(T0&& v0, T1&& v1) {
  return compact_pair<T0, T1>(std::forward<T0>(v0), std::forward<T1>(v1));
}

NS_END

#endif
