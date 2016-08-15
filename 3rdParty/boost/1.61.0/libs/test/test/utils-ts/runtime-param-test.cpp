//  (C) Copyright Gennadiy Rozental 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : unit test for runtime parameter framework
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE Boost.Test CLA parser test
#include <boost/test/unit_test.hpp>
#include <boost/test/utils/runtime/parameter.hpp>
#include <boost/test/utils/runtime/finalize.hpp>
#include <boost/test/utils/runtime/cla/argv_traverser.hpp>
#include <boost/test/utils/runtime/cla/parser.hpp>
#include <boost/test/utils/runtime/env/fetch.hpp>
namespace utf = boost::unit_test;
namespace rt = boost::runtime;

#include <iostream>

BOOST_AUTO_TEST_SUITE( test_argv_traverser )

BOOST_AUTO_TEST_CASE( test_construction )
{
    char const* argv1[] = { "test.exe" };
    rt::cla::argv_traverser tr1( sizeof(argv1)/sizeof(char const*), argv1 );

    BOOST_TEST( tr1.eoi() );
    BOOST_TEST( tr1.remainder() == 1 );

    char const* argv2[] = { "test.exe", "--abc=1" };
    rt::cla::argv_traverser tr2( sizeof(argv2)/sizeof(char const*), argv2 );

    BOOST_TEST( !tr2.eoi() );
    BOOST_TEST( tr2.remainder() == 2 );

    char const* argv3[] = { "test.exe", "" };
    rt::cla::argv_traverser tr3( sizeof(argv3)/sizeof(char const*), argv3 );

    BOOST_TEST( !tr3.eoi() );
    BOOST_TEST( tr3.remainder() == 2 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_next_token )
{
    char const* argv1[] = { "test.exe", "a", "b", "" };
    rt::cla::argv_traverser tr( sizeof(argv1)/sizeof(char const*), argv1 );

    tr.next_token();
    BOOST_TEST( tr.remainder() == 3 );
    BOOST_TEST( argv1[0] == "test.exe" );
    BOOST_TEST( argv1[1] == "b" );
    BOOST_TEST( argv1[2] == "" );

    tr.next_token();
    BOOST_TEST( tr.remainder() == 2 );
    BOOST_TEST( argv1[0] == "test.exe" );
    BOOST_TEST( argv1[1] == "" );

    tr.next_token();
    BOOST_TEST( tr.remainder() == 1 );
    BOOST_TEST( argv1[0] == "test.exe" );
    BOOST_TEST( tr.eoi() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_current_token )
{
    char const* argv1[] = { "test.exe", "abc", "zxcvb", "as kl", "--ooo=111", "a", "" };
    rt::cla::argv_traverser tr( sizeof(argv1)/sizeof(char const*), argv1 );

    BOOST_TEST( tr.current_token() == "abc" );
    tr.next_token();
    BOOST_TEST( tr.current_token() == "zxcvb" );
    tr.next_token();
    BOOST_TEST( tr.current_token() == "as kl" );
    tr.next_token();
    BOOST_TEST( tr.current_token() == "--ooo=111" );
    tr.next_token();
    BOOST_TEST( tr.current_token() == "a" );
    tr.next_token();
    BOOST_TEST( !tr.eoi() );
    BOOST_TEST( tr.current_token() == rt::cstring() );
    tr.next_token();
    BOOST_TEST( tr.eoi() );
    BOOST_TEST( tr.current_token() == rt::cstring() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_save_token )
{
    char const* argv1[] = { "test.exe", "a", "b", "" };
    rt::cla::argv_traverser tr( sizeof(argv1)/sizeof(char const*), argv1 );

    tr.save_token();
    BOOST_TEST( tr.remainder() == 4 );
    BOOST_TEST( tr.current_token() == "b" );

    tr.save_token();
    BOOST_TEST( tr.remainder() == 4 );
    BOOST_TEST( tr.current_token() == "" );

    tr.save_token();
    BOOST_TEST( tr.remainder() == 4 );
    BOOST_TEST( tr.eoi() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_remainder )
{
    char const* argv[] = { "test.exe", "abcjkl", "zx vb", " 3 ", "" };
    rt::cla::argv_traverser tr( sizeof(argv)/sizeof(char const*), argv );

    tr.next_token();
    tr.save_token();
    tr.next_token();
    tr.save_token();

    BOOST_TEST( tr.remainder() == 3 );
    BOOST_TEST( argv[0] == "test.exe" );
    BOOST_TEST( argv[1] == "zx vb" );
    BOOST_TEST( argv[2] == "" );
}

BOOST_AUTO_TEST_SUITE_END()

//____________________________________________________________________________//
//____________________________________________________________________________//
//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE( test_parameter_specification,
                       * utf::depends_on("test_argv_traverser") )

BOOST_AUTO_TEST_CASE( test_param_construction )
{
    rt::parameter<int> p1( "P1" );

    BOOST_TEST( p1.p_name == "P1" );
    BOOST_TEST( p1.p_description == "" );
    BOOST_TEST( p1.p_env_var == "" );
    BOOST_TEST( p1.p_optional );
    BOOST_TEST( !p1.p_repeatable );
    BOOST_TEST( !p1.p_has_optional_value );

    rt::parameter<int,rt::REQUIRED_PARAM> p2( "P2", (
        rt::description = "123",
        rt::env_var = "E2"
    ));

    BOOST_TEST( p2.p_name == "P2" );
    BOOST_TEST( p2.p_description == "123" );
    BOOST_TEST( p2.p_env_var == "E2" );
    BOOST_TEST( !p2.p_optional );
    BOOST_TEST( !p2.p_repeatable );
    BOOST_TEST( !p2.p_has_optional_value );

    rt::parameter<int,rt::REPEATABLE_PARAM> p4( "P4", (
        rt::description = "123",
        rt::env_var = "E4"
    ));

    BOOST_TEST( p4.p_name == "P4" );
    BOOST_TEST( p4.p_description == "123" );
    BOOST_TEST( p4.p_env_var == "E4" );
    BOOST_TEST( p4.p_optional );
    BOOST_TEST( p4.p_repeatable );
    BOOST_TEST( !p4.p_has_optional_value );

    rt::option p5( "P5", (
        rt::description = "bool arg",
        rt::env_var = "E5"
    ));
    p5.add_cla_id( "-", "b", " " );

    BOOST_TEST( p5.p_name == "P5" );
    BOOST_TEST( p5.p_description == "bool arg" );
    BOOST_TEST( p5.p_env_var == "E5" );
    BOOST_TEST( p5.p_optional );
    BOOST_TEST( !p5.p_repeatable );
    BOOST_TEST( p5.p_has_optional_value );

    rt::option p6( "P6", (
        rt::description = "option with true default",
        rt::env_var = "E6",
        rt::default_value = true
    ));
    p6.add_cla_id( "-", "b", " " );

    BOOST_TEST( p6.p_name == "P6" );
    BOOST_TEST( p6.p_description == "option with true default" );
    BOOST_TEST( p6.p_env_var == "E6" );
    BOOST_TEST( p6.p_optional );
    BOOST_TEST( !p6.p_repeatable );
    BOOST_TEST( p6.p_has_optional_value );

    rt::parameter<int> p3( "P3" );
    p3.add_cla_id( "/", "P3", ":" );
    p3.add_cla_id( "-", "p+p_p", " " );

    BOOST_TEST( p3.cla_ids().size() == 3U );
    BOOST_TEST( p3.cla_ids()[1].m_prefix == "/" );
    BOOST_TEST( p3.cla_ids()[1].m_tag == "P3" );
    BOOST_TEST( p3.cla_ids()[1].m_value_separator == ":" );
    BOOST_TEST( p3.cla_ids()[2].m_prefix == "-" );
    BOOST_TEST( p3.cla_ids()[2].m_tag == "p+p_p" );
    BOOST_TEST( p3.cla_ids()[2].m_value_separator == "" );

    BOOST_CHECK_THROW( p3.add_cla_id( "^", "p", " " ), rt::invalid_cla_id );
    BOOST_CHECK_THROW( p3.add_cla_id( " ", "p", " " ), rt::invalid_cla_id );
    BOOST_CHECK_THROW( p3.add_cla_id( "", "p", " " ), rt::invalid_cla_id );
    BOOST_CHECK_THROW( p3.add_cla_id( "-", "-abc", " " ), rt::invalid_cla_id );
    BOOST_CHECK_THROW( p3.add_cla_id( "-", "b c", " " ), rt::invalid_cla_id );
    BOOST_CHECK_THROW( p3.add_cla_id( "-", "", " " ), rt::invalid_cla_id );
    BOOST_CHECK_THROW( p3.add_cla_id( "-", "a", "-" ), rt::invalid_cla_id );
    BOOST_CHECK_THROW( p3.add_cla_id( "-", "a", "/" ), rt::invalid_cla_id );
    BOOST_CHECK_THROW( p3.add_cla_id( "-", "a", "" ), rt::invalid_cla_id );

    rt::parameter<int> p7( "P7", rt::optional_value = 1);
    BOOST_CHECK_THROW( p7.add_cla_id( "-", "a", " " ), rt::invalid_cla_id );

    BOOST_CHECK_THROW( (rt::parameter<int,rt::REQUIRED_PARAM>( "P", rt::default_value = 1)), rt::invalid_param_spec );
    BOOST_CHECK_THROW( (rt::parameter<int,rt::REPEATABLE_PARAM>( "P", rt::default_value = std::vector<int>{1})), rt::invalid_param_spec );
    BOOST_CHECK_THROW( (rt::parameter<int,rt::REPEATABLE_PARAM>( "P", rt::optional_value = std::vector<int>{1})), rt::invalid_param_spec );

    enum EnumType { V1, V2 };
    rt::enum_parameter<EnumType> p8( "P8", (
        rt::enum_values<EnumType>::value = {
            {"V1", V1},
            {"V2", V2}},
        rt::default_value = V1
    ));

    BOOST_TEST( p8.p_optional );
    BOOST_TEST( !p8.p_repeatable );
    BOOST_TEST( !p8.p_has_optional_value );
    BOOST_TEST( p8.p_has_default_value );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_params_store )
{
    rt::parameter<int> p1( "P1" );
    rt::parameter<int> p2( "P2" );
    rt::parameter<int> p3( "P1" );

    rt::parameters_store S;

    BOOST_TEST( S.is_empty() );

    S.add( p1 );
    S.add( p2 );

    BOOST_CHECK_THROW( S.add( p3 ), rt::duplicate_param );
    BOOST_TEST( !S.is_empty() );
    BOOST_TEST( S.all().size() == 2U );
    BOOST_TEST( S.get("P1")->p_name == "P1" );
    BOOST_TEST( S.get("P2")->p_name == "P2" );
    BOOST_CHECK_THROW( S.get("P3"), rt::unknown_param );
}

BOOST_AUTO_TEST_SUITE_END()

//____________________________________________________________________________//
//____________________________________________________________________________//
//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_param_trie_construction,
                      * utf::depends_on("test_parameter_specification") )
{
    // make sure this does not crash
    rt::parameters_store store;
    rt::cla::parser parser( store );

    rt::parameters_store store1;

    rt::parameter<int> p1( "P1" );
    p1.add_cla_id( "--", "param_one", " " );
    p1.add_cla_id( "-", "p1", " " );
    store1.add( p1 );

    rt::parameter<int> p2( "P2" );
    p2.add_cla_id( "--", "param_two", " " );
    p2.add_cla_id( "-", "p2", " " );
    store1.add( p2 );

    rt::parameter<int> p3( "P3" );
    p3.add_cla_id( "--", "another_", " " );
    p3.add_cla_id( "/", "p3", " " );
    store1.add( p3 );

    rt::parameter<int> p4( "P4" );
    p4.add_cla_id( "-", "param_one", " " );
    store1.add( p4 );

    rt::cla::parser parser1( store1 );

    rt::parameters_store store2;

    rt::parameter<int> p5( "P5" );
    p5.add_cla_id( "-", "paramA", " " );
    store2.add( p5 );

    rt::parameter<int> p6( "P6" );
    p6.add_cla_id( "-", "paramA", " " );
    store2.add( p6 );

    BOOST_CHECK_THROW( rt::cla::parser testparser( store2 ), rt::conflicting_param );

    rt::parameters_store store3;

    rt::parameter<int> p7( "P7" );
    p7.add_cla_id( "-", "paramA", " " );
    store3.add( p7 );

    rt::parameter<int> p8( "P8" );
    p8.add_cla_id( "-", "param", " " );
    store3.add( p8 );

    BOOST_CHECK_THROW( rt::cla::parser testparser( store3 ), rt::conflicting_param );

    rt::parameters_store store4;

    rt::parameter<int> p9( "P9" );
    p9.add_cla_id( "-", "param", " " );
    store4.add( p9 );

    rt::parameter<int> p10( "P10" );
    p10.add_cla_id( "-", "paramA", " " );
    store4.add( p10 );

    BOOST_CHECK_THROW( rt::cla::parser testparser( store4 ), rt::conflicting_param );
}

//____________________________________________________________________________//
//____________________________________________________________________________//
//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_arguments_store )
{
    rt::arguments_store store;

    BOOST_TEST( store.size() == 0U );
    BOOST_TEST( !store.has( "P1" ) );
    BOOST_TEST( !store.has( "P2" ) );
    BOOST_TEST( !store.has( "P3" ) );
    BOOST_TEST( !store.has( "P4" ) );
    BOOST_TEST( !store.has( "P5" ) );

    store.set( "P1", 10 );
    store.set( "P2", std::string("abc") );
    store.set( "P3", rt::cstring("abc") );
    store.set( "P4", true );
    store.set( "P5", std::vector<int>( 1, 12 ) );

    BOOST_TEST( store.has( "P1" ) );
    BOOST_TEST( store.has( "P2" ) );
    BOOST_TEST( store.has( "P3" ) );
    BOOST_TEST( store.has( "P4" ) );
    BOOST_TEST( store.has( "P5" ) );
    BOOST_TEST( store.size() == 5U );

    BOOST_TEST( store.get<int>( "P1" ) == 10 );
    BOOST_TEST( store.get<std::string>( "P2" ) == "abc" );
    BOOST_TEST( store.get<rt::cstring>( "P3" ) == "abc" );
    BOOST_TEST( store.get<bool>( "P4" ) == true);
    BOOST_TEST( store.get<std::vector<int>>( "P5" ) == std::vector<int>( 1, 12 ) );

    store.set( "P1", 20 );
    BOOST_TEST( store.get<int>( "P1" ) == 20 );

    BOOST_CHECK_THROW( store.get<int>( "P0" ), rt::access_to_missing_argument );
    BOOST_CHECK_THROW( store.get<long>( "P1" ), rt::arg_type_mismatch );
}

//____________________________________________________________________________//
//____________________________________________________________________________//
//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE( test_cla_parsing,
                       * utf::depends_on("test_argv_traverser")
                       * utf::depends_on("test_parameter_specification")
                       * utf::depends_on("test_arguments_store") )

BOOST_AUTO_TEST_CASE( test_basic_parsing )
{
    rt::parameters_store params_store;

    rt::parameter<std::string> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "p1", " " );
    params_store.add( p1 );

    rt::parameter<std::string> p2( "P2" );
    p2.add_cla_id( "--", "param_two", "=" );
    p2.add_cla_id( "-", "p2", " " );
    params_store.add( p2 );

    rt::cla::parser testparser( params_store );

    char const* argv1[] = { "test.exe" };
    rt::arguments_store args_store1;

    testparser.parse( sizeof(argv1)/sizeof(char const*), (char**)argv1, args_store1 );

    BOOST_TEST( args_store1.size() == 0U );

    char const* argv2[] = { "test.exe", "--param_one=abc" };
    rt::arguments_store args_store2;

    testparser.parse( sizeof(argv2)/sizeof(char const*), (char**)argv2, args_store2 );

    BOOST_TEST( args_store2.size() == 1U );
    BOOST_TEST( args_store2.has( "P1" ) );
    BOOST_TEST( args_store2.get<std::string>( "P1" ) == "abc" );

    char const* argv3[] = { "test.exe", "--param_two=12" };
    rt::arguments_store args_store3;

    testparser.parse( sizeof(argv3)/sizeof(char const*), (char**)argv3, args_store3 );

    BOOST_TEST( args_store3.size() == 1U );
    BOOST_TEST( args_store3.has( "P2" ) );
    BOOST_TEST( args_store3.get<std::string>( "P2" ) == "12" );

    char const* argv4[] = { "test.exe", "-p1", "aaa", "-p2", "37" };
    rt::arguments_store args_store4;

    testparser.parse( sizeof(argv4)/sizeof(char const*), (char**)argv4, args_store4 );

    BOOST_TEST( args_store4.size() == 2U );
    BOOST_TEST( args_store4.has( "P1" ) );
    BOOST_TEST( args_store4.get<std::string>( "P1" ) == "aaa" );
    BOOST_TEST( args_store4.has( "P2" ) );
    BOOST_TEST( args_store4.get<std::string>( "P2" ) == "37" );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_typed_argument_parsing )
{
    rt::parameters_store params_store;

    rt::parameter<double> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "p1", " " );
    params_store.add( p1 );

    rt::parameter<int> p2( "P2" );
    p2.add_cla_id( "--", "param_two", "=" );
    p2.add_cla_id( "-", "p2", " " );
    params_store.add( p2 );

    rt::option p3( "P3" );
    p3.add_cla_id( "--", "third", "=" );
    p3.add_cla_id( "-", "p3", " " );
    params_store.add( p3 );

    rt::parameter<rt::cstring> p4( "P4" );
    p4.add_cla_id( "--", "another", "=" );
    p4.add_cla_id( "-", "p4", " " );
    params_store.add( p4 );

    rt::cla::parser testparser( params_store );

    char const* argv1[] = { "test.exe", "--another=some thing", "-p1", "1.2", "-p2", "37", "--third=Y" };
    rt::arguments_store args_store1;

    testparser.parse( sizeof(argv1)/sizeof(char const*), (char**)argv1, args_store1 );

    BOOST_TEST( args_store1.size() == 4U );
    BOOST_TEST( args_store1.has( "P1" ) );
    BOOST_TEST( args_store1.get<double>( "P1" ) == 1.2 );
    BOOST_TEST( args_store1.has( "P2" ) );
    BOOST_TEST( args_store1.get<int>( "P2" ) == 37 );
    BOOST_TEST( args_store1.has( "P3" ) );
    BOOST_TEST( args_store1.get<bool>( "P3" ) );
    BOOST_TEST( args_store1.has( "P4" ) );
    BOOST_TEST( args_store1.get<rt::cstring>( "P4" ) == "some thing" );

    char const* argv2[] = { "test.exe", "-p3" };
    rt::arguments_store args_store2;

    testparser.parse( sizeof(argv2)/sizeof(char const*), (char**)argv2, args_store2 );
    BOOST_TEST( args_store2.size() == 1U );
    BOOST_TEST( args_store2.has( "P3" ) );
    BOOST_TEST( args_store2.get<bool>( "P3" ) );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_parameter_name_guessing )
{
    rt::parameters_store params_store;

    rt::parameter<int> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "one", " " );
    params_store.add( p1 );

    rt::parameter<int> p2( "P2" );
    p2.add_cla_id( "--", "param_two", "=" );
    p2.add_cla_id( "-", "two", " " );
    params_store.add( p2 );

    rt::cla::parser testparser( params_store );

    char const* argv1[] = { "test.exe", "--param_o=1", "-t", "2" };
    rt::arguments_store args_store1;

    testparser.parse( sizeof(argv1)/sizeof(char const*), (char**)argv1, args_store1 );

    BOOST_TEST( args_store1.size() == 2U );
    BOOST_TEST( args_store1.has( "P1" ) );
    BOOST_TEST( args_store1.has( "P2" ) );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_repeatable_parameters )
{
    rt::parameters_store params_store;

    rt::parameter<int,rt::REPEATABLE_PARAM> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "one", " " );
    params_store.add( p1 );

    rt::cla::parser testparser( params_store );
    rt::arguments_store args_store;

    char const* argv[] = { "test.exe", "-one", "1", "-one", "2" };
    testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );

    BOOST_TEST( args_store.size() == 1U );
    BOOST_TEST( args_store.has( "P1" ) );

    std::vector<int> P1_expected{1, 2};
    BOOST_TEST( args_store.get<std::vector<int>>( "P1" ) == P1_expected );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_parameter_with_optional_value )
{
    rt::parameters_store params_store;

    rt::parameter<int> p1( "P1", rt::optional_value = 5 );
    p1.add_cla_id( "--", "param_one", "=" );
    params_store.add( p1 );

    rt::cla::parser testparser( params_store );

    rt::arguments_store args_store1;
    char const* argv1[] = { "test.exe", "--param_one=1" };
    testparser.parse( sizeof(argv1)/sizeof(char const*), (char**)argv1, args_store1 );

    BOOST_TEST( args_store1.size() == 1U );
    BOOST_TEST( args_store1.has( "P1" ) );
    BOOST_TEST( args_store1.get<int>( "P1" ) == 1 );

    rt::arguments_store args_store2;
    char const* argv2[] = { "test.exe", "--param_one" };
    testparser.parse( sizeof(argv2)/sizeof(char const*), (char**)argv2, args_store2 );

    BOOST_TEST( args_store2.size() == 1U );
    BOOST_TEST( args_store2.has( "P1" ) );
    BOOST_TEST( args_store2.get<int>( "P1" ) == 5 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_validations )
{
    rt::parameters_store params_store;

    rt::parameter<int> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "one", " " );
    params_store.add( p1 );

    rt::parameter<int> p2( "P2" );
    p2.add_cla_id( "--", "param_two", "=" );
    p2.add_cla_id( "-", "two", " " );
    params_store.add( p2 );

    rt::option p3( "P3" );
    p3.add_cla_id( "--", "option", "=" );
    params_store.add( p3 );

    rt::cla::parser testparser( params_store );
    rt::arguments_store args_store;

    {
        char const* argv[] = { "test.exe", "--=1" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::format_error );
    }

    {
        char const* argv[] = { "test.exe", "--param_one:1" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::format_error );
    }

    {
        char const* argv[] = { "test.exe", "--param_one=" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::format_error );
    }

    {
        char const* argv[] = { "test.exe", "--param_one=1", "--param_one=2" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::duplicate_arg );
    }

    {
        char const* argv[] = { "test.exe", "--param=1" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::ambiguous_param );
    }

    {
        char const* argv[] = { "test.exe", "--opt=Yeah" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::format_error );
    }

    {
        char const* argv[] = { "test.exe", "param_one=1" };
        BOOST_TEST( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ) == 2 );
    }

    {
        char const* argv[] = { "test.exe", "---param_one=1" };
        BOOST_TEST( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ) == 2 );
    }

    {
        char const* argv[] = { "test.exe", "=1" };
        BOOST_TEST( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ) == 2 );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_unrecognized_param_suggestions )
{
    rt::parameters_store params_store;

    rt::parameter<int> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "p1", " " );
    params_store.add( p1 );

    rt::parameter<int> p2( "P2" );
    p2.add_cla_id( "--", "param_two", "=" );
    params_store.add( p2 );

    rt::parameter<int> p3( "P3" );
    p3.add_cla_id( "--", "param_three", "=" );
    params_store.add( p3 );

    rt::cla::parser testparser( params_store );
    rt::arguments_store args_store;

    {
        char const* argv[] = { "test.exe", "--laram_one=1" };
        BOOST_CHECK_EXCEPTION( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ),
                               rt::unrecognized_param,
                               []( rt::unrecognized_param const& ex ) -> bool {
                                    return ex.m_typo_candidates == std::vector<rt::cstring>{"param_one"};
                               });
    }

    {
        char const* argv[] = { "test.exe", "--paran_one=1" };
        BOOST_CHECK_EXCEPTION( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ),
                               rt::unrecognized_param,
                               []( rt::unrecognized_param const& ex ) -> bool {
                                    return ex.m_typo_candidates == std::vector<rt::cstring>{"param_one"};
                               });
    }

    {
        char const* argv[] = { "test.exe", "--param_onw=1" };
        BOOST_CHECK_EXCEPTION( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ),
                               rt::unrecognized_param,
                               []( rt::unrecognized_param const& ex ) -> bool {
                                    return ex.m_typo_candidates == std::vector<rt::cstring>{"param_one"};
                               });
    }

    {
        char const* argv[] = { "test.exe", "--param_to=1" };
        BOOST_CHECK_EXCEPTION( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ),
                               rt::unrecognized_param,
                               ([]( rt::unrecognized_param const& ex ) -> bool {
                                    return ex.m_typo_candidates == std::vector<rt::cstring>{"param_three", "param_two"};
                               }));
    }

    {
        char const* argv[] = { "test.exe", "--paramtwo=1" };
        BOOST_CHECK_EXCEPTION( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ),
                               rt::unrecognized_param,
                               ([]( rt::unrecognized_param const& ex ) -> bool {
                                    return ex.m_typo_candidates == std::vector<rt::cstring>{"param_two"};
                               }));
    }

    {
        char const* argv[] = { "test.exe", "--parum_=1" };
        BOOST_CHECK_EXCEPTION( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ),
                               rt::unrecognized_param,
                               ([]( rt::unrecognized_param const& ex ) -> bool {
                                    return ex.m_typo_candidates == std::vector<rt::cstring>{};
                               }));
    }

    {
        char const* argv[] = { "test.exe", "--param__one=1" };
        BOOST_CHECK_EXCEPTION( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ),
                               rt::unrecognized_param,
                               ([]( rt::unrecognized_param const& ex ) -> bool {
                                    return ex.m_typo_candidates == std::vector<rt::cstring>{"param_one"};
                               }));
    }

    {
        char const* argv[] = { "test.exe", "--param_twoo=1" };
        BOOST_CHECK_EXCEPTION( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ),
                               rt::unrecognized_param,
                               ([]( rt::unrecognized_param const& ex ) -> bool {
                                    return ex.m_typo_candidates == std::vector<rt::cstring>{"param_two"};
                               }));
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_end_of_params )
{
    rt::parameters_store params_store;

    rt::parameter<int> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "p1", " " );
    params_store.add( p1 );

    rt::parameter<int> p2( "P2" );
    p2.add_cla_id( "--", "param_two", "=" );
    params_store.add( p2 );

    BOOST_CHECK_THROW( rt::cla::parser testparser( params_store, rt::end_of_params = "==" ), rt::invalid_cla_id );

    rt::cla::parser testparser( params_store, rt::end_of_params = "--" );

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "--param_one=1", "--", "/abc" };
        int new_argc = testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );

        BOOST_TEST( args_store.size() == 1U );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( args_store.get<int>( "P1" ) == 1 );
        BOOST_TEST( new_argc == 2 );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "-p1", "1", "--", "--param_two=2" };
        int new_argc = testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );

        BOOST_TEST( args_store.size() == 1U );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( !args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<int>( "P1" ) == 1 );
        BOOST_TEST( new_argc == 2 );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_negation_prefix )
{
    rt::parameters_store params_store;

    rt::parameter<int> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "p1", " " );
    params_store.add( p1 );

    rt::option p2( "P2" );
    p2.add_cla_id( "--", "param_two", "=", true );
    p2.add_cla_id( "-", "p2", " ", true );
    p2.add_cla_id( "-", "p3", " " );
    params_store.add( p2 );

    BOOST_CHECK_THROW( rt::cla::parser testparser( params_store, rt::negation_prefix = "no:" ), rt::invalid_cla_id );

    rt::cla::parser testparser( params_store, rt::negation_prefix = "no_" );

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "--no_param_two" };
        testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );

        BOOST_TEST( args_store.size() == 1U );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<bool>( "P2" ) == false );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "-no_p2" };
        testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );

        BOOST_TEST( args_store.size() == 1U );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<bool>( "P2" ) == false );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "--no_param_one" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::format_error );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "--no_param_two=Y" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::format_error );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "-no_p3" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::format_error );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_enum_parameter )
{
    rt::parameters_store params_store;

    enum EnumType { V1, V2, V3 };
    rt::enum_parameter<EnumType> p1( "P1", (
        rt::enum_values<EnumType>::value = {
            {"V1", V1},
            {"V2", V2}},
        rt::default_value = V3
    ));

    p1.add_cla_id( "--", "param_one", "=" );
    params_store.add( p1 );

    rt::enum_parameter<EnumType, rt::REPEATABLE_PARAM> p2( "P2", (
        rt::enum_values<EnumType>::value = {
            {"V1", V1},
            {"V2", V2},
            {"V2alt", V2},
            {"V3", V3}}
    ));
    p2.add_cla_id( "--", "param_two", " " );
    params_store.add( p2 );

    rt::cla::parser testparser( params_store );

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "--param_one=V1" };
        testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( args_store.get<EnumType>( "P1" ) == V1 );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "--param_one=V2" };
        testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );
        rt::finalize_arguments( params_store, args_store );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( args_store.get<EnumType>( "P1" ) == V2 );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe" };
        testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );
        rt::finalize_arguments( params_store, args_store );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( args_store.get<EnumType>( "P1" ) == V3 );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "--param_one=V3" };
        BOOST_CHECK_THROW( testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store ), rt::format_error );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe", "--param_two", "V2alt", "--param_two", "V1", "--param_two", "V3" };
        testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );
        rt::finalize_arguments( params_store, args_store );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<std::vector<EnumType>>( "P2" ) == (std::vector<EnumType>{V2, V1, V3}) );
    }

    {
        rt::arguments_store args_store;
        char const* argv[] = { "test.exe" };
        testparser.parse( sizeof(argv)/sizeof(char const*), (char**)argv, args_store );
        rt::finalize_arguments( params_store, args_store );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<std::vector<EnumType>>( "P2" ) == std::vector<EnumType>{} );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

