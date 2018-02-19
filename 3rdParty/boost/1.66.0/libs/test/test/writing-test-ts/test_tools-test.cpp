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
//  Description : tests all Test Tools but output_test_stream
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/tools/output_test_stream.hpp>
#include <boost/test/execution_monitor.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include <boost/test/output/compiler_log_formatter.hpp>
#include <boost/test/framework.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/test/detail/suppress_warnings.hpp>

// Boost
#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>

// STL
#include <iostream>
#include <iomanip>

#ifdef BOOST_MSVC
# pragma warning(push)
# pragma warning(disable: 4702) // unreachable code
#endif

namespace ut=boost::unit_test;
namespace tt=boost::test_tools;

//____________________________________________________________________________//

#define CHECK_CRITICAL_TOOL_USAGE( tool_usage )     \
{                                                   \
    bool throw_ = false;                            \
    try {                                           \
        tool_usage;                                 \
    } catch( boost::execution_aborted const& ) {    \
        throw_ = true;                              \
    }                                               \
                                                    \
    BOOST_CHECK_MESSAGE( throw_, "not aborted" );   \
}                                                   \
/**/

//____________________________________________________________________________//

// thanks to http://stackoverflow.com/questions/9226400/portable-printing-of-exponent-of-a-double-to-c-iostreams
#if defined(BOOST_MSVC) && (BOOST_MSVC < 1900)
struct ScientificNotationExponentOutputNormalizer {
    ScientificNotationExponentOutputNormalizer() : m_old_format(_set_output_format(_TWO_DIGIT_EXPONENT)) {}

    ~ScientificNotationExponentOutputNormalizer() { _set_output_format(m_old_format); }
private:
    unsigned m_old_format;
};
#else
struct ScientificNotationExponentOutputNormalizer {};
#endif

//____________________________________________________________________________//

class bool_convertible
{
    struct Tester {};
public:
    operator Tester*() const { return static_cast<Tester*>( 0 ) + 1; }
};

//____________________________________________________________________________//

struct shorten_lf : public boost::unit_test::output::compiler_log_formatter
{
    void    print_prefix( std::ostream& output, boost::unit_test::const_string, std::size_t line )
    {
    }
};

//____________________________________________________________________________//

std::string match_file_name( "./baseline-outputs/test_tools-test.pattern" );
std::string save_file_name( "test_tools-test.pattern" );

static tt::output_test_stream&
ots()
{
    static boost::shared_ptr<tt::output_test_stream> inst;

    if( !inst ) {
        inst.reset(
            ut::framework::master_test_suite().argc <= 1
                ? new tt::output_test_stream( ut::runtime_config::save_pattern() ? save_file_name : match_file_name, !ut::runtime_config::save_pattern() )
                : new tt::output_test_stream( ut::framework::master_test_suite().argv[1], !ut::runtime_config::save_pattern() ) );
    }

    return *inst;
}

//____________________________________________________________________________//

#define TEST_CASE( name )                                           \
static void name ## _impl();                                        \
static void name ## _impl_defer();                                  \
                                                                    \
BOOST_AUTO_TEST_CASE( name )                                        \
{                                                                   \
    ut::test_case* impl = ut::make_test_case(&name ## _impl,        \
                                             #name,                 \
                                             __FILE__,              \
                                             __LINE__ );            \
    impl->p_default_status.value = ut::test_unit::RS_ENABLED;       \
                                                                    \
    ut::unit_test_log.set_stream( ots() );                          \
    ut::unit_test_log.set_threshold_level( ut::log_nothing );       \
    ut::unit_test_log.set_formatter( new shorten_lf );              \
    ut::framework::finalize_setup_phase( impl->p_id );              \
    ut::framework::run( impl );                                     \
                                                                    \
    ut::log_level ll = ut::runtime_config::get<ut::log_level>(      \
        ut::runtime_config::btrt_log_level );                       \
    ut::output_format lf = ut::runtime_config::get<ut::output_format>( \
        ut::runtime_config::btrt_log_format );                      \
                                                                    \
    ut::unit_test_log.set_threshold_level(                          \
        ll != ut::invalid_log_level ? ll : ut::log_all_errors );    \
    ut::unit_test_log.set_format( lf );                             \
    ut::unit_test_log.set_stream( std::cout );                      \
    BOOST_CHECK( ots().match_pattern() );                           \
}                                                                   \
                                                                    \
