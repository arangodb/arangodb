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

#ifndef IRESEARCH_MEMORY_H
#define IRESEARCH_MEMORY_H

#include <memory>

#include "shared.hpp"
#include "ebo.hpp"
#include "log.hpp"
#include "noncopyable.hpp"
#include "math_utils.hpp"
#include "type_utils.hpp"

namespace iresearch {
namespace memory {

inline constexpr size_t align_up(size_t size, size_t alignment) noexcept {
#if defined(_MSC_VER) && (_MSC_VER < 1900)
  assert(math::is_power2(alignment));
  return (size + alignment - 1) & (0 - alignment);
#else
  return IRS_ASSERT(math::is_power2(alignment)),
         (size + alignment - 1) & (0 - alignment);
#endif
}

// ----------------------------------------------------------------------------
// --SECTION--                                                    is_shared_ptr
// ----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// @brief dump memory statistics and stack trace to stderr
///////////////////////////////////////////////////////////////////////////////
IRESEARCH_API void dump_mem_stats_trace() noexcept;

///////////////////////////////////////////////////////////////////////////////
/// @class aligned_storage
/// @brief same as 'std::aligned_storage' but MSVC doesn't honor alignment on
/// MSVC2013, 2017 (prior 15.8)
///////////////////////////////////////////////////////////////////////////////
template<size_t Size, size_t Alignment>
class aligned_storage {
 private:
  #if defined(_MSC_VER) && (_MSC_VER < 1900)
    // as per MSVC documentation:
    // align(#) valid entries are integer powers of two from 1 to 8192 (bytes)
    // e.g. 2, 4, 8, 16, 32, or 64
    #pragma warning(disable: 4324) // structure was padded due to __declspec(align())
    template<size_t Align> struct align_t {};
    template<> struct alignas(1)    align_t<1> { };
    template<> struct alignas(2)    align_t<2> { };
    template<> struct alignas(4)    align_t<4> { };
    template<> struct alignas(8)    align_t<8> { };
    template<> struct alignas(16)   align_t<16> { };
    template<> struct alignas(32)   align_t<32> { };
    template<> struct alignas(64)   align_t<64> { };
    template<> struct alignas(128)  align_t<128> { };
    template<> struct alignas(256)  align_t<256> { };
    template<> struct alignas(512)  align_t<512> { };
    template<> struct alignas(1024) align_t<1024> { };
    template<> struct alignas(2048) align_t<2048> { };
    template<> struct alignas(4096) align_t<4096> { };
    template<> struct alignas(8192) align_t<8192> { };
    #pragma warning(default: 4324)
  #else
    template<size_t Align> struct alignas(Align) align_t { };
  #endif

  static_assert(alignof(align_t<Alignment>) == Alignment, "alignof(align_t<Alignment>) != Alignment");

 public:
  union {
    align_t<Alignment> align_;
    char data[MSVC_ONLY(!Size ? 1 :) Size];
  };
}; // aligned_storage

///////////////////////////////////////////////////////////////////////////////
/// @struct aligned_union
/// @brief Provides the member typedef type, which is a POD type of a size and
///        alignment suitable for use as uninitialized storage for an object of
///        any of the specified Types
///////////////////////////////////////////////////////////////////////////////
template<typename... Types>
struct aligned_union {
#if defined(_MSC_VER)
  typedef typename std::aligned_union<0, Types...>::type type;
#else // __GNUC__
  // GCC < 4.9 does not support std::aligned_union
  struct type {
    alignas(irs::template_traits_t<Types...>::align_max()) char raw[
      irs::template_traits_t<Types...>::size_max()
    ];
  };
#endif

  static const size_t alignment_value = alignof(type);
  static const size_t size_value =  sizeof(type);
}; // aligned_union 

///////////////////////////////////////////////////////////////////////////////
/// @struct aligned_type
/// @brief Provides the storage (POD type) that is suitable for use as
///        uninitialized storage for an object of  any of the specified Types
///////////////////////////////////////////////////////////////////////////////
template<typename... Types>
struct aligned_type {
  template<typename T>
  T* as() noexcept {
    #if defined(_MSC_VER) && (_MSC_VER < 1900)
      const bool result = irs::is_convertible<T, Types...>();
      assert(result);
    #else
      static_assert(
        irs::is_convertible<T, Types...>(),
        "T must be convertible to the specified Types"
      );
    #endif

    return reinterpret_cast<T*>(&storage);
  }

