//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/value_stack.hpp>

#include <boost/json/serialize.hpp>
#include <boost/json/static_resource.hpp>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

BOOST_STATIC_ASSERT( std::is_nothrow_destructible<value_stack>::value );

class value_stack_test
{
public:
    // This is from the javadoc
    void
    testValueStack()
    {

    //----------------------------------
    // value_stack
    {
    // This example builds a json::value without any dynamic memory allocations:

    // Construct the value stack using a local buffer
    unsigned char temp[4096];
    value_stack st( storage_ptr(), temp, sizeof(temp) );

    // Create a static resource with a local initial buffer
    unsigned char buf[4096];
    static_resource mr( buf, sizeof(buf) );

    // All values on the stack will use `mr`
    st.reset(&mr);

    // Push the key/value pair "a":1.
    st.push_key("a");
    st.push_int64(1);

    // Push "b":null
    st.push_key("b");
    st.push_null();

    // Push "c":"hello"
    st.push_key("c");
    st.push_string("hello");

    // Pop the three key/value pairs and push an object with those three values.
    st.push_object(3);

    // Pop the object from the stack and take ownership.
    value jv = st.release();

    assert( serialize(jv) == "{\"a\":1,\"b\":null,\"c\":\"hello\"}" );

    // At this point we could re-use the stack by calling reset
    }

    //----------------------------------
    // value_stack::push_array
    {
        value_stack st;

        // reset must be called first or else the behavior is undefined
        st.reset();

        // Place three values on the stack
        st.push_int64( 1 );
        st.push_int64( 2 );
        st.push_int64( 3 );

        // Remove the 3 values, and push an array with those 3 elements on the stack
        st.push_array( 3 );

        // Pop the object from the stack and take ownership.
        value jv = st.release();

        assert( serialize(jv) == "[1,2,3]" );

        // At this point, reset must be called again to use the stack
    }

    //----------------------------------
    // value_stack::push_object
    {
        value_stack st;

        // reset must be called first or else the behavior is undefined
        st.reset();

        // Place a key/value pair onto the stack
        st.push_key( "x" );
        st.push_bool( true );

        // Replace the key/value pair with an object containing a single element
        st.push_object( 1 );

        // Pop the object from the stack and take ownership.
        value jv = st.release();

        assert( serialize(jv) == "{\"x\":true}" );

        // At this point, reset must be called again to use the stack
    }

    }

    void
    run()
    {
        testValueStack();
    }
};

TEST_SUITE(value_stack_test, "boost.json.value_stack");

BOOST_JSON_NS_END
