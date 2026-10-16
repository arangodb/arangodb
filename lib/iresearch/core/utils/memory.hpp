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

#pragma once

#include <memory>

#include "resource_manager.hpp"
#include "shared.hpp"
#include "utils/assert.hpp"
#include "utils/log.hpp"
#include "utils/math_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/type_utils.hpp"

namespace irs::memory {

constexpr size_t align_up(size_t size, size_t alignment) noexcept {
  IRS_ASSERT(math::is_power2(alignment));
  return (size + alignment - 1) & (0 - alignment);
}

// Dump memory statistics and stack trace to stderr
void dump_mem_stats_trace() noexcept;

// Same as 'std::aligned_storage' but MSVC doesn't honor alignment on
// MSVC2013, 2017 (prior 15.8)
template<size_t Size, size_t Alignment>
class aligned_storage {
 private:
#if defined(_MSC_VER) && (_MSC_VER < 1900)
// as per MSVC documentation:
// align(#) valid entries are integer powers of two from 1 to 8192 (bytes)
// e.g. 2, 4, 8, 16, 32, or 64
#pragma warning( \
  disable : 4324)  // structure was padded due to __declspec(align())
  template<size_t Align>
  struct align_t {};
  template<>
  struct alignas(1) align_t<1> {};
  template<>
  struct alignas(2) align_t<2> {};
  template<>
  struct alignas(4) align_t<4> {};
  template<>
  struct alignas(8) align_t<8> {};
  template<>
  struct alignas(16) align_t<16> {};
  template<>
  struct alignas(32) align_t<32> {};
  template<>
  struct alignas(64) align_t<64> {};
  template<>
  struct alignas(128) align_t<128> {};
  template<>
  struct alignas(256) align_t<256> {};
  template<>
  struct alignas(512) align_t<512> {};
  template<>
  struct alignas(1024) align_t<1024> {};
  template<>
  struct alignas(2048) align_t<2048> {};
  template<>
  struct alignas(4096) align_t<4096> {};
  template<>
  struct alignas(8192) align_t<8192> {};
#pragma warning(default : 4324)
#else
  template<size_t Align>
  struct alignas(Align) align_t {};
#endif

  static_assert(alignof(align_t<Alignment>) == Alignment,
                "alignof(align_t<Alignment>) != Alignment");

 public:
  union {
    align_t<Alignment> align_;
    char data[MSVC_ONLY(!Size ? 1 :) Size];
  };
};

// Provides the member typedef type, which is a POD type of a size and
// alignment suitable for use as uninitialized storage for an object of
// any of the specified Types
template<typename... Types>
struct aligned_union {
#if defined(_MSC_VER)
  typedef typename std::aligned_union<0, Types...>::type type;
#else  // __GNUC__
  // GCC < 4.9 does not support std::aligned_union
  struct type {
    alignas(irs::template_traits_t<Types...>::align_max()) char raw
      [irs::template_traits_t<Types...>::size_max()];
  };
#endif

  static constexpr size_t alignment_value = alignof(type);
  static constexpr size_t size_value = sizeof(type);
};

// Provides the storage (POD type) that is suitable for use as
// uninitialized storage for an object of  any of the specified Types
template<typename... Types>
struct aligned_type {
  template<typename T>
  T* as() noexcept {
#if defined(_MSC_VER) && (_MSC_VER < 1900)
    const bool result = irs::is_convertible<T, Types...>();
    IRS_ASSERT(result);
#else
    static_assert(irs::is_convertible<T, Types...>(),
                  "T must be convertible to the specified Types");
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
    IRS_ASSERT(result);
#else
    static_assert(irs::in_list<T, Types...>(),
                  "T must be in the specified list of Types");
#endif

    new (as<T>()) T(std::forward<Args>(args)...);
  }