  template<typename T>
  const T* as() const noexcept {
    return const_cast<aligned_type&>(*this).as<T>();
  }

  template<typename T, typename... Args>
  void construct(Args&&... args) {
    #if defined(_MSC_VER) && (_MSC_VER < 1900)
      const bool result = irs::in_list<T, Types...>();
      assert(result);
    #else
      static_assert(
        irs::in_list<T, Types...>(),
        "T must be in the specified list of Types"
      );
    #endif

    new (as<T>()) T(std::forward<Args>(args)...);
  }

  template<typename T>
  void destroy() noexcept {
    #if defined(_MSC_VER) && (_MSC_VER < 1900)
      const bool result = irs::is_convertible<T, Types...>();
      assert(result);
    #else
      static_assert(
        irs::is_convertible<T, Types...>(),
        "T must be convertible to the specified Types"
      );
    #endif

    as<T>()->~T();
  }

  typename aligned_union<Types...>::type storage;
}; // aligned_type

// ----------------------------------------------------------------------------
// --SECTION--                                                         Deleters
// ----------------------------------------------------------------------------

template<typename Alloc>
struct allocator_deallocator : public compact_ref<0, Alloc> {
  typedef compact_ref<0, Alloc> allocator_ref_t;
  typedef typename allocator_ref_t::allocator_type allocator_type;
  typedef typename allocator_type::pointer pointer;

  allocator_deallocator(const allocator_type& alloc) noexcept
    : allocator_ref_t(alloc) {
  }

  void operator()(pointer p) const noexcept {
    auto& alloc = const_cast<allocator_ref_t*>(this)->get();

    // deallocate storage
    std::allocator_traits<allocator_type>::deallocate(
      alloc, p, 1
    );
  }
}; // allocator_deallocator

template<typename Alloc>
struct allocator_deleter : public compact_ref<0, Alloc> {
  typedef compact_ref<0, Alloc> allocator_ref_t;
  typedef typename allocator_ref_t::type allocator_type;
  typedef typename allocator_type::pointer pointer;

  allocator_deleter(allocator_type& alloc) noexcept
    : allocator_ref_t(alloc) {
  }

  void operator()(pointer p) const noexcept {
    typedef std::allocator_traits<allocator_type> traits_t;

    auto& alloc = const_cast<allocator_deleter*>(this)->get();

    // destroy object
    traits_t::destroy(alloc, p);

    // deallocate storage
    traits_t::deallocate(alloc, p, 1);
  }
}; // allocator_deleter

template<typename Alloc>
class allocator_array_deallocator : public compact_ref<0, Alloc> {
 public:
  typedef compact_ref<0, Alloc> allocator_ref_t;
  typedef typename allocator_ref_t::type allocator_type;
  typedef typename allocator_type::pointer pointer;

  allocator_array_deallocator(const allocator_type& alloc, size_t size) noexcept
    : allocator_ref_t(alloc), size_(size) {
  }

  void operator()(pointer p) const noexcept {
    typedef std::allocator_traits<allocator_type> traits_t;

    auto& alloc = const_cast<allocator_type&>(allocator_ref_t::get());

    // deallocate storage
    traits_t::deallocate(alloc, p, size_);
  }

 private:
  size_t size_;
}; // allocator_array_deallocator

template<typename Alloc>
class allocator_array_deleter : public compact_ref<0, Alloc> {
 public:
  typedef compact_ref<0, Alloc> allocator_ref_t;
  typedef typename allocator_ref_t::type allocator_type;
  typedef typename allocator_type::pointer pointer;

  allocator_array_deleter(allocator_type& alloc, size_t size) noexcept
    : allocator_ref_t(alloc), size_(size) {
  }

  void operator()(pointer p) const noexcept {
    typedef std::allocator_traits<allocator_type> traits_t;

    auto& alloc = const_cast<allocator_type&>(allocator_ref_t::get());

    // destroy objects
    for (auto begin = p, end = p + size_; begin != end; ++begin) {
      traits_t::destroy(alloc, begin);
    }

    // deallocate storage
    traits_t::deallocate(alloc, p, size_);
  }

