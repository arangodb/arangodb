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
#include "type_utils.hpp"

NS_ROOT
NS_BEGIN(memory)

// ----------------------------------------------------------------------------
// --SECTION--                                                    is_shared_ptr
// ----------------------------------------------------------------------------

template<typename T>
struct is_shared_ptr : std::false_type {};

template<typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

///////////////////////////////////////////////////////////////////////////////
/// @brief dump memory statistics and stack trace to stderr
///////////////////////////////////////////////////////////////////////////////
IRESEARCH_API void dump_mem_stats_trace() NOEXCEPT;

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

  static const size_t alignment_value = ALIGNOF(type);
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
  T* as() NOEXCEPT {
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
  const T* as() const NOEXCEPT {
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
  void destroy() NOEXCEPT {
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

  allocator_deallocator(const allocator_type& alloc)
    : allocator_ref_t(alloc) {
  }

  void operator()(pointer p) const NOEXCEPT {
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

  allocator_deleter(allocator_type& alloc)
    : allocator_ref_t(alloc) {
  }

  void operator()(pointer p) const NOEXCEPT {
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

  allocator_array_deallocator(const allocator_type& alloc, size_t size)
    : allocator_ref_t(alloc), size_(size) {
  }

  void operator()(pointer p) const NOEXCEPT {
    auto& alloc = const_cast<allocator_ref_t*>(this)->get();

    // deallocate storage
    std::allocator_traits<allocator_type>::deallocate(
       alloc, p, size_
    );
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

  allocator_array_deleter(allocator_type& alloc, size_t size)
    : allocator_ref_t(alloc), size_(size) {
  }

  void operator()(pointer p) const NOEXCEPT {
    typedef std::allocator_traits<allocator_type> traits_t;

    auto& alloc = const_cast<allocator_ref_t*>(this)->get();

    // destroy objects
    for (auto end = p + size_; p != end; ++p) {
      traits_t::destroy(alloc, p);
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
struct managed_deleter {
 public:
  managed_deleter(T* ptr = nullptr) NOEXCEPT
    : ptr_(ptr) {
  }

  managed_deleter(managed_deleter&& rhs) NOEXCEPT
    : ptr_(rhs.ptr_) {
    rhs.ptr_ = nullptr;
  }

  managed_deleter& operator=(managed_deleter&& rhs) NOEXCEPT {
    if (this != &rhs) {
      ptr_ = rhs.ptr_;
      rhs.ptr_ = nullptr;
    }
    return *this;
  }

  void operator()(T*) NOEXCEPT {
    delete ptr_;
  }

 private:
  T* ptr_;
}; // managed_deleter

template <typename T, bool Manage = true>
inline typename std::enable_if<
  !std::is_array<T>::value,
  std::unique_ptr<T, managed_deleter<T>>
>::type make_managed(T* ptr) NOEXCEPT {
  return std::unique_ptr<T, managed_deleter<T>>(ptr, Manage ? ptr : nullptr);
}

template <typename T, bool Manage = true>
inline typename std::enable_if<
  !std::is_array<T>::value,
  std::unique_ptr<T, managed_deleter<T>>
>::type make_managed(std::unique_ptr<T>&& ptr) NOEXCEPT {
  auto* p = Manage ? ptr.release() : ptr.get();
  return std::unique_ptr<T, managed_deleter<T>>(p, Manage ? p : nullptr);
}

#define DECLARE_MANAGED_PTR(class_name) typedef std::unique_ptr<class_name, memory::managed_deleter<class_name> > ptr

// ----------------------------------------------------------------------------
// --SECTION--                                                      make_unique
// ----------------------------------------------------------------------------

template <typename T, typename... Types>
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

template<typename T, typename Alloc>
typename std::enable_if<
  std::is_array<T>::value && std::extent<T>::value == 0,
  std::unique_ptr<T, allocator_array_deleter<Alloc>>
>::type allocate_unique(Alloc& alloc, size_t size) {
  typedef std::allocator_traits<
    typename std::remove_cv<Alloc>::type
  > traits_t;
  typedef typename traits_t::pointer pointer;

  if (!size) {
    return nullptr;
  }

  pointer p;

  try {
    p = alloc.allocate(size); // allocate space for 'size' object
  } catch (std::bad_alloc&) {
    fprintf(
      stderr,
      "Memory allocation failure while creating and initializing " IR_SIZE_T_SPECIFIER " object(s) of size " IR_SIZE_T_SPECIFIER " bytes\n",
      size, sizeof(T)
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

  return std::unique_ptr<T[], allocator_array_deleter<Alloc>>(
    p, allocator_array_deleter<Alloc>(alloc)
  );
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
    return std::make_shared<Class>(std::forward<Args>(args)...);
  }
};

template<typename Class>
struct maker<Class, false> {
  template<typename... Args>
  static typename Class::ptr make(Args&&... args) {
    return typename Class::ptr(new Class(std::forward<Args>(args)...));
  }
};

NS_END // memory
NS_END // ROOT

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

#define PTR_NAMED_NOTHROW(class_type, name, ...) \
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
  }

#define DECLARE_SPTR(class_name) typedef std::shared_ptr<class_name> ptr
#define DECLARE_PTR(class_name) typedef std::unique_ptr<class_name> ptr
#define DECLARE_REF(class_name) typedef std::reference_wrapper<class_name> ref
#define DECLARE_CREF(class_name) typedef std::reference_wrapper<const class_name> cref

#define DECLARE_FACTORY(class_name) \
template<typename Class, bool> friend struct irs::memory::maker; \
template<typename _T, typename... _Args> \
static ptr make(_Args&&... args) { \
  typedef typename std::enable_if<std::is_base_of<class_name, _T>::value, _T>::type type; \
  try { \
    typedef irs::memory::maker<type> maker_t; \
    return maker_t::template make(std::forward<_Args>(args)...); \
  } catch (std::bad_alloc&) { \
    fprintf( \
      stderr, \
      "Memory allocation failure while creating and initializing an object of size " IR_SIZE_T_SPECIFIER " bytes\n", \
      sizeof(type) \
    ); \
    ::iresearch::memory::dump_mem_stats_trace(); \
    throw; \
  } \
}

//////////////////////////////////////////////////////////////////////////////
/// @brief default implementation of a factory method, instantiation on heap
///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
//////////////////////////////////////////////////////////////////////////////
#define DECLARE_FACTORY_DEFAULT(...) static ptr make(__VA_ARGS__);
#define DEFINE_FACTORY_DEFAULT(class_type) \
/*static*/ class_type::ptr class_type::make() { \
  PTR_NAMED(class_type, ptr); \
  return ptr; \
}

//////////////////////////////////////////////////////////////////////////////
/// @brief implementation of a factory method, using a deque to store and
///        reuse instances with the help of a skip-list style offset free_list
///        use std::deque as a non-reordering block-reserving container
///        user should #include all required dependencies e.g. <deque>,<mutex>
///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
//////////////////////////////////////////////////////////////////////////////
#define DEFINE_FACTORY_POOLED(class_type) \
/*static*/ class_type::ptr class_type::make() { \
  static const size_t free_list_empty = std::numeric_limits<size_t>::max(); \
  static size_t free_list_head = free_list_empty; \
  static std::mutex mutex; \
  static std::deque<std::pair<class_type, size_t>> pool; \
  class_type::ptr::element_type* entry; \
  size_t entry_pos; \
  std::lock_guard<std::mutex> lock(mutex); \
  if (free_list_empty == free_list_head) { \
    entry_pos = pool.size(); \
    entry = &(pool.emplace(pool.end(), class_type(), free_list_empty)->first); \
  } else { \
    auto& entry_pair = pool[free_list_head]; \
    entry = &(entry_pair.first); \
    entry_pos = free_list_head; \
    free_list_head = entry_pair.second; \
  } \
  return class_type::ptr( \
    entry, \
    [entry_pos](class_type::ptr::element_type*)->void { \
      std::lock_guard<std::mutex> lock(mutex); \
      pool[entry_pos].second = free_list_head; \
      free_list_head = entry_pos; \
    } \
  ); \
}

//////////////////////////////////////////////////////////////////////////////
/// @brief implementation of a factory method, returning a singleton instance
///        NOTE: make(...) MUST be defined in CPP to ensire proper code scope
//////////////////////////////////////////////////////////////////////////////
#define DEFINE_FACTORY_SINGLETON(class_type) \
/*static*/ class_type::ptr class_type::make() { \
  struct make_impl_t { \
    static class_type::ptr make() { PTR_NAMED(class_type, ptr); return ptr; } \
  }; \
  static auto instance = make_impl_t::make(); \
  return instance; \
}

#endif