  template<typename T>
  void destroy() noexcept {
#if defined(_MSC_VER) && (_MSC_VER < 1900)
    const bool result = irs::is_convertible<T, Types...>();
    IRS_ASSERT(result);
#else
    static_assert(irs::is_convertible<T, Types...>(),
                  "T must be convertible to the specified Types");
#endif

    as<T>()->~T();
  }

  typename aligned_union<Types...>::type storage;
};

template<typename Alloc>
class allocator_deallocator {
 public:
  using allocator_type = Alloc;
  using pointer = typename allocator_type::pointer;

  explicit allocator_deallocator(const allocator_type& alloc) : alloc_{alloc} {}

  void operator()(pointer p) noexcept {
    // deallocate storage
    std::allocator_traits<allocator_type>::deallocate(alloc_, p, 1);
  }

 private:
  IRS_NO_UNIQUE_ADDRESS allocator_type alloc_;
};

template<typename Alloc>
class allocator_deleter {
 public:
  using allocator_type = Alloc;
  using pointer = typename allocator_type::pointer;

  explicit allocator_deleter(const allocator_type& alloc) : alloc_{alloc} {}

  void operator()(pointer p) noexcept {
    using traits_t = std::allocator_traits<allocator_type>;

    // destroy object
    traits_t::destroy(alloc_, p);

    // deallocate storage
    traits_t::deallocate(alloc_, p, 1);
  }

 private:
  IRS_NO_UNIQUE_ADDRESS allocator_type alloc_;
};

template<typename Alloc>
class allocator_array_deallocator {
  using traits_t = std::allocator_traits<Alloc>;

 public:
  using allocator_type = typename traits_t::allocator_type;
  using pointer = typename traits_t::pointer;

  static_assert(std::is_nothrow_move_constructible_v<allocator_type>);
  static_assert(std::is_nothrow_move_assignable_v<allocator_type>);

  allocator_array_deallocator(const allocator_type& alloc, size_t size) noexcept
    : alloc_{alloc}, size_{size} {}
  allocator_array_deallocator(allocator_type&& alloc, size_t size) noexcept
    : alloc_{std::move(alloc)}, size_{size} {}

  void operator()(pointer p) noexcept {
    traits_t::deallocate(alloc_, p, size_);
  }

  allocator_array_deallocator(allocator_array_deallocator&& other) noexcept
    : alloc_{std::move(other.alloc_)}, size_{std::exchange(other.size_, 0)} {}

  allocator_array_deallocator& operator=(
    allocator_array_deallocator&& other) noexcept {
    if (this != &other) {
      alloc_ = std::move(other.alloc_);
      size_ = std::exchange(other.size_, 0);
    }
    return *this;
  }

  allocator_type& alloc() noexcept { return alloc_; }
  size_t size() const noexcept { return size_; }

 private:
  IRS_NO_UNIQUE_ADDRESS allocator_type alloc_;
  size_t size_;
};

template<typename Alloc>
class allocator_array_deleter {
  using traits_t = std::allocator_traits<Alloc>;

 public:
  using allocator_type = typename traits_t::allocator_type;
  using pointer = typename traits_t::pointer;

  allocator_array_deleter(const allocator_type& alloc, size_t size)
    : alloc_{alloc}, size_{size} {}

  void operator()(pointer p) noexcept {
    for (auto begin = p, end = p + size_; begin != end; ++begin) {
      traits_t::destroy(alloc_, begin);
    }
    traits_t::deallocate(alloc_, p, size_);
  }

  allocator_type& alloc() noexcept { return alloc_; }
  size_t size() const noexcept { return size_; }

 private:
  IRS_NO_UNIQUE_ADDRESS allocator_type alloc_;
  size_t size_;
};

struct Managed {
 protected:
  virtual ~Managed() = default;

 private:
  friend struct ManagedDeleter;

  // Const because we can allocate and then delete const object
  virtual void Destroy() const noexcept {}
};

template<typename Base>
struct OnHeap final : Base {
  static_assert(std::is_base_of_v<Managed, Base>);

  template<typename... Args>
  OnHeap(Args&&... args) : Base{std::forward<Args>(args)...} {}

