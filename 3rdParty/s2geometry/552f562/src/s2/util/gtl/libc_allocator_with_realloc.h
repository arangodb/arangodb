// Copyright 2010 Google Inc. All Rights Reserved.
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

// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
//

#ifndef S2_UTIL_GTL_LIBC_ALLOCATOR_WITH_REALLOC_H_
#define S2_UTIL_GTL_LIBC_ALLOCATOR_WITH_REALLOC_H_

#include <cstdlib>           // for malloc/realloc/free
#include <cstddef>           // for ptrdiff_t
#include <new>                // for placement new

namespace gtl {

template<class T>
class libc_allocator_with_realloc {
 public:
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;

  template<class U>
  struct rebind {
    typedef libc_allocator_with_realloc<U> other;
  };

  libc_allocator_with_realloc() = default;

  template <class U>
  libc_allocator_with_realloc(const libc_allocator_with_realloc<U>&) {}

  pointer address(reference r) const  { return &r; }
  const_pointer address(const_reference r) const  { return &r; }

  pointer allocate(size_type n, const_pointer = nullptr) {
    pointer p = static_cast<pointer>(malloc(n * sizeof(value_type)));
    if (p == nullptr && n != 0) throw std::bad_alloc();
    return p;
  }
  void deallocate(pointer p, size_type) {
    free(p);
  }
  pointer reallocate(pointer p, size_type n) {
    p = static_cast<pointer>(realloc(p, n * sizeof(value_type)));
    if (p == nullptr && n != 0) throw std::bad_alloc();
    return p;
  }

  size_type max_size() const  {
    return static_cast<size_type>(-1) / sizeof(value_type);
  }

  void construct(pointer p, const value_type& val) {
    new(p) value_type(val);
  }
  void destroy(pointer p) { p->~value_type(); }
};

// libc_allocator_with_realloc<void> specialization.
template<>
class libc_allocator_with_realloc<void> {
 public:
  typedef void value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef void* pointer;
  typedef const void* const_pointer;

  template<class U>
  struct rebind {
    typedef libc_allocator_with_realloc<U> other;
  };

  libc_allocator_with_realloc() = default;

  template <class U>
  libc_allocator_with_realloc(const libc_allocator_with_realloc<U>&) {}
};

template<class T>
inline bool operator==(const libc_allocator_with_realloc<T>&,
                       const libc_allocator_with_realloc<T>&) {
  return true;
}

template<class T>
inline bool operator!=(const libc_allocator_with_realloc<T>&,
                       const libc_allocator_with_realloc<T>&) {
  return false;
}

}

#endif  // S2_UTIL_GTL_LIBC_ALLOCATOR_WITH_REALLOC_H_