 private:
  size_t size_;
}; // allocator_array_deleter

struct noop_deleter {
  template<typename T>
  void operator()(T*) { }
};

// ----------------------------------------------------------------------------
// --SECTION--                                                   managed unique
// ----------------------------------------------------------------------------

template<typename T>
struct managed_deleter : util::noncopyable {
 public:
  using value_type = T;
  using pointer = T*;

  explicit managed_deleter(pointer ptr = nullptr) noexcept
    : ptr_(ptr) {
  }

  template<
    typename U,
    typename = std::enable_if_t<std::is_convertible_v<U*, pointer>, U*>>
  managed_deleter(managed_deleter<U>&& rhs) noexcept
    : ptr_(rhs.ptr_) {
    rhs.ptr_ = nullptr;
  }

  managed_deleter(managed_deleter&& rhs) noexcept
    : ptr_(rhs.ptr_) {
    rhs.ptr_ = nullptr;
  }

  managed_deleter& operator=(managed_deleter&& rhs) noexcept {
    if (this != &rhs) {
      ptr_ = rhs.ptr_;
      rhs.ptr_ = nullptr;
    }
    return *this;
  }

  template<
    typename U,
    typename = std::enable_if_t<std::is_convertible_v<U*, pointer>, U*>>
  managed_deleter& operator=(managed_deleter<U>&& rhs) noexcept {
    ptr_ = rhs.ptr_;
    rhs.ptr_ = nullptr;
    return *this;
  }

  void operator()(pointer p) noexcept {
    assert(!ptr_ || p == ptr_);
    delete ptr_;
  }

  pointer get() const noexcept {
    return ptr_;
  }

 private:
  template<typename U>
  friend struct managed_deleter;