 private:
  void Destroy() const noexcept final { delete this; }
};

template<typename Base>
struct Tracked final : Base {
  static_assert(std::is_base_of_v<Managed, Base>);

  template<typename... Args>
  Tracked(IResourceManager& rm, Args&&... args)
    : Base{std::forward<Args>(args)...}, rm_{rm} {
    rm.Increase(sizeof(*this));
  }

 private:
  void Destroy() const noexcept final {
    auto& rm = rm_;
    delete this;
    rm.Decrease(sizeof(*this));
  }

  IResourceManager& rm_;
};

struct ManagedDeleter {
  void operator()(const Managed* p) noexcept {
    IRS_ASSERT(p != nullptr);  // std::unique_ptr doesn't call dtor on nullptr
    p->Destroy();
  }
};

template<typename T>
class managed_ptr final : std::unique_ptr<T, ManagedDeleter> {
 private:
  using Ptr = std::unique_ptr<T, ManagedDeleter>;

  template<typename U>
  friend class managed_ptr;

  template<typename Base, typename Derived>
  friend constexpr managed_ptr<Base> to_managed(Derived& p) noexcept;

  template<typename Base, typename Derived, typename... Args>
  friend managed_ptr<Base> make_managed(Args&&... args);
  template<typename Base, typename Derived, typename... Args>
  friend managed_ptr<Base> make_tracked(IResourceManager&, Args&&... args);

  constexpr explicit managed_ptr(T* p) noexcept : Ptr{p} {}

 public:
  using typename Ptr::element_type;
  using typename Ptr::pointer;

  static_assert(!std::is_array_v<T>);

  constexpr managed_ptr() noexcept = default;
  constexpr managed_ptr(managed_ptr&& u) noexcept = default;
  constexpr managed_ptr(std::nullptr_t) noexcept : Ptr{nullptr} {}
  template<typename U,
           typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  constexpr managed_ptr(managed_ptr<U>&& u) noexcept : Ptr{std::move(u)} {}

  constexpr managed_ptr& operator=(managed_ptr&& t) noexcept = default;
  constexpr managed_ptr& operator=(std::nullptr_t) noexcept {
    Ptr::operator=(nullptr);
    return *this;
  }
  template<typename U,
           typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  constexpr managed_ptr& operator=(managed_ptr<U>&& u) noexcept {
    Ptr::operator=(std::move(u));
    return *this;
  }

  constexpr void reset(std::nullptr_t = nullptr) noexcept { Ptr::reset(); }
  constexpr void swap(managed_ptr& t) noexcept { Ptr::swap(t); }

  using Ptr::get;
  using Ptr::operator bool;
  using Ptr::operator*;
  using Ptr::operator->;
};

template<typename T, typename U>
constexpr bool operator==(const managed_ptr<T>& t, const managed_ptr<U>& u) {
  return t.get() == u.get();
}

template<typename T>
constexpr bool operator==(const managed_ptr<T>& t, std::nullptr_t) noexcept {
  return !t;
}

template<typename Base, typename Derived = Base>
constexpr managed_ptr<Base> to_managed(Derived& p) noexcept {
  return managed_ptr<Base>{&p};
}

template<typename Base, typename Derived = Base, typename... Args>
managed_ptr<Base> make_managed(Args&&... args) {
  return managed_ptr<Base>{new OnHeap<Derived>{std::forward<Args>(args)...}};
}

template<typename Base, typename Derived = Base, typename... Args>
managed_ptr<Base> make_tracked(IResourceManager& rm, Args&&... args) {
  return managed_ptr<Base>{
    new Tracked<Derived>{rm, std::forward<Args>(args)...}};
}

template<typename T, typename Alloc, typename... Types>
inline std::enable_if_t<
  !std::is_array_v<T>,
  std::unique_ptr<T, allocator_deleter<std::remove_cv_t<Alloc>>>>
