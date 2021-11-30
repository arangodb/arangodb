//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json/memory_resource.hpp>
#include <boost/json/monotonic_resource.hpp>
#include <boost/json/value.hpp>
#include <vector>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

//----------------------------------------------------------

static void set1() {

//----------------------------------------------------------
{
//[doc_uses_allocator_1
// We want to use this resource for all the containers
monotonic_resource mr;

// Declare a vector of JSON values
std::vector< value, polymorphic_allocator< value > > v( &mr );

// The polymorphic allocator will use our resource
assert( v.get_allocator().resource() == &mr );

// Add a string to the vector
v.emplace_back( "boost" );

// The vector propagates the memory resource to the string
assert( v[0].storage().get() == &mr );
//]
}
//----------------------------------------------------------
{
//[doc_uses_allocator_2
// This vector will use the default memory resource
std::vector< value, polymorphic_allocator < value > > v;

// This value will same memory resource as the vector
value jv( v.get_allocator() );

// However, ownership is not transferred,
assert( ! jv.storage().is_shared() );

// and deallocate is never null
assert( ! jv.storage().is_deallocate_trivial() );
//]
}
//----------------------------------------------------------

} // set1

//----------------------------------------------------------

class doc_uses_allocator_test
{
public:
    void
    run()
    {
        (void)&set1;
        BOOST_TEST_PASS();
    }
};

TEST_SUITE(doc_uses_allocator_test, "boost.json.doc_uses_allocator");

BOOST_JSON_NS_END
