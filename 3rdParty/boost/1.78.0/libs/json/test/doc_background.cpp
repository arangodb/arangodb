//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json/monotonic_resource.hpp>
#include <boost/json/static_resource.hpp>
#include <boost/json/value.hpp>

#ifndef BOOST_JSON_STANDALONE
#include <boost/container/pmr/vector.hpp>
#endif
#include <vector>

#include "test_suite.hpp"

#ifdef assert
#undef assert
#endif
#define assert BOOST_TEST

BOOST_JSON_NS_BEGIN

namespace doc_background {

namespace background {
#ifdef BOOST_JSON_STANDALONE
template<class T>
using vector = std::pmr::vector<T>;
#else
template<class T>
using vector = boost::container::pmr::vector<T>;
#endif
} // background

//----------------------------------------------------------

namespace std {
template<class T>
using allocator = ::std::allocator<T>;
} // std

//[doc_background_1
namespace std {

template< class T, class Allocator = std::allocator< T > >
class vector;

} // namespace std
//]

//----------------------------------------------------------

//[doc_background_2
namespace std {

template< class T, class Allocator >
class vector
{
public:
    explicit vector( Allocator const& alloc );

    //...
//]
};

template<class T, class A>
vector<T,A>::vector(A const&){}
}

//----------------------------------------------------------

//[doc_background_3
namespace std {
namespace pmr {

class memory_resource
{
public:
    virtual ~memory_resource();

    void* allocate  ( size_t bytes, size_t alignment );
    void  deallocate( void* p, size_t bytes, size_t alignment );
    bool  is_equal  ( const memory_resource& other ) const;

protected:
    virtual void* do_allocate  ( size_t bytes, size_t alignment ) = 0;
    virtual void  do_deallocate( void* p, size_t bytes, size_t alignment ) = 0;
    virtual bool  do_is_equal  ( memory_resource const& other ) const noexcept = 0;
};

} // namespace pmr
} // namespace std
//]

//----------------------------------------------------------

//[doc_background_4

namespace std {
namespace pmr {

template< class T >
using vector = std::vector< T, polymorphic_allocator< T > >;

} // namespace pmr
} // namespace std

//]

//----------------------------------------------------------

static void set1() {
using namespace background;

//----------------------------------------------------------
{
struct T {};
//[doc_background_5
// A type of memory resource
monotonic_resource mr;

// Construct a vector using the monotonic buffer resource
vector< T > v1(( polymorphic_allocator< T >(&mr) ));

// Or this way, since construction from memory_resource* is implicit:
vector< T > v2( &mr );
//]
}
//----------------------------------------------------------
{
//[doc_background_6
{
    // A type of memory resource which uses a stack buffer
    unsigned char temp[4096];
    static_resource mr( temp, sizeof(temp) );

    // Construct a vector using the static buffer resource
    vector< value > v( &mr );

    // The vector will allocate from `temp` first, and then the heap.
}
//]
}
//----------------------------------------------------------

} // set1

//----------------------------------------------------------

struct my_resource : memory_resource
{
    void* do_allocate  ( size_t, size_t ) override { return 0; }
    void  do_deallocate( void*, size_t, size_t ) override {}
    bool  do_is_equal  ( memory_resource const& ) const noexcept override { return true; }
};
//[doc_background_7
namespace my_library {

std::pmr::vector<char> get_chars1()
{
    // This leaks memory because `v` does not own the memory resource
    std::pmr::vector<char> v( new my_resource );

    return v;
}

} // my_library
//]

//----------------------------------------------------------

//[doc_background_8
namespace my_library {

std::pmr::vector<char> get_chars2()
{
    // Declare a local memory resource
    my_resource mr;

    // Construct a vector that uses our resource
    std::pmr::vector<char> v( &mr );

    // Undefined behavior, `mr` goes out of scope!
    return v;
}

} // my_library
//]

//----------------------------------------------------------

} // doc_background

class doc_background_test
{
public:
    void
    run()
    {
        using namespace doc_background;
        (void)&set1;
        BOOST_TEST_PASS();
    }
};

TEST_SUITE(doc_background_test, "boost.json.doc_background");

BOOST_JSON_NS_END