allocate_unique(Alloc& alloc, Types&&... Args) {
  using traits_t = std::allocator_traits<std::remove_cv_t<Alloc>>;
  using pointer = typename traits_t::pointer;
  using allocator_type = typename traits_t::allocator_type;
  using deleter_t = allocator_deleter<allocator_type>;

  // allocate space for 1 object
  pointer p = alloc.allocate(1);

  try {
    // construct object
    traits_t::construct(alloc, p, std::forward<Types>(Args)...);
  } catch (...) {
    // free allocated storage in case of any error during construction
    alloc.deallocate(p, 1);
    throw;
  }

  return std::unique_ptr<T, deleter_t>{p, deleter_t{alloc}};
}

template<
  typename T, typename Alloc,
  typename = std::enable_if_t<std::is_array_v<T> && std::extent_v<T> == 0>>
auto allocate_unique(Alloc& alloc, size_t size) {
  using traits_t = std::allocator_traits<std::remove_cv_t<Alloc>>;
  using pointer = typename traits_t::pointer;
  using allocator_type = typename traits_t::allocator_type;
  using deleter_t = allocator_array_deleter<allocator_type>;
  using unique_ptr_t = std::unique_ptr<T, deleter_t>;

  pointer p = nullptr;

  if (!size) {
    return unique_ptr_t{p, deleter_t{alloc, size}};
  }

  p = alloc.allocate(size);  // allocate space for 'size' object

  auto begin = p;

  try {
    for (auto end = begin + size; begin != end; ++begin) {
      traits_t::construct(alloc, begin);  // construct object
    }
  } catch (...) {
    // destroy constructed objects
    for (; p != begin; --begin) {
      traits_t::destroy(alloc, begin);
    }

    // free allocated storage in case of any error during construction
    alloc.deallocate(p, size);
    throw;
  }

  return unique_ptr_t{p, deleter_t{alloc, size}};
}

// do not construct objects in a block
struct allocate_only_tag final {};
inline constexpr allocate_only_tag allocate_only{};

template<
  typename T, typename Alloc,
  typename = std::enable_if_t<std::is_array_v<T> && std::extent_v<T> == 0>>
auto allocate_unique(Alloc&& alloc, size_t size, allocate_only_tag /*tag*/) {
  using traits_t = std::allocator_traits<std::remove_cvref_t<Alloc>>;
  using pointer = typename traits_t::pointer;
  using allocator_type = typename traits_t::allocator_type;
  using deleter_t = allocator_array_deallocator<allocator_type>;
  using unique_ptr_t = std::unique_ptr<T, deleter_t>;

  pointer p = nullptr;

  if (!size) {
    return unique_ptr_t{p, deleter_t{std::forward<Alloc>(alloc), size}};
  }

  p = alloc.allocate(size);  // allocate space for 'size' object

  return unique_ptr_t{p, deleter_t{std::forward<Alloc>(alloc), size}};
}

// Decline wrong syntax
template<typename T, typename Alloc, typename... Types>
std::enable_if_t<std::extent_v<T> != 0, void> allocate_unique(
  Alloc&, Types&&...) = delete;

template<typename Class, bool = is_shared_ptr_v<typename Class::ptr>>
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
    static_assert(
      std::is_nothrow_constructible_v<typename Class::ptr,
                                      typename Class::ptr::element_type*>);

    return typename Class::ptr(new Class(std::forward<Args>(args)...));
  }
};

}  // namespace irs::memory

// Default inline implementation of a factory method, instantiation on heap
#define DEFINE_FACTORY_INLINE(class_name)                                     \
  template<typename Type, typename... Args>                                   \
  static ptr make(Args&&... args) {                                           \
    using type = std::enable_if_t<std::is_base_of_v<class_name, Type>, Type>; \
    using maker_t = irs::memory::maker<type>;                                 \
    return maker_t::make(std::forward<Args>(args)...);        \
  }                                                                           \
  template<typename Class, bool>                                              \
  friend struct irs::memory::maker
