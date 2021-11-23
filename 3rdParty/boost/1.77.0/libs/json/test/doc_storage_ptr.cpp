//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json/monotonic_resource.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/static_resource.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/json/value.hpp>

#include <iostream>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

//----------------------------------------------------------

static void set1() {

//----------------------------------------------------------
{
//[doc_storage_ptr_1
storage_ptr sp1;
storage_ptr sp2;

assert( sp1.get() != nullptr );                         // always points to a valid resource
assert( sp1.get() == sp2.get() );                       // both point to the default resource
assert( *sp1.get() == *sp2.get() );                     // the default resource compares equal
//]
}
//----------------------------------------------------------
{
//[doc_storage_ptr_2
array arr;                                              // default construction
object obj;
string str;
value jv;

assert( jv.storage().get() == storage_ptr().get() );    // uses the default memory resource
assert( jv.storage().get() == arr.storage().get() );    // both point to the default resource
assert( *arr.storage() == *obj.storage() );             // containers use equivalent resources
//]
}
//----------------------------------------------------------
{
//[doc_storage_ptr_3
monotonic_resource mr;

value const jv = parse( "[1,2,3]", &mr );
//]
}
//----------------------------------------------------------

} // set1

//----------------------------------------------------------

//[doc_storage_ptr_4
value parse_value( string_view s)
{
    return parse( s, make_shared_resource< monotonic_resource >() );
}
//]

//----------------------------------------------------------

//[doc_storage_ptr_5
template< class Handler >
void do_rpc( string_view s, Handler&& h )
{
    unsigned char buffer[ 8192 ];                       // Small stack buffer to avoid most allocations during parse
    monotonic_resource mr( buffer );                    // This resource will use our local buffer first
    value const jv = parse( s, &mr );                   // Parse the input string into a value that uses our resource
    h( jv );                                            // Call the handler to perform the RPC command
}
//]

//----------------------------------------------------------

void set2() {

//----------------------------------------------------------
{
//[doc_storage_ptr_6
unsigned char buffer[ 8192 ];
static_resource mr( buffer );                           // The resource will use our local buffer
//]
}
//----------------------------------------------------------
{
//[doc_storage_ptr_7
monotonic_resource mr;
array arr( &mr );                                       // construct an array using our resource
arr.emplace_back( "boost" );                            // insert a string
assert( *arr[0].as_string().storage() == mr );          // the resource is propagated to the string
//]
}
//----------------------------------------------------------
{
//[doc_storage_ptr_8
{
    monotonic_resource mr;

    array arr( &mr );                                   // construct an array using our resource

    assert( ! arr.storage().is_shared() );              // no shared ownership
}
//]
}
//----------------------------------------------------------
{
//[doc_storage_ptr_9
storage_ptr sp = make_shared_resource< monotonic_resource >();

string str( sp );

assert( sp.is_shared() );                               // shared ownership
assert( str.storage().is_shared() );                    // shared ownership
//]
}
//----------------------------------------------------------

} // set2

//----------------------------------------------------------
//[doc_storage_ptr_10
class logging_resource : public memory_resource
{
private:
    void* do_allocate( std::size_t bytes, std::size_t align ) override
    {
        std::cout << "Allocating " << bytes << " bytes with alignment " << align << '\n';

        return ::operator new( bytes );
    }

    void do_deallocate( void* ptr, std::size_t bytes, std::size_t align ) override
    {
        std::cout << "Deallocating " << bytes << " bytes with alignment " << align << " @ address " << ptr << '\n';

        return ::operator delete( ptr );
    }

    bool do_is_equal( memory_resource const& other ) const noexcept override
    {
        // since the global allocation and deallocation functions are used,
        // any instance of a logging_resource can deallocate memory allocated
        // by another instance of a logging_resource

        return dynamic_cast< logging_resource const* >( &other ) != nullptr;
    }
};
//]

//----------------------------------------------------------

class doc_storage_ptr_test
{
public:
    void
    run()
    {
        (void)&set1;
        BOOST_TEST_PASS();
    }
};

TEST_SUITE(doc_storage_ptr_test, "boost.json.doc_storage_ptr");

BOOST_JSON_NS_END
