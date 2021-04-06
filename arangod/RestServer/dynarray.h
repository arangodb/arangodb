// Copyright 2009 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iterator>
#include <stdexcept>
#include <limits>

namespace std {

struct bad_array_length_ { };

template< class T >
struct dynarray
{
    // types:
    typedef       T                               value_type;
    typedef       T&                              reference;
    typedef const T&                              const_reference;
    typedef       T*                              iterator;
    typedef const T*                              const_iterator;
    typedef std::reverse_iterator<iterator>       reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef size_t                                size_type;
    typedef ptrdiff_t                             difference_type;

    // fields:
private:
    T*        store;
    size_type count;

    // helper functions:
    void check(size_type n)
        { if ( n >= count ) throw out_of_range("dynarray"); }
    T* alloc(size_type n)
        { if ( n > std::numeric_limits<size_type>::max()/sizeof(T) )
              throw std::bad_array_length_();
          return reinterpret_cast<T*>( new char[ n*sizeof(T) ] ); }

public:
    // construct and destruct:
    dynarray() = delete;
    const dynarray operator=(const dynarray&) = delete;

    explicit dynarray(size_type c)
        : store( alloc( c ) ), count( c )
        { size_type i = 0;
          try {
              for ( i = 0; i < count; ++i )
                  new (store+i) T;
          } catch ( ... ) {
              for ( ; i > 0; --i )
                 (store+(i-1))->~T();
              throw;
          } }

    dynarray(const dynarray& d)
        : store( alloc( d.count ) ), count( d.count )
        { try { uninitialized_copy( d.begin(), d.end(), begin() ); }
          catch ( ... ) { delete store; throw; } }

    ~dynarray()
        { for ( size_type i = 0; i < count; ++i )
              (store+i)->~T();
          delete[] store; }

    // iterators:
    iterator       begin()        { return store; }
    const_iterator begin()  const { return store; }
    const_iterator cbegin() const { return store; }
    iterator       end()          { return store + count; }
    const_iterator end()    const { return store + count; }
    const_iterator cend()   const { return store + count; }

    reverse_iterator       rbegin()       
        { return reverse_iterator(end()); }
    const_reverse_iterator rbegin()  const
        { return reverse_iterator(end()); }
    reverse_iterator       rend()         
        { return reverse_iterator(begin()); }
    const_reverse_iterator rend()    const
        { return reverse_iterator(begin()); }

    // capacity:
    size_type size()     const { return count; }
    size_type max_size() const { return count; }
    bool      empty()    const { return count == 0; }

    // element access:
    reference       operator[](size_type n)       { return store[n]; }
    const_reference operator[](size_type n) const { return store[n]; }

    reference       front()       { return store[0]; }
    const_reference front() const { return store[0]; }
    reference       back()        { return store[count-1]; }
    const_reference back()  const { return store[count-1]; }

    const_reference at(size_type n) const { check(n); return store[n]; }
    reference       at(size_type n)       { check(n); return store[n]; }

    // data access:
    T*       data()       { return store; }
    const T* data() const { return store; }
};

} // namespace std