void name ## _impl()                                                \
{                                                                   \
    ut::unit_test_log.set_threshold_level( ut::log_all_errors );    \
                                                                    \
    name ## _impl_defer();                                          \
                                                                    \
    ut::unit_test_log.set_threshold_level( ut::log_nothing );       \
}                                                                   \
                                                                    \
void name ## _impl_defer()                                          \
/**/

//____________________________________________________________________________//

TEST_CASE( test_BOOST_WARN )
{
    ut::unit_test_log.set_threshold_level( ut::log_warnings );
    BOOST_WARN( sizeof(int) == sizeof(short) );

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    BOOST_WARN( sizeof(unsigned char) == sizeof(char) );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_CHECK )
{
    // check correct behavior in if clause
    if( true )
        BOOST_CHECK( true );

    // check correct behavior in else clause
    if( false )
    {}
    else
        BOOST_CHECK( true );

    bool_convertible bc;
    BOOST_CHECK( bc );

    int i=2;

    BOOST_CHECK( false );
    BOOST_CHECK( 1==2 );
    BOOST_CHECK( i==1 );

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    BOOST_CHECK( i==2 );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_REQUIRE )
{
    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE( true ) );

    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE( false ) );

    int j = 3;

    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE( j > 5 ) );

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE( j < 5 ) );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_WARN_MESSAGE )
{
    BOOST_WARN_MESSAGE( sizeof(int) == sizeof(short), "memory won't be used efficiently" );
    int obj_size = 33;

    BOOST_WARN_MESSAGE( obj_size <= 8,
                        "object size " << obj_size << " is too big to be efficiently passed by value" );

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    BOOST_WARN_MESSAGE( obj_size > 8, "object size " << obj_size << " is too small" );
}

//____________________________________________________________________________//

boost::test_tools::predicate_result
test_pred1()
{
    boost::test_tools::predicate_result res( false );

    res.message() << "Some explanation";

    return res;
}

TEST_CASE( test_BOOST_CHECK_MESSAGE )
{
    BOOST_CHECK_MESSAGE( 2+2 == 5, "Well, may be that what I believe in" );

    BOOST_CHECK_MESSAGE( test_pred1(), "Checking predicate failed" );

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    BOOST_CHECK_MESSAGE( 2+2 == 4, "Could it fail?" );

    int i = 1;
    int j = 2;
    std::string msg = "some explanation";
    BOOST_CHECK_MESSAGE( i > j, "Comparing " << i << " and " << j << ": " << msg );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_REQUIRE_MESSAGE )
{
    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE_MESSAGE( false, "Here we should stop" ) );

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE_MESSAGE( true, "That's OK" ) );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_ERROR )
{
    BOOST_ERROR( "Fail to miss an error" );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_FAIL )
{
    CHECK_CRITICAL_TOOL_USAGE( BOOST_FAIL( "No! No! Show must go on." ) );
}

//____________________________________________________________________________//

struct my_exception {
    explicit my_exception( int ec = 0 ) : m_error_code( ec ) {}

    int m_error_code;
};

bool is_critical( my_exception const& ex ) { return ex.m_error_code < 0; }

TEST_CASE( test_BOOST_CHECK_THROW )
{
    int i=0;
    BOOST_CHECK_THROW( i++, my_exception );

    ut::unit_test_log.set_threshold_level( ut::log_warnings );
    BOOST_WARN_THROW( i++, my_exception );

    ut::unit_test_log.set_threshold_level( ut::log_all_errors );
    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE_THROW( i++, my_exception ) );

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    if( i/10 > 10 )
    {}
    else
        BOOST_CHECK_THROW( throw my_exception(), my_exception ); // unreachable code warning is expected
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_CHECK_EXCEPTION )
{
    BOOST_CHECK_EXCEPTION( throw my_exception( 1 ), my_exception, is_critical ); // unreachable code warning is expected

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    BOOST_CHECK_EXCEPTION( throw my_exception( -1 ), my_exception, is_critical ); // unreachable code warning is expected
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_CHECK_NO_THROW )
{
    int i=0;
    if( i*10 == 0 )
        BOOST_CHECK_NO_THROW( i++ );
    else {}

    BOOST_CHECK_NO_THROW( throw my_exception() ); // unreachable code warning is expected
}

//____________________________________________________________________________//

struct B {
    B( int i ) : m_i( i ) {}

    int m_i;
};

bool          operator==( B const& b1, B const& b2 ) { return b1.m_i == b2.m_i; }
std::ostream& operator<<( std::ostream& str, B const& b ) { return str << "B(" << b.m_i << ")"; }

//____________________________________________________________________________//

struct C {
    C( int i, int id ) : m_i( i ), m_id( id ) {}

    int m_i;
    int m_id;
};

boost::test_tools::predicate_result
operator==( C const& c1, C const& c2 )
{
    boost::test_tools::predicate_result res( c1.m_i == c2.m_i && c1.m_id == c2.m_id );

    if( !res ) {
        if( c1.m_i != c2.m_i )
            res.message() << "Index mismatch";
        else
            res.message() << "Id mismatch";
    }

    return res;
}

std::ostream& operator<<( std::ostream& str, C const& c ) { return str << "C(" << c.m_i << ',' << c.m_id << ")"; }

//____________________________________________________________________________//

TEST_CASE( test_BOOST_CHECK_EQUAL )
{
    int i=1;
    int j=2;
    BOOST_CHECK_EQUAL( i, j );
    BOOST_CHECK_EQUAL( ++i, j );
    BOOST_CHECK_EQUAL( i++, j );

    char const* str1 = "test1";
    char const* str2 = "test12";
    BOOST_CHECK_EQUAL( str1, str2 );

    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );
    BOOST_CHECK_EQUAL( i+1, j );

    char const* str3 = "1test1";
    BOOST_CHECK_EQUAL( str1, str3+1 );

    ut::unit_test_log.set_threshold_level( ut::log_all_errors );
    str1 = NULL;
    str2 = NULL;
    BOOST_CHECK_EQUAL( str1, str2 );

    str1 = "test";
    str2 = NULL;
    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE_EQUAL( str1, str2 ) );

    B b1(1);
    B b2(2);

    ut::unit_test_log.set_threshold_level( ut::log_warnings );
    BOOST_WARN_EQUAL( b1, b2 );

    ut::unit_test_log.set_threshold_level( ( ut::log_all_errors ) );
    C c1( 0, 100 );
    C c2( 0, 101 );
    C c3( 1, 102 );
    BOOST_CHECK_EQUAL( c1, c3 );
    BOOST_CHECK_EQUAL( c1, c2 );

    char ch1 = -2;
    char ch2 = -3;
    BOOST_CHECK_EQUAL(ch1, ch2);
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_CHECK_LOGICAL_EXPR )
{
    int i=1;
    int j=2;
    BOOST_CHECK_NE( i, j );

    BOOST_CHECK_NE( ++i, j );

    BOOST_CHECK_LT( i, j );
    BOOST_CHECK_GT( i, j );

    BOOST_CHECK_LE( i, j );
    BOOST_CHECK_GE( i, j );

    ++i;

    BOOST_CHECK_LE( i, j );
    BOOST_CHECK_GE( j, i );

    char const* str1 = "test1";
    char const* str2 = "test1";

    BOOST_CHECK_NE( str1, str2 );
}