//____________________________________________________________________________//
//____________________________________________________________________________//
//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_fetch_from_environment )
{
    rt::parameters_store params_store;

    rt::parameter<int> p1( "P1", rt::env_var = "P1VAR");
    params_store.add( p1 );

    rt::option p2( "P2", rt::env_var = "P2VAR");
    params_store.add( p2 );

    rt::option p3( "P3");
    params_store.add( p3 );

    {
        rt::arguments_store args_store;

        auto env_read = []( rt::cstring ) -> std::pair<rt::cstring,bool>
        {
            return std::make_pair( rt::cstring(), false );
        };

        rt::env::env_detail::fetch_absent( params_store, args_store, env_read );

        BOOST_TEST( args_store.size() == 0U );
    }

    {
        rt::arguments_store args_store;
        args_store.set( "P1", 3 );
        args_store.set( "P2", true );

        auto env_read = []( rt::cstring ) -> std::pair<rt::cstring,bool>
        {
            return std::make_pair( rt::cstring(), false );
        };

        rt::env::env_detail::fetch_absent( params_store, args_store, env_read );

        BOOST_TEST( args_store.size() == 2U );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( !args_store.has( "P1VAR" ) );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<int>( "P1" ) == 3 );
        BOOST_TEST( args_store.get<bool>( "P2" ) == true );
    }

    {
        rt::arguments_store args_store;

        auto env_read = []( rt::cstring var_name ) -> std::pair<rt::cstring,bool>
        {
            if( var_name == "P1VAR" )
                return std::make_pair( rt::cstring("5"), true );

            if( var_name == "P2VAR" )
                return std::make_pair( rt::cstring("Y"), true );

            return std::make_pair( rt::cstring(), false );
        };

        rt::env::env_detail::fetch_absent( params_store, args_store, env_read );

        BOOST_TEST( args_store.size() == 2U );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( !args_store.has( "P1VAR" ) );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<int>( "P1" ) == 5 );
        BOOST_TEST( args_store.get<bool>( "P2" ) == true );
    }

    {
        rt::arguments_store args_store;

        auto env_read = []( rt::cstring var_name ) -> std::pair<rt::cstring,bool>
        {
            if( var_name == "P2VAR" )
                return std::make_pair( rt::cstring("No"), true );

            return std::make_pair( rt::cstring(), false );
        };

        rt::env::env_detail::fetch_absent( params_store, args_store, env_read );

        BOOST_TEST( args_store.size() == 1U );
        BOOST_TEST( !args_store.has( "P1" ) );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<bool>( "P2" ) == false );
    }

    {
        rt::arguments_store args_store;

        auto env_read = []( rt::cstring var_name ) -> std::pair<rt::cstring,bool>
        {
            if( var_name == "P2VAR" )
                return std::make_pair( rt::cstring(), true );

            return std::make_pair( rt::cstring(), false );
        };

        rt::env::env_detail::fetch_absent( params_store, args_store, env_read );

        BOOST_TEST( args_store.size() == 1U );
        BOOST_TEST( !args_store.has( "P1" ) );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<bool>( "P2" ) == true );
    }

    {
        rt::arguments_store args_store;

        auto env_read = []( rt::cstring var_name ) -> std::pair<rt::cstring,bool>
        {
            if( var_name == "P1VAR" )
                return std::make_pair( rt::cstring("one"), true );

            return std::make_pair( rt::cstring(), false );
        };

        BOOST_CHECK_THROW( rt::env::env_detail::fetch_absent( params_store, args_store, env_read ),
                           rt::format_error  );
    }

    {
        rt::arguments_store args_store;

        auto env_read = []( rt::cstring var_name ) -> std::pair<rt::cstring,bool>
        {
            if( var_name == "P1VAR" )
                return std::make_pair( rt::cstring(), true );

            return std::make_pair( rt::cstring(), false );
        };

        BOOST_CHECK_THROW( rt::env::env_detail::fetch_absent( params_store, args_store, env_read ),
                           rt::format_error  );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_finalize_arguments )
{
    rt::parameters_store params_store;

    rt::parameter<int,rt::REQUIRED_PARAM> p1( "P1" );
    p1.add_cla_id( "--", "param_one", "=" );
    p1.add_cla_id( "-", "p1", " " );
    params_store.add( p1 );

    rt::parameter<int> p2( "P2", rt::default_value = 10 );
    params_store.add( p2 );

    rt::option p3( "P3" );
    params_store.add( p3 );

    rt::option p4( "P4", rt::default_value = true );

    params_store.add( p4 );

    {
        rt::arguments_store args_store;
        args_store.set( "P1", 3 );

        rt::finalize_arguments( params_store, args_store );

        BOOST_TEST( args_store.size() == 4U );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.has( "P3" ) );
        BOOST_TEST( args_store.has( "P4" ) );
        BOOST_TEST( args_store.get<int>( "P1" ) == 3 );
        BOOST_TEST( args_store.get<int>( "P2" ) == 10 );
        BOOST_TEST( args_store.get<bool>( "P3" ) == false );
        BOOST_TEST( args_store.get<bool>( "P4" ) == true );
    }

    {
        rt::arguments_store args_store;
        args_store.set( "P1", 3 );
        args_store.set( "P2", 4 );

        rt::finalize_arguments( params_store, args_store );

        BOOST_TEST( args_store.size() == 4U );
        BOOST_TEST( args_store.has( "P1" ) );
        BOOST_TEST( args_store.has( "P2" ) );
        BOOST_TEST( args_store.get<int>( "P1" ) == 3 );
        BOOST_TEST( args_store.get<int>( "P2" ) == 4 );
    }

    {
        rt::arguments_store args_store;

        BOOST_CHECK_THROW( rt::finalize_arguments( params_store, args_store ), rt::missing_req_arg );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_param_callback )
{
    rt::parameters_store params_store;

    int counter = 0;
    auto cb = [&counter](rt::cstring param_name )
    {
        BOOST_TEST( param_name == "P1" );
        counter++;
    };

    rt::parameter<int> p1( "P1", rt::callback = cb );
    params_store.add( p1 );

    rt::parameter<int> p2( "P2" );
    params_store.add( p2 );

    {
        rt::arguments_store args_store;
        args_store.set( "P1", 3 );

        rt::finalize_arguments( params_store, args_store );
        BOOST_TEST( counter == 1 );
    }

    {
        rt::arguments_store args_store;
        args_store.set( "P2", 3 );

        rt::finalize_arguments( params_store, args_store );
        BOOST_TEST( counter == 1 );
    }
}

// EOF

// cla help/usage
// build info
