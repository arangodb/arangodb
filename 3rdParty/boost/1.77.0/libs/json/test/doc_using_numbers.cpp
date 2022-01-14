//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>

#include "test_suite.hpp"

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4101)
# pragma warning(disable: 4189) // local variable is initialized but not referenced
#elif defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused"
# pragma clang diagnostic ignored "-Wunused-variable"
#elif defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused"
# pragma GCC diagnostic ignored "-Wunused-variable"
#endif

BOOST_JSON_NS_BEGIN

//----------------------------------------------------------

static void set1() {

//----------------------------------------------------------
{
//[doc_using_numbers_1
// construction from int
value jv1 = 1;

assert( jv1.is_int64() );

// construction from unsigned int
value jv2 = 2u;

assert( jv2.is_uint64() );

// construction from double
value jv3 = 3.0;

assert( jv3.is_double() );
//]
}
//----------------------------------------------------------
{
//[doc_using_numbers_2

value jv = 1;

assert( jv.is_int64() );

// jv.kind() != kind::uint64; throws
std::uint64_t r1 = jv.as_uint64();

// jv.kind() != kind::uint64; the behavior is undefined
std::uint64_t r2 = jv.get_uint64();

// if_double will always return nullptr, branch is not taken
if(double* d = jv.if_double())
    assert( false );

//]
};
//----------------------------------------------------------
{
//[doc_using_numbers_3
struct convert_int64
{
    value jv;

    operator int() const
    {
        return value_to< int >( this->jv );
    }
};
//]
}
//----------------------------------------------------------
try
{
//[doc_using_numbers_4
    value jv1 = 404;

    assert( jv1.is_int64() );

    // ok, identity conversion
    std::int64_t r1 = value_to< std::int64_t >( jv1 );

    // loss of data, throws system_error
    char r2 = value_to< char >( jv1 );

    // ok, no loss of data
    double r3 = value_to< double >( jv1 );

    value jv2 = 1.23;

    assert( jv1.is_double() );

    // ok, same as static_cast<float>( jv2.get_double() )
    float r4 = value_to< float >( jv2 );

    // not exact, throws system_error
    int r5 = value_to< int >( jv2 );

    value jv3 = {1, 2, 3};

    assert( ! jv3.is_number() );

    // not a number, throws system_error
    int r6 = value_to< int >( jv3 );
//]
}
catch(...)
{
}
//----------------------------------------------------------
{
//[doc_using_numbers_5

value jv = 10.5;

error_code ec;

// ok, conversion is exact
float r1 = jv.to_number< float >( ec );

assert( ! ec );

// error, conversion is non-exact
int r2 = jv.to_number< int >( ec );

assert( ec == error::not_exact );

//]
}
//----------------------------------------------------------
{
//[doc_using_numbers_6
value jv = parse("[-42, 100, 10.25, -299999999999999999998, 2e32]");

array ja = jv.as_array();

// represented by std::int64_t
assert( ja[0].is_int64() );

// represented by std::int64_t
assert( ja[1].is_int64() );

// contains decimal point, represented as double
assert( ja[2].is_double() );

// less than INT64_MIN, represented as double
assert( ja[3].is_double() );

// contains exponent, represented as double
assert( ja[4].is_double() );
//]
}
//----------------------------------------------------------
{
//[doc_using_numbers_7
//]
}
//----------------------------------------------------------

} // set1

//----------------------------------------------------------

class doc_using_numbers_test
{
public:
    void
    run()
    {
        (void)&set1;
    }
};

TEST_SUITE(doc_using_numbers_test, "boost.json.doc_using_numbers");

BOOST_JSON_NS_END
