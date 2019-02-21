//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_DETAIL_MARKED_PTR_HPP
#define XENIUM_DETAIL_MARKED_PTR_HPP

#include <cassert>
#include <cstdint>
#include <cstddef>

namespace xenium { namespace reclamation { namespace detail {
  
  template <class T, std::size_t N>
  class marked_ptr {
  public:
    // Construct a marked ptr
    marked_ptr(T* p = nullptr, uintptr_t mark = 0) noexcept
    {
      assert(mark <= MarkMask && "mark exceeds the number of bits reserved");
      assert((reinterpret_cast<uintptr_t>(p) & MarkMask) == 0 &&
        "bits reserved for masking are occupied by the pointer");
      ptr = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(p) | mark);
    }
    
    // Set to nullptr
    void reset() noexcept { ptr = nullptr; }
    
    // Get mark bits
    uintptr_t mark() const noexcept {
      return reinterpret_cast<uintptr_t>(ptr) & MarkMask;
    }
    
    // Get underlying pointer (with mark bits stripped off).
    T* get() const noexcept {
      return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) & ~MarkMask);
    }
    
    // True if get() != nullptr || mark() != 0
    explicit operator bool() const noexcept { return ptr != nullptr; }
    
    // Get pointer with mark bits stripped off.
    T* operator->() const noexcept { return get(); }
    
    // Get reference to target of pointer.
    T& operator*() const noexcept { return *get(); }

    inline friend bool operator==(const marked_ptr& l, const marked_ptr& r) { return l.ptr == r.ptr; }
    inline friend bool operator!=(const marked_ptr& l, const marked_ptr& r) { return l.ptr != r.ptr; }

    static constexpr std::size_t number_of_mark_bits = N;
  private:
    static constexpr uintptr_t MarkMask = (1 << N) - 1;
    T* ptr;

#ifdef _MSC_VER
    // These members are only for the VS debugger visualizer (natvis).
    enum Masking { MarkMask_ = MarkMask };
    using PtrType = T*;
#endif
  };
}}}

#endif