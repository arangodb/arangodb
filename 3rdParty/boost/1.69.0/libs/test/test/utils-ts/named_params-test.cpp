//  (C) Copyright Gennadiy Rozental 2001-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : unit test for named function parameters framework
// *****************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE Named function parameters test
#include <boost/test/unit_test.hpp>
#include <boost/test/utils/named_params.hpp>
namespace utf = boost::unit_test;
namespace nfp = boost::nfp;

namespace test_single_int_parameter {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<int, struct k2_t> k2;

template<typename Params>
void dotest0( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( p[k1] == 7 );
}

template<typename Params>
void dotest1( Params const& p )
{
    BOOST_TEST_REQUIRE( !p.has(k1) );
}

BOOST_AUTO_TEST_CASE( test_single_int_parameter )
{
    dotest0( k1=7 );
    dotest1( k2=7 );
}

} // test_single_int_parameter

//____________________________________________________________________________//

namespace test_single_string_parameter {

nfp::typed_keyword<std::string, struct k1_t> k1;
nfp::typed_keyword<char const*, struct k2_t> k2;

template<typename Params>
void dotest0( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( p[k1] == "abc" );
}

template<typename Params>
void dotest1( Params const& p )
{
    BOOST_TEST_REQUIRE( !p.has(k1) );
}

BOOST_AUTO_TEST_CASE( test_single_string_parameter )
{
    dotest0( k1="abc" );
    dotest1( k2="cba" );
}

} // test_single_string_parameter

//____________________________________________________________________________//

namespace test_single_bool_parameter {

nfp::typed_keyword<bool, struct k1_t> k1;
nfp::typed_keyword<bool, struct k2_t> k2;

template<typename Params>
void dotest0( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( p[k1] );
}

template<typename Params>
void dotest1( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( !p[k1] );
}

template<typename Params>
void dotest2( Params const& p )
{
    BOOST_TEST_REQUIRE( !p.has(k1) );
}

BOOST_AUTO_TEST_CASE( test_single_bool_parameter )
{
    dotest0( k1 );
    dotest1( !k1 );
    dotest2( k2 );
}

} // test_single_bool_parameter

//____________________________________________________________________________//

namespace test_parameter_combination {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<std::string, struct k2_t> k2;
nfp::typed_keyword<bool, struct k3_t> k3;

template<typename Params>
void dotest0( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST_REQUIRE( p.has(k2) );
    BOOST_TEST_REQUIRE( !p.has(k3) );
    BOOST_TEST( p[k1] == 6 );
    BOOST_TEST( p[k2] == "123" );
}

template<typename Params>
void dotest1( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST_REQUIRE( !p.has(k2) );
    BOOST_TEST_REQUIRE( p.has(k3) );
    BOOST_TEST( p[k1] == 6 );
    BOOST_TEST( p[k3] );
}

template<typename Params>
void dotest2( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST_REQUIRE( p.has(k2) );
    BOOST_TEST_REQUIRE( p.has(k3) );
    BOOST_TEST( p[k1] == 5 );
    BOOST_TEST( p[k2] == "1q" );
    BOOST_TEST( !p[k3] );
}

BOOST_AUTO_TEST_CASE( test_parameter_combination )
{
    dotest0(( k1=6, k2="123" ));
    dotest1(( k3, k1=6 ));
    dotest2(( k2 = "1q", !k3, k1=5 ));
}

} // test_parameter_combination

//____________________________________________________________________________//

namespace test_const_arg {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<char const*, struct k2_t> k2;

template<typename Params>
void dotest0( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST_REQUIRE( p.has(k2) );
    BOOST_TEST( p[k1] == 3 );
    BOOST_TEST( p[k2] == "123" );
}

BOOST_AUTO_TEST_CASE( test_const_arg )
{
    int const val = 3;
    dotest0(( k1=val, k2="123" ));
}

} // test_const_arg

//____________________________________________________________________________//

// the unit tests below assert functionality for non copyable classes, C++11 only
#if    !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) \
    && !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS) \
    && !defined(BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS)

namespace test_mutable_arg {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<std::string, struct k2_t> k2;

template<typename Params>
void dotest0( Params&& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( p[k1] == 2 );
    BOOST_TEST( p[k2] == "qwe" );
    p[k1] += 2;
    BOOST_TEST( p[k1] == 4 );
    p[k2] = "asd";
}

BOOST_AUTO_TEST_CASE( test_mutable_arg )
{
    int val = 2;
    std::string str = "qwe";
    dotest0(( k1=val, k2=str ));
    BOOST_TEST( val == 4 );
    BOOST_TEST( str == "asd" );
}

} // test_mutable_arg

