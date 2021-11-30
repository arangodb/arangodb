//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json.hpp>
#ifndef BOOST_JSON_STANDALONE
#include <boost/container/pmr/vector.hpp>
#endif
#include <iostream>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

static void set1() {

//----------------------------------------------------------
{
//[doc_quick_look_1
object obj;                                                     // construct an empty object
obj[ "pi" ] = 3.141;                                            // insert a double
obj[ "happy" ] = true;                                          // insert a bool
obj[ "name" ] = "Boost";                                        // insert a string
obj[ "nothing" ] = nullptr;                                     // insert a null
obj[ "answer" ].emplace_object()["everything"] = 42;            // insert an object with 1 element
obj[ "list" ] = { 1, 0, 2 };                                    // insert an array with 3 elements
obj[ "object" ] = { {"currency", "USD"}, {"value", 42.99} };    // insert an object with 2 elements
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_2
value jv = {
    { "pi", 3.141 },
    { "happy", true },
    { "name", "Boost" },
    { "nothing", nullptr },
    { "answer", {
        { "everything", 42 } } },
    {"list", {1, 0, 2}},
    {"object", {
        { "currency", "USD" },
        { "value", 42.99 }
            } }
    };
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_3
array arr;                                          // construct an empty array
arr = { 1, 2, 3 };                                  // replace the contents with 3 elements
value jv1( arr );                                   // this makes a copy of the array
value jv2( std::move(arr) );                        // this performs a move-construction

assert( arr.empty() );                              // moved-from arrays become empty
arr = { nullptr, true, "boost" };                   // fill in the array again
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_4
{
    unsigned char buf[ 4096 ];                      // storage for our array
    static_resource mr( buf );                      // memory resource which uses buf
    array arr( &mr );                               // construct using the memory resource
    arr = { 1, 2, 3 };                              // all allocated memory comes from `buf`
}
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_5
{
    monotonic_resource mr;                          // memory resource optimized for insertion
    array arr( &mr );                               // construct using the memory resource
    arr.resize( 1 );                                // make space for one element
    arr[ 0 ] = { 1, 2, 3 };                         // assign an array to element 0
    assert( *arr[0].storage() == *arr.storage() );  // same memory resource
}
//]
}
//----------------------------------------------------------
{
#ifndef BOOST_JSON_STANDALONE
//[doc_quick_look_6
{
    monotonic_resource mr;
    boost::container::pmr::vector< value > vv( &mr );
    vv.resize( 3 );

    // The memory resource of the container is propagated to each element
    assert( *vv.get_allocator().resource() == *vv[0].storage() );
    assert( *vv.get_allocator().resource() == *vv[1].storage() );
    assert( *vv.get_allocator().resource() == *vv[2].storage() );
}
//]
#endif
}
//----------------------------------------------------------

} // set1()

//----------------------------------------------------------

//[doc_quick_look_7
value f()
{
    // create a reference-counted memory resource
    storage_ptr sp = make_shared_resource< monotonic_resource >();

    // construct with shared ownership of the resource
    value jv( sp );

    // assign an array with 3 elements, the monotonic resource will be used
    jv = { 1, 2, 3 };

    // The caller receives the value, which still owns the resource
    return jv;
}
//]
//----------------------------------------------------------

static void set2() {

//----------------------------------------------------------
{
//[doc_quick_look_8
value jv = parse( "[1, 2, 3]" );
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_9
error_code ec;
value jv = parse( R"( "Hello, world!" )", ec );
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_10
unsigned char buf[ 4096 ];
static_resource mr( buf );
parse_options opt;
opt.allow_comments = true;
opt.allow_trailing_commas = true;
value jv = parse( "[1, 2, 3, ] // array ", &mr, opt );
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_11
stream_parser p;
error_code ec;
p.reset();
p.write( "[1, 2 ", ec );
if( ! ec )
    p.write( ", 3]", ec );
if( ! ec )
    p.finish( ec );
if( ec )
    return;
value jv = p.release();
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_12
value jv = { 1, 2, 3 };
std::string s = serialize( jv );                // produces "[1,2,3]"
//]
}
//----------------------------------------------------------
{
value jv;
//[doc_quick_look_13
serializer sr;
sr.reset( &jv );                                // prepare to output `jv`
do
{
    char buf[ 16 ];
    std::cout << sr.read( buf );
}
while( ! sr.done() );
//]
}
//----------------------------------------------------------

} // set2()

//----------------------------------------------------------

//[doc_quick_look_14
namespace my_app {

struct customer
{
    int id;
    std::string name;
    bool current;
};

} // namespace my_app
//]

//----------------------------------------------------------

//[doc_quick_look_15
namespace my_app {

void tag_invoke( value_from_tag, value& jv, customer const& c )
{
    jv = {
        { "id" , c.id },
        { "name", c.name },
        { "current", c.current }
    };
}

} // namespace my_app
//]

static void set3() {

//----------------------------------------------------------
{
//[doc_quick_look_16
my_app::customer c{ 1001, "Boost", true };
std::cout << serialize( value_from( c ) );
//]
}
//----------------------------------------------------------
{
//[doc_quick_look_17
std::vector< my_app::customer > vc;
//...
value jv = value_from( vc );
//]
}
//----------------------------------------------------------

} // set3()

//----------------------------------------------------------

//[doc_quick_look_18
namespace my_app {

// This helper function deduces the type and assigns the value with the matching key
template<class T>
void extract( object const& obj, T& t, string_view key )
{
    t = value_to<T>( obj.at( key ) );
}

customer tag_invoke( value_to_tag< customer >, value const& jv )
{
    customer c;
    object const& obj = jv.as_object();
    extract( obj, c.id, "id" );
    extract( obj, c.name, "name" );
    extract( obj, c.current, "current" );
    return c;
}

} // namespace my_app
//]

//----------------------------------------------------------

namespace my_app_2 {
namespace my_app { using boost::json::my_app::customer; }
//[doc_quick_look_19
namespace my_app {

customer tag_invoke( value_to_tag< customer >, value const& jv )
{
    object const& obj = jv.as_object();
    return customer {
        value_to<int>( obj.at( "id" ) ),
        value_to<std::string>( obj.at( "name" ) ),
        value_to<bool>( obj.at( "current" ) )
    };
}

} // namespace my_app
//]
} // my_app_2

//----------------------------------------------------------

static void set4() {
using namespace my_app;

//----------------------------------------------------------
{
//[doc_quick_look_20
json::value jv;
//...
customer c( value_to<customer>(jv) );
//]
}
//----------------------------------------------------------
{
json::value jv;
//[doc_quick_look_21
std::vector< customer > vc = value_to< std::vector< customer > >( jv );
//]
}
//----------------------------------------------------------

//----------------------------------------------------------

} // set4

class doc_quick_look_test
{
public:
    void
    run()
    {
        (void)&set1;
        (void)&set2;
        (void)&set3;
        (void)&set4;
        BOOST_TEST_PASS();
    }
};

TEST_SUITE(doc_quick_look_test, "boost.json.doc_quick_look");

BOOST_JSON_NS_END
