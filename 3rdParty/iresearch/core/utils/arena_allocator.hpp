////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ARENA_ALLOCATOR_H
#define IRESEARCH_ARENA_ALLOCATOR_H

#include <vector>

#include "shared.hpp"
#include "memory.hpp"
#include "noncopyable.hpp"

namespace iresearch {
namespace memory {

///////////////////////////////////////////////////////////////////////////////
/// @class arena_allocator
///////////////////////////////////////////////////////////////////////////////
template<size_t Size, size_t Alignment = alignof(std::max_align_t)>
struct memory_arena
    : private aligned_storage<Size, Alignment>,
      private util::noncopyable {
 public:
  static const size_t SIZE = Size;
  static const size_t ALIGNMENT = Alignment;

  memory_arena() = default;
  ~memory_arena() noexcept { p_ = nullptr; }

  char* allocate(size_t size) {
    assert(within_arena(p_));
    auto* p = p_ + align_up(size, Alignment);

    if (within_arena(p)) {
      std::swap(p, p_);
      return p;
    }

#if (defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606)
//FIXME
//    if (Alignment > alignof(MAX_ALIGN_T)) {
//      return reinterpret_cast<char*>(::operator new(size, Alignment));
//    }
#else
    static_assert(
      Alignment <= alignof(MAX_ALIGN_T),
      "new can't guarantee the requested alignment"
    );
#endif

    return reinterpret_cast<char*>(::operator new(size));
  }

  void deallocate(char* p, size_t size) noexcept {
    assert(within_arena(p_));

    if (within_arena(p)) {
      size = align_up(size, Alignment);

      if (p + size == p_) {
        p_ = p;
      }
    } else {
#if (defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606)
//FIXME
//      if (Alignment > alignof(MAX_ALIGN_T)) {
//        ::operator delete(p, Alignment);
//      }
#else
      static_assert(
        Alignment <= alignof(MAX_ALIGN_T),
        "new can't guarantee the requested alignment"
      );
#endif
      ::operator delete(p);
    }
  }

  size_t used() const noexcept {
    return p_ - buffer_t::data;
  }

  bool within_arena(const char* p) const noexcept {
    return std::begin(buffer_t::data) <= p && p <= std::end(buffer_t::data);
  }

 private:
  typedef aligned_storage<Size, Alignment> buffer_t;

  char* p_{ buffer_t::data }; // current position
}; // memory_arena

///////////////////////////////////////////////////////////////////////////////
/// @class arena_allocator_wrapper
///////////////////////////////////////////////////////////////////////////////
template<typename T, typename Arena>
class arena_allocator {
 public:
  typedef T value_type;
  typedef Arena arena_t;

  // cppcheck-suppress constParameter
  // cppcheck-suppress noExplicitConstructor
  arena_allocator(arena_t& arena) noexcept
    : arena_(&arena) {
    static_assert(arena_t::SIZE % arena_t::ALIGNMENT == 0,
                  "size needs to be a multiple of alignment");
  }

  template <class U>
  arena_allocator(const arena_allocator<U, Arena>& rhs) noexcept
    : arena_(rhs.arena_) {
  }

  arena_allocator(const arena_allocator&) = default;

  template <class U>
  struct rebind {
    using other = arena_allocator<U, Arena>;
  };

  T* allocate(size_t n) const {
    return reinterpret_cast<T*>(arena_->allocate(n * sizeof(T)));
  }

  void deallocate(T* p, size_t n) const noexcept {
    arena_->deallocate(reinterpret_cast<char*>(p), n * sizeof(T));
  }

  template<typename T1, typename U, typename A1, typename A2>
  friend bool operator==(const arena_allocator<T1, A1>& lhs,
                         const arena_allocator<U, A2>& rhs) noexcept;

  template <typename U, typename A>
  friend class arena_allocator;

 private:
  arena_allocator& operator=(const arena_allocator&) = delete;

  arena_t* arena_;
}; // arena_allocator

template <class T, typename A1, class U, typename A2>
inline bool operator!=(const arena_allocator<T, A1>& lhs,
                       const arena_allocator<U, A2>& rhs) noexcept {
  return !(lhs == rhs);
}

template<typename T1, typename U, typename A1, typename A2>
inline bool operator==(const arena_allocator<T1, A1>& lhs,
  const arena_allocator<U, A2>& rhs) noexcept {
  return std::is_same<A1, A2>::value && 
         lhs.arena_ == reinterpret_cast<const void*>(rhs.arena_);
}

template<typename T, size_t N>
using arena = memory_arena<N*sizeof(T), alignof(T)>;

template<typename T, typename Arena>
using arena_vector = std::vector<T, arena_allocator<T, Arena>>;

} // memory
} // ROOT

#endif // IRESEARCH_ARENA_ALLOCATOR_H