//____________________________________________________________________________//

bool is_even( int i )        { return i%2 == 0;  }
int  foo( int arg, int mod ) { return arg % mod; }
bool moo( int arg1, int arg2, int mod ) { return ((arg1+arg2) % mod) == 0; }

BOOST_TEST_DONT_PRINT_LOG_VALUE( std::list<int> )

boost::test_tools::assertion_result
compare_lists( std::list<int> const& l1, std::list<int> const& l2 )
{
    if( l1.size() != l2.size() ) {
        boost::test_tools::predicate_result res( false );

        res.message() << "Different sizes [" << l1.size() << "!=" << l2.size() << "]";

        return res;
    }

    return true;
}

TEST_CASE( test_BOOST_CHECK_PREDICATE )
{
    BOOST_CHECK_PREDICATE( is_even, (14) );

    int i = 17;
    BOOST_CHECK_PREDICATE( is_even, (i) );

    using std::not_equal_to;
    BOOST_CHECK_PREDICATE( not_equal_to<int>(), (i)(17) );

    int j=15;
    BOOST_CHECK_PREDICATE( boost::bind( is_even, boost::bind( &foo, _1, _2 ) ), (i)(j) );

    ut::unit_test_log.set_threshold_level( ut::log_warnings );
    BOOST_WARN_PREDICATE( moo, (12)(i)(j) );

    ut::unit_test_log.set_threshold_level( ( ut::log_all_errors ) );
    std::list<int> l1, l2, l3;
    l1.push_back( 1 );
    l3.push_back( 1 );
    l1.push_back( 2 );
    l3.push_back( 3 );
    BOOST_CHECK_PREDICATE( compare_lists, (l1)(l2) );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_REQUIRE_PREDICATE )
{
    int arg1 = 1;
    int arg2 = 2;

    using std::less_equal;
    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE_PREDICATE( less_equal<int>(), (arg1)(arg2) ) );

    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE_PREDICATE( less_equal<int>(), (arg2)(arg1) ) );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_CHECK_EQUAL_COLLECTIONS )
{
    ut::unit_test_log.set_threshold_level( ( ut::log_all_errors ) );

    int pattern [] = { 1, 2, 3, 4, 5, 6, 7 };

    std::list<int> testlist;

    testlist.push_back( 1 );
    testlist.push_back( 2 );
    testlist.push_back( 4 ); // 3
    testlist.push_back( 4 );
    testlist.push_back( 5 );
    testlist.push_back( 7 ); // 6
    testlist.push_back( 7 );

    BOOST_CHECK_EQUAL_COLLECTIONS( testlist.begin(), testlist.end(), pattern, pattern+7 );
    BOOST_CHECK_EQUAL_COLLECTIONS( testlist.begin(), testlist.end(), pattern, pattern+2 );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_CHECK_BITWISE_EQUAL )
{
    BOOST_CHECK_BITWISE_EQUAL( 0x16, 0x16 );

    BOOST_CHECK_BITWISE_EQUAL( (char)0x06, (char)0x16 );

    ut::unit_test_log.set_threshold_level( ut::log_warnings );
    BOOST_WARN_BITWISE_EQUAL( (char)0x26, (char)0x04 );

    ut::unit_test_log.set_threshold_level( ( ut::log_all_errors ) );
    CHECK_CRITICAL_TOOL_USAGE( BOOST_REQUIRE_BITWISE_EQUAL( (char)0x26, (int)0x26 ) );
}

//____________________________________________________________________________//

struct A {
    friend std::ostream& operator<<( std::ostream& str, A const& ) { str << "struct A"; return str;}
};