//____________________________________________________________________________//


namespace test_noncopyable_arg {

struct NC {
    NC( int v ) : val( v ) {}
    int val;

    NC( NC const& ) = delete;
    NC( NC&& ) = delete;
    void operator=( NC const& ) = delete;
    void operator=( NC&& ) = delete;
};

nfp::typed_keyword<NC, struct k1_t> k1;

template<typename Params>
void dotest0( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( p[k1].val == 9 );
}

BOOST_AUTO_TEST_CASE( test_noncopyable_arg )
{
    dotest0(( k1=NC{ 9 } ));
}

} // test_noncopyable_arg


#endif /* C++11 only */

//____________________________________________________________________________//

namespace test_required_arg {

nfp::typed_keyword<int, struct k1_t, true> k1;
nfp::typed_keyword<int, struct k2_t, true> k2;

template<typename Params>
void dotest0( Params const& p )
{
// won't compile    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( p[k1] == 11 );
    BOOST_TEST( p[k2] == 10 );
}

BOOST_AUTO_TEST_CASE( test_required_arg )
{
    dotest0(( k1=11, k2=10 ));
// won't compile    dotest0(( k1=11 ));
}

} // test_required_arg

//____________________________________________________________________________//

namespace test_argument_erasure {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<int, struct k2_t> k2;
nfp::typed_keyword<int, struct k3_t> k3;

template<typename Params>
void dotest0_in( Params const& p )
{
    BOOST_TEST_REQUIRE( !p.has(k1) );
    BOOST_TEST_REQUIRE( p.has(k2) );
    BOOST_TEST_REQUIRE( p.has(k3) );
    BOOST_TEST( p[k2] == 12 );
    BOOST_TEST( p[k3] <= 0 );
}

template<typename Params>
void dotest0( Params const& p )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST_REQUIRE( p.has(k2) );
    BOOST_TEST_REQUIRE( p.has(k3) );

    p.erase(k1);

    dotest0_in( p );
}

template<typename Params>
void dotest1_in( Params const& p )
{
    BOOST_TEST_REQUIRE( !p.has(k1) );
    BOOST_TEST_REQUIRE( !p.has(k2) );
    BOOST_TEST_REQUIRE( p.has(k3) );
    BOOST_TEST( p[k3] <= 0 );
}

template<typename Params>
void dotest1( Params const& p )
{
    p.erase(k1);
    p.erase(k2);

    dotest1_in( p );
}

BOOST_AUTO_TEST_CASE( test_argument_erasure )
{
    dotest0(( k1=7, k2=12, k3=0 ));
    dotest0(( k2=12, k1=7, k3=-1 ));
    dotest0(( k3=-2, k2=12, k1=7 ));

    dotest1(( k1=7, k2=12, k3=0 ));
}

} // test_argument_erasure

//____________________________________________________________________________//

namespace test_polymorphic_arg {

nfp::keyword<struct k1_t> k1;

template<typename Params, typename T>
void dotest0( Params const& p, T const& arg )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( p[k1] == arg );
}

BOOST_AUTO_TEST_CASE( test_polymorphic_arg )
{
    dotest0( k1=11, 11 );
    dotest0( k1=std::string("qwe"), "qwe" );
}

} // test_polymorphic_arg

//____________________________________________________________________________//

namespace test_optional_assign {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<int, struct k2_t> k2;

template<typename Params, typename T>
void dotest0( Params const& p, T& targ )
{
    nfp::opt_assign( targ, p, k1 );
}

BOOST_AUTO_TEST_CASE( test_optional_assign )
{
    int value = 0;
    dotest0( k1=11, value );
    BOOST_TEST( value == 11 );

    dotest0( k2=12, value );
    BOOST_TEST( value == 11 );
}

} // test_optional_assign

//____________________________________________________________________________//

namespace test_optional_get {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<int, struct k2_t> k2;

template<typename Params, typename T>
void dotest0( Params const& p, T& targ )
{
    targ = nfp::opt_get( p, k1, T() );
}

BOOST_AUTO_TEST_CASE( test_optional_get )
{
    int value = 0;
    dotest0( k1=11, value );
    BOOST_TEST( value == 11 );

    dotest0( k2=12, value );
    BOOST_TEST( value == 0 );
}

} // namespace test_optional_get