  pointer ptr_;
}; // managed_deleter

template<typename T>
using managed_ptr = std::unique_ptr<T, memory::managed_deleter<T>>;

template <typename T, bool Manage = true>
inline typename std::enable_if<
  !std::is_array<T>::value,
  managed_ptr<T>
>::type to_managed(T* ptr) noexcept {
  return managed_ptr<T>(ptr, Manage ? managed_deleter<T>(ptr) : managed_deleter<T>(nullptr));
}

template <typename T>
inline typename std::enable_if<
  !std::is_array<T>::value,
  managed_ptr<T>
>::type to_managed(std::unique_ptr<T>&& ptr) noexcept {
  auto* p = ptr.release();
  return managed_ptr<T>(p, managed_deleter<T>(p));
}

template<typename T, typename... Types>
inline typename std::enable_if<
  !std::is_array<T>::value,
  managed_ptr<T>
>::type make_managed(Types&&... Args) {
  try {
    return to_managed<T, true>(new T(std::forward<Types>(Args)...));
  } catch (std::bad_alloc&) {
    fprintf(
      stderr,
      "Memory allocation failure while creating and initializing an object of size " IR_SIZE_T_SPECIFIER " bytes\n",
      sizeof(T)
    );
    dump_mem_stats_trace();
    throw;
  }
}

#define DECLARE_MANAGED_PTR(class_name) typedef irs::memory::managed_ptr<class_name> ptr

// ----------------------------------------------------------------------------
// --SECTION--                                                      make_shared
// ----------------------------------------------------------------------------

template<typename T, typename... Args>
inline std::shared_ptr<T> make_shared(Args&&... args) {
  try {
    return std::make_shared<T>(std::forward<Args>(args)...);
  } catch (std::bad_alloc&) {
    fprintf(
      stderr,
      "Memory allocation failure while creating and initializing an object of size " IR_SIZE_T_SPECIFIER " bytes\n",
      sizeof(T)
    );
    dump_mem_stats_trace();
    throw;
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                                      make_unique
// ----------------------------------------------------------------------------

template<typename T, typename... Types>
inline typename std::enable_if<
  !std::is_array<T>::value,
  std::unique_ptr<T>
>::type make_unique(Types&&... Args) {
  try {
    return std::unique_ptr<T>(new T(std::forward<Types>(Args)...));
  } catch (std::bad_alloc&) {
    fprintf(
      stderr,
      "Memory allocation failure while creating and initializing an object of size " IR_SIZE_T_SPECIFIER " bytes\n",
      sizeof(T)
    );
    dump_mem_stats_trace();
    throw;
  }
}

template<typename T>
inline typename std::enable_if<
  std::is_array<T>::value && std::extent<T>::value == 0,
  std::unique_ptr<T>
>::type make_unique(size_t size) {
  typedef typename std::remove_extent<T>::type value_type;

  try {
    return std::unique_ptr<T>(new value_type[size]());
  } catch (std::bad_alloc&) {
    fprintf(
      stderr,
      "Memory allocation failure while creating and initializing an array of " IR_SIZE_T_SPECIFIER " objects each of size " IR_SIZE_T_SPECIFIER " bytes\n",
      size, sizeof(value_type)
    );
    dump_mem_stats_trace();
    throw;
  }
}

template<typename T, typename... Types>
typename std::enable_if<
  std::extent<T>::value != 0,
  void
>::type make_unique(Types&&...) = delete;

// ----------------------------------------------------------------------------
// --SECTION--                                                  allocate_unique
// ----------------------------------------------------------------------------

template<typename T, typename Alloc, typename... Types>
inline typename std::enable_if<
  !std::is_array<T>::value,
  std::unique_ptr<T, allocator_deleter<Alloc>>
>::type allocate_unique(Alloc& alloc, Types&&... Args) {
  typedef std::allocator_traits<
    typename std::remove_cv<Alloc>::type
  > traits_t;
  typedef typename traits_t::pointer pointer;

  pointer p;

  try {
    p = alloc.allocate(1); // allocate space for 1 object
  } catch (std::bad_alloc&) {
    fprintf(
      stderr,
      "Memory allocation failure while creating and initializing an object of size " IR_SIZE_T_SPECIFIER " bytes\n",
      sizeof(T)
    );
    dump_mem_stats_trace();
    throw;
  }

  try {
    traits_t::construct(alloc, p, std::forward<Types>(Args)...); // construct object
  } catch (...) {
    alloc.deallocate(p, 1); // free allocated storage in case of any error during construction
    throw;
  }

  return std::unique_ptr<T, allocator_deleter<Alloc>>(
    p, allocator_deleter<Alloc>(alloc)
  );
}

template<
  typename T,
  typename Alloc
> typename std::enable_if<
  std::is_array<T>::value && std::extent<T>::value == 0,
  std::unique_ptr<T, allocator_array_deleter<Alloc>>
>::type allocate_unique(Alloc& alloc, size_t size) {
  typedef std::allocator_traits<
    typename std::remove_cv<Alloc>::type
  > traits_t;
  typedef typename traits_t::pointer pointer;
  typedef allocator_array_deleter<Alloc> deleter_t;
  typedef std::unique_ptr<T, deleter_t> unique_ptr_t;

  pointer p = nullptr;

  if (!size) {
    return unique_ptr_t(p, deleter_t(alloc, size));
  }

  try {
    p = alloc.allocate(size); // allocate space for 'size' object
  } catch (std::bad_alloc&) {
    fprintf(
      stderr,
      "Memory allocation failure while creating and initializing " IR_SIZE_T_SPECIFIER " object(s) of size " IR_SIZE_T_SPECIFIER " bytes\n",
      size, sizeof(typename traits_t::value_type)
    );
    dump_mem_stats_trace();
    throw;
  }

  auto begin = p;

  try {
    for (auto end = begin + size; begin != end; ++begin) {
      traits_t::construct(alloc, begin); // construct object
    }
  } catch (...) {
    // destroy constructed objects
    for ( ; p != begin; --begin) {
      traits_t::destroy(alloc, begin);
    }

    // free allocated storage in case of any error during construction
    alloc.deallocate(p, size);
    throw;
  }

  return unique_ptr_t(p, deleter_t(alloc, size));
}

// do not construct objects in a block
struct allocate_only_tag { };
static const auto allocate_only = allocate_only_tag();

template<
  typename T,
  typename Alloc
> typename std::enable_if<
  std::is_array<T>::value && std::extent<T>::value == 0,
  std::unique_ptr<T, allocator_array_deallocator<Alloc>>
>::type allocate_unique(Alloc& alloc, size_t size, allocate_only_tag) {
  typedef std::allocator_traits<
    typename std::remove_cv<Alloc>::type
  > traits_t;
  typedef typename traits_t::pointer pointer;
  typedef allocator_array_deallocator<Alloc> deleter_t;
  typedef std::unique_ptr<T, deleter_t> unique_ptr_t;

  pointer p = nullptr;

  if (!size) {
    return unique_ptr_t(p, deleter_t(alloc, size));
  }

  try {
    p = alloc.allocate(size); // allocate space for 'size' object
  } catch (std::bad_alloc&) {
    fprintf(
      stderr,
      "Memory allocation failure while creating and initializing " IR_SIZE_T_SPECIFIER " object(s) of size " IR_SIZE_T_SPECIFIER " bytes\n",
      size, sizeof(typename traits_t::value_type)
    );
    dump_mem_stats_trace();
    throw;
  }

  return unique_ptr_t(p, deleter_t(alloc, size));
}

// Decline wrong syntax
template<typename T, typename Alloc, typename... Types>
typename std::enable_if<
  std::extent<T>::value != 0,
  void
>::type allocate_unique(Alloc&, Types&&...) = delete;

// ----------------------------------------------------------------------------
// --SECTION--                                                            maker
// ----------------------------------------------------------------------------

template<typename Class, bool = is_shared_ptr<typename Class::ptr>::value>
struct maker {
  template<typename... Args>
  static typename Class::ptr make(Args&&... args) {
    // creates shared_ptr with a single heap allocation
    return irs::memory::make_shared<Class>(std::forward<Args>(args)...);
  }
};

template<typename Class>
struct maker<Class, false> {
  template<typename... Args>
  static typename Class::ptr make(Args&&... args) {
    static_assert(
      std::is_nothrow_constructible<
        typename Class::ptr,
        typename Class::ptr::element_type*>::value,
      "type must be nothrow constructible"
    );

    try {
      return typename Class::ptr(new Class(std::forward<Args>(args)...));
    } catch (std::bad_alloc&) {
      fprintf(
        stderr,
        "Memory allocation failure while creating and initializing an object of size " IR_SIZE_T_SPECIFIER " bytes\n",
        sizeof(Class)
      );
      ::iresearch::memory::dump_mem_stats_trace();
      throw;
    }
  }
};

} // memory
} // ROOT

#define PTR_NAMED(class_type, name, ...) \
  class_type::ptr name; \
  try { \
    name.reset(new class_type(__VA_ARGS__)); \
  } catch (const std::bad_alloc&) { \
    fprintf( \
      stderr, \
      "Memory allocation failure while creating and initializing an object of size " IR_SIZE_T_SPECIFIER " bytes\n", \
      sizeof(class_type) \
    ); \
    ::iresearch::memory::dump_mem_stats_trace(); \
    throw; \
  }

#define DECLARE_SHARED_PTR(class_name) \
  friend struct irs::memory::maker<class_name, true>; \
  typedef std::shared_ptr<class_name> ptr

#define DECLARE_UNIQUE_PTR(class_name) \
  friend struct irs::memory::maker<class_name, false>; \
  typedef std::unique_ptr<class_name> ptr

//////////////////////////////////////////////////////////////////////////////
/// @brief default inline implementation of a factory method, instantiation on
///        heap
//////////////////////////////////////////////////////////////////////////////
#define DEFINE_FACTORY_INLINE(class_name) \
template<typename Class, bool> friend struct irs::memory::maker; \
template<typename _T, typename... Args> \
static ptr make(Args&&... args) { \
  typedef typename std::enable_if<std::is_base_of<class_name, _T>::value, _T>::type type; \
  typedef irs::memory::maker<type> maker_t; \
  return maker_t::template make(std::forward<Args>(args)...); \
}

//////////////////////////////////////////////////////////////////////////////
/// @brief declaration of a factory method
//////////////////////////////////////////////////////////////////////////////
#define DECLARE_FACTORY(...) static ptr make(__VA_ARGS__)

//////////////////////////////////////////////////////////////////////////////
/// @brief default implementation of a factory method, instantiation on heap
///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
//////////////////////////////////////////////////////////////////////////////
#define DEFINE_FACTORY_DEFAULT(class_type) \
/*static*/ class_type::ptr class_type::make() { \
  return irs::memory::maker<class_type>::make(); \
}

#endif