TEST_CASE( test_BOOST_TEST_MESSAGE )
{
    ut::unit_test_log.set_threshold_level( ut::log_messages );

    BOOST_TEST_MESSAGE( "still testing" );
    BOOST_TEST_MESSAGE( "1+1=" << 2 );

    int i = 2;
    BOOST_TEST_MESSAGE( i << "+" << i << "=" << (i+i) );

    A a = A();
    BOOST_TEST_MESSAGE( a );

#if BOOST_TEST_USE_STD_LOCALE
    BOOST_TEST_MESSAGE( std::hex << std::showbase << 20 );
#else
    BOOST_TEST_MESSAGE( "0x14" );
#endif

    BOOST_TEST_MESSAGE( std::setw( 4 ) << 20 );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_TEST_CHECKPOINT )
{
    BOOST_TEST_CHECKPOINT( "Going to do a silly things" );

    throw "some error";
}

//____________________________________________________________________________//

bool foo() { throw 1; return true; }

TEST_CASE( test_BOOST_TEST_PASSPOINT )
{
    BOOST_CHECK( foo() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_BOOST_IS_DEFINED )
{
#define SYMBOL1
#define SYMBOL2 std::cout
#define ONE_ARG( arg ) arg
#define TWO_ARG( arg1, arg2 ) BOOST_JOIN( arg1, arg2 )

    BOOST_CHECK( BOOST_IS_DEFINED(SYMBOL1) );
    BOOST_CHECK( BOOST_IS_DEFINED(SYMBOL2) );
    BOOST_CHECK( !BOOST_IS_DEFINED( SYMBOL3 ) );
    BOOST_CHECK( BOOST_IS_DEFINED( ONE_ARG(arg1) ) );
    BOOST_CHECK( !BOOST_IS_DEFINED( ONE_ARG1(arg1) ) );
    BOOST_CHECK( BOOST_IS_DEFINED( TWO_ARG(arg1,arg2) ) );
    BOOST_CHECK( !BOOST_IS_DEFINED( TWO_ARG1(arg1,arg2) ) );
}

//____________________________________________________________________________//

TEST_CASE( test_context_logging )
{
    BOOST_TEST_INFO( "some context" );
    BOOST_CHECK( false );

    int i = 12;
    BOOST_TEST_INFO( "some more context: " << i );
    BOOST_CHECK( false );

    BOOST_TEST_INFO( "info 1" );
    BOOST_TEST_INFO( "info 2" );
    BOOST_TEST_INFO( "info 3" );
    BOOST_CHECK( false );

    BOOST_TEST_CONTEXT( "some sticky context" ) {
        BOOST_CHECK( false );

        BOOST_TEST_INFO( "more context" );
        BOOST_CHECK( false );

        BOOST_TEST_INFO( "different subcontext" );
        BOOST_CHECK( false );
    }

    BOOST_TEST_CONTEXT( "outer context" ) {
        BOOST_CHECK( false );

        BOOST_TEST_CONTEXT( "inner context" ) {
            BOOST_CHECK( false );
        }

        BOOST_CHECK( false );
    }
}

//____________________________________________________________________________//

class FooType {
public:
    FooType&    operator*()     { return *this; }
    operator    bool() const    { return false; }
    int         operator&()     { return 10; }
};

TEST_CASE( test_BOOST_TEST_basic_arithmetic_op )
{
    ut::unit_test_log.set_threshold_level( ut::log_successful_tests );

    BOOST_TEST( true );
    BOOST_TEST( false );

    bool_convertible bc;
    BOOST_TEST( bc );

    int i = 1;

    BOOST_TEST( i == 2 );
    BOOST_TEST( i != 1 );
    BOOST_TEST( i > 2 );
    BOOST_TEST( i < 1 );
    BOOST_TEST( i <= 0 );
    BOOST_TEST( i >= 5 );

    int j = 2;
    BOOST_TEST( i+j >= 5 );
    BOOST_TEST( j-i == 2 );

    int* p = &i;
    BOOST_TEST( *p == 2 );

    BOOST_TEST( j-*p == 0 );

    FooType F;

    BOOST_TEST( FooType() );
    BOOST_TEST( *F );
    BOOST_TEST( **F );
    BOOST_TEST( ***F );
    BOOST_TEST( &F > 100 );
    BOOST_TEST( &*F > 100 );

    BOOST_TEST( (i == 1) & (j == 1) );
    BOOST_TEST( (i == 2) | (j == 1) );

    BOOST_TEST(( i == 1 && j == 1 ));
    BOOST_TEST(( i == 2 || j == 1 ));

    // check correct behavior in if clause
    if( true )
        BOOST_TEST( true );

    // check correct behavior in else clause
    if( false )
    {}
    else
        BOOST_TEST( true );

    BOOST_TEST( i+j==15, "This message reported instead including " << i << " and " << j );

    // Does not work
    // BOOST_TEST( i == 1 && j == 1 );
    // BOOST_TEST( i == 2 || j == 1 );
    // BOOST_TEST( i > 5 ? false : true );
}

BOOST_TEST_SPECIALIZED_COLLECTION_COMPARE(std::vector<int>)

TEST_CASE( test_BOOST_TEST_collection_comp )
{
    std::vector<int> v;
    v.push_back( 1 );
    v.push_back( 2 );
    v.push_back( 3 );

    std::vector<int> v2 = v;

    std::vector<int> l;
    l.push_back( 1 );
    l.push_back( 3 );
    l.push_back( 2 );

    BOOST_TEST( v == l );
    BOOST_TEST( v != v2 );
    BOOST_TEST( v < l );
    BOOST_TEST( v > l );

    BOOST_TEST( v <= l );
    BOOST_TEST( v >= l );
}

//____________________________________________________________________________//

namespace boost{ namespace test_tools{ namespace tt_detail{
template<>
struct print_log_value<double> {
    void    operator()( std::ostream& os, double d )
  {
    std::streamsize curr_prec = os.precision();
    os << std::setprecision(1) << d << std::setprecision( curr_prec );
  }
};
}}}

TEST_CASE( test_BOOST_TEST_fpv_comp )
{
    ScientificNotationExponentOutputNormalizer norm;
    ut::ut_detail::ignore_unused_variable_warning( norm );

    double d1 = 1.1e-5;
    double d2 = 1.101e-5;
    float  f1 = 1.1e-5f;

    BOOST_TEST( tt::fpc_tolerance<double>() == 0 );
    BOOST_TEST( d1 == d2 );
    BOOST_TEST( tt::fpc_tolerance<double>() == 0 );
    BOOST_TEST( d1 == d2, tt::tolerance( 1e-7 ) );
    BOOST_TEST( tt::fpc_tolerance<double>() == 0 );
    BOOST_TEST( d1 != f1, tt::tolerance( 1e-7 ) );
    BOOST_TEST( tt::fpc_tolerance<double>() == 0 );
    BOOST_TEST( tt::fpc_tolerance<float>() == 0 );

    BOOST_TEST( d1 > d2 );
    BOOST_TEST( d1+1./1e20 > d2,  1e-5% tt::tolerance() );
    BOOST_TEST( tt::fpc_tolerance<double>() == 0 );
    BOOST_TEST( d2 <= d1, tt::tolerance( tt::fpc::percent_tolerance( 1e-5 ) ) );
    BOOST_TEST( tt::fpc_tolerance<double>() == 0 );

    BOOST_TEST( d1-1e-5 == 0., tt::tolerance( 1e-7 ) );
    BOOST_TEST( d1-1e-5 != 0., tt::tolerance( 1e-4 ) );
    BOOST_TEST( 0. != 1e-5-d1, tt::tolerance( 1e-4 ) );
    BOOST_TEST( d2-1e-5 < 0., tt::tolerance( 1e-6 ) );

    const double cd = 1.0;
    BOOST_TEST( cd == 1.0);
    BOOST_TEST( 1.0 == cd );
    BOOST_TEST( cd == 1.01, 10.% tt::tolerance());
    BOOST_TEST( 1.01 == cd, 0.1% tt::tolerance() );

    BOOST_TEST( 0.0 == 0.0);
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_TEST_cstring_comp )
{
    char const* str1 = "test1";
    char const* str2 = "test12";
    std::string str3 = "test2";
    char        str4[] = "test3";
    char const* str5 = NULL;
    char const* str6 = "1test1";

    BOOST_TEST( str1 == str2 );
    BOOST_TEST( str1 == str3 );
    BOOST_TEST( str3 == str2 );
    BOOST_TEST( str1 == str4 );
    BOOST_TEST( str3 == str4 );
    BOOST_TEST( str1 == str5 );

    BOOST_TEST( str1 != (str6+1) );
    BOOST_TEST( str5 != str5 );

    BOOST_TEST( str3 < str1 );
    BOOST_TEST( str1 >= str2 );
}

//____________________________________________________________________________//

TEST_CASE( string_comparison_per_element )
{
    using namespace boost::test_tools;

    std::string s1 = "asdfhjk";
    std::string s2 = "asdfgjk";

    BOOST_TEST( s1 == s2, tt::per_element() );

    std::string s3 = "hello world";
    std::string s4 = "helko worlt";

    BOOST_TEST( s3 == s4, tt::per_element() );
}

//____________________________________________________________________________//

TEST_CASE( test_BOOST_TEST_bitwise )
{
    int a = 0xAB;
    int b = 0x88;
    short c = 0x8A;
    // decltype is needed for this to work. Not the case for eg. MSVC 2008.
    BOOST_TEST( a == b, tt::bitwise() );
    BOOST_TEST( c == b, tt::bitwise() );
}

//____________________________________________________________________________//

int goo()
{
    static int i = 0;
    return i++;
}

struct copy_counter : boost::noncopyable {
    static int s_value;

    copy_counter() {}
    copy_counter( copy_counter const& ) { s_value++; }
    copy_counter( copy_counter&& ) {}
};

int copy_counter::s_value = 0;

bool operator==( copy_counter const&, copy_counter const& ) { return true; }
std::ostream& operator<<( std::ostream& os, copy_counter const& ) { return os << "copy_counter"; }

BOOST_AUTO_TEST_CASE( test_argument_handling )
{
    BOOST_TEST( goo() == 0 );
    BOOST_TEST( goo() == 1 );
    BOOST_TEST( 2 == goo() );
    BOOST_TEST( 3 == goo() );
    BOOST_TEST( goo() != 5 );
    BOOST_TEST( copy_counter() == copy_counter() );
    BOOST_TEST( copy_counter::s_value == 0 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_precision_mutation, * ut::expected_failures( 1 ) )
{
    std::streamsize initial_precition = std::cout.precision();
    std::cout.precision(initial_precition);

    BOOST_TEST( 1.2 == 2.3, 10.% tt::tolerance() );

    BOOST_TEST( initial_precition == std::cout.precision() );
}

//____________________________________________________________________________//

// addresses issue #11887
#if !defined(BOOST_NO_CXX11_AUTO_DECLARATIONS) && \
    !defined(BOOST_NO_CXX11_LAMBDAS) && \
    !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && \
    !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST) && \
    !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX)

struct rv_erasure_test {
    rv_erasure_test() : value( 1 ) {}
    ~rv_erasure_test() { value = 0; }

    int value;
};

BOOST_AUTO_TEST_CASE( test_rvalue_erasure )
{
    auto erase_rv = []( rv_erasure_test const& arg ) -> rv_erasure_test const& { return arg; };

    BOOST_TEST( 1 == erase_rv( rv_erasure_test{} ).value );
}

#endif

// EOF