//____________________________________________________________________________//

namespace test_is_named_param_pack {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<int, struct k2_t> k2;

template<typename Params>
void dotest0( Params const& )
{
    BOOST_TEST( nfp::is_named_param_pack<Params>::value );
}

BOOST_AUTO_TEST_CASE( test_is_named_param_pack )
{
    dotest0( k1=11 );
    dotest0(( k1=11, k2=10 ));

    BOOST_TEST( !nfp::is_named_param_pack<int>::value );
    BOOST_TEST( !nfp::is_named_param_pack<bool>::value );
    BOOST_TEST( !nfp::is_named_param_pack<k1_t>::value );
    typedef nfp::typed_keyword<int, struct k1_t> kw_t;
    BOOST_TEST( !nfp::is_named_param_pack<kw_t>::value );
}

} // test_is_named_param_pack

//____________________________________________________________________________//

namespace test_param_type {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<bool, struct k2_t> k2;
nfp::keyword<struct k3_t> k3;
nfp::keyword<struct k4_t> k4;

template<typename T, typename D, typename Params, typename KW>
void dotest0( Params const&, KW const& )
{
    typedef boost::is_same<typename nfp::param_type<Params,KW,D>::type,T> check;
    BOOST_TEST( check::value );
}

BOOST_AUTO_TEST_CASE( test_param_type )
{
    dotest0<int,void>(( k1=11, k2, k3="abc" ), k1 );
    dotest0<bool,void>(( k1=11, k2, k3="abc" ), k2 );
    dotest0<bool,void>(( k1=11, !k2, k3="abc" ), k2 );
    dotest0<double,void>(( k1=11, k2, k3=1.2 ), k3 );
    dotest0<void,void>(( k1=11, k2, k3="abc" ), k4 );
    int const c = 1;
    dotest0<int const*,void>(( k4=&c, k3=1.2 ), k4 );
}

} // test_param_type

//____________________________________________________________________________//

namespace test_has_param {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<bool, struct k2_t, true> k2;
nfp::keyword<struct k3_t> k3;
nfp::typed_keyword<float, struct k4_t> k4;

template<typename Params, typename KW>
void dotest0( Params const&, KW const&, bool exp )
{
    BOOST_TEST( (nfp::has_param<Params,KW>::value) == exp );
}

BOOST_AUTO_TEST_CASE( test_has_param )
{
    dotest0(( k1=11, k2, k3="abc" ), k1, true );
    dotest0(( k1=11, !k2, k3="abc" ), k2, true );
    dotest0(( k2, k1=11, k3="abc" ), k2, true );
    dotest0(( k1=11, k2, k3="abc" ), k3, true );
    dotest0(( k1=11, k2, k3="abc" ), k4, false );
}

} // test_has_param

//____________________________________________________________________________//

namespace test_optional_append {

nfp::typed_keyword<int, struct k1_t> k1;
nfp::typed_keyword<int, struct k2_t> k2;

template<typename Params>
void dotest0( Params const& p, int exp )
{
    BOOST_TEST_REQUIRE( p.has(k1) );
    BOOST_TEST( p[k1] == exp );
}

template<typename Params, typename NP>
void dotest0_fwd( Params const& p, NP const& np, int exp )
{
    dotest0( nfp::opt_append( p, np ), exp );
}

BOOST_AUTO_TEST_CASE( test_optional_append )
{
    dotest0_fwd( k1=11, k2=10, 11 );
    dotest0_fwd( k2=10, k1=11, 11 );
    dotest0_fwd( (k1=9,k2=10), k1=11, 9 );
    dotest0_fwd( (k2=10,k1=9), k1=11, 9 );
}

} // test_optional_append

//____________________________________________________________________________//

#if    !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) \
    && !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS) \
    && !defined(BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS)

namespace test_no_params {

nfp::typed_keyword<int, struct k1_t> k1;

template<typename Params=nfp::no_params_type>
void dotest0( bool exp, Params const& p = nfp::no_params )
{
    BOOST_TEST_REQUIRE( p.has(k1) == exp );
}

BOOST_AUTO_TEST_CASE( test_no_params )
{
    dotest0( false );
    dotest0( true, k1=11 );
}

} // test_no_params
#endif
