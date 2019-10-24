// Boost.Range library
//
//  Copyright Thorsten Ottosen 2003-2004. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//


#include <boost/detail/workaround.hpp>

#if BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x564))
#  pragma warn -8091 // suppress warning in Boost.Test
#  pragma warn -8057 // unused argument argc/argv in Boost.Test
#endif

#include <boost/range/sub_range.hpp>
#include <boost/range/as_literal.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/test_tools.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace boost_range_test
{
    namespace
    {

void check_sub_range()
{

    typedef std::string::iterator                 iterator;
    typedef std::string::const_iterator           const_iterator;
    typedef boost::iterator_range<iterator>       irange;
    typedef boost::iterator_range<const_iterator> cirange;
    std::string       str  = "hello world";
    const std::string cstr = "const world";
    irange r    = boost::make_iterator_range( str );
    r           = boost::make_iterator_range( str.begin(), str.end() );
    cirange r2  = boost::make_iterator_range( cstr );
    r2          = boost::make_iterator_range( cstr.begin(), cstr.end() );
    r2          = boost::make_iterator_range( str );

    typedef boost::sub_range<std::string>       srange;
    typedef boost::sub_range<const std::string> csrange;
    srange s     = r;
    BOOST_CHECK( r == r );
    BOOST_CHECK( s == r );
    s            = boost::make_iterator_range( str );
    csrange s2   = r;
    s2           = r2;
    s2           = boost::make_iterator_range( cstr );
    BOOST_CHECK( r2 == r2 );
    BOOST_CHECK( s2 != r2 );
    s2           = boost::make_iterator_range( str );
    BOOST_CHECK( !(s != s) );

    BOOST_CHECK( r.begin() == s.begin() );
    BOOST_CHECK( r2.begin()== s2.begin() );
    BOOST_CHECK( r.end()   == s.end() );
    BOOST_CHECK( r2.end()  == s2.end() );
    BOOST_CHECK_EQUAL( r.size(), s.size() );
    BOOST_CHECK_EQUAL( r2.size(), s2.size() );

//#if BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x564))
//    if( !(bool)r )
//        BOOST_CHECK( false );
//    if( !(bool)r2 )
//        BOOST_CHECK( false );
//    if( !(bool)s )
//        BOOST_CHECK( false );
//    if( !(bool)s2 )
//        BOOST_CHECK( false );
//#else
    if( !r )
        BOOST_CHECK( false );
    if( !r2 )
        BOOST_CHECK( false );
    if( !s )
        BOOST_CHECK( false );
    if( !s2 )
        BOOST_CHECK( false );
//#endif

    std::cout << r << r2 << s << s2;

    std::string res  = boost::copy_range<std::string>( r );
    BOOST_CHECK_EQUAL_COLLECTIONS( res.begin(), res.end(), r.begin(), r.end() );

    r.empty();
    s.empty();
    r.size();
    s.size();

    //
    // As of range v2 not legal anymore.
    //
    //irange singular_irange;
    //BOOST_CHECK( singular_irange.empty() );
    //BOOST_CHECK( singular_irange.size() == 0 );
    //
    //srange singular_srange;
    //BOOST_CHECK( singular_srange.empty() );
    //BOOST_CHECK( singular_srange.size() == 0 );
    //
    //BOOST_CHECK( empty( singular_irange ) );
    //BOOST_CHECK( empty( singular_srange ) );
    //

    srange rr = boost::make_iterator_range( str );
    BOOST_CHECK( rr.equal( r ) );

    rr  = boost::make_iterator_range( str.begin(), str.begin() + 5 );
    BOOST_CHECK( rr == boost::as_literal("hello") );
    BOOST_CHECK( rr != boost::as_literal("hell") );
    BOOST_CHECK( rr < boost::as_literal("hello dude") );
    BOOST_CHECK( boost::as_literal("hello") == rr );
    BOOST_CHECK( boost::as_literal("hell")  != rr );
    BOOST_CHECK( ! (boost::as_literal("hello dude") < rr ) );

    irange rrr = rr;
    BOOST_CHECK( rrr == rr );
    BOOST_CHECK( !( rrr != rr ) );
    BOOST_CHECK( !( rrr < rr ) );

    const irange cr = boost::make_iterator_range( str );
    BOOST_CHECK_EQUAL( cr.front(), 'h' );
    BOOST_CHECK_EQUAL( cr.back(), 'd' );
    BOOST_CHECK_EQUAL( cr[1], 'e' );
    BOOST_CHECK_EQUAL( cr(1), 'e' );

    rrr = boost::make_iterator_range( str, 1, -1 );
    BOOST_CHECK( rrr == boost::as_literal("ello worl") );
    rrr = boost::make_iterator_range( rrr, -1, 1 );
    BOOST_CHECK( rrr == str );
    rrr.front() = 'H';
    rrr.back()  = 'D';
    rrr[1]      = 'E';
    BOOST_CHECK( rrr == boost::as_literal("HEllo worlD") );
}

template<class T>
void check_mutable_type(T&)
{
    BOOST_STATIC_ASSERT(!boost::is_const<T>::value);
}

template<class T>
void check_constant_type(T&)
{
    BOOST_STATIC_ASSERT(boost::is_const<T>::value);
}

template<class Range, class Iterator>
void check_is_const_iterator(Iterator it)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            BOOST_DEDUCED_TYPENAME boost::range_iterator<
                BOOST_DEDUCED_TYPENAME boost::add_const<Range>::type
            >::type,
            Iterator
        >::value));
}

template<class Range, class Iterator>
void check_is_iterator(Iterator it)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            BOOST_DEDUCED_TYPENAME boost::range_iterator<
                BOOST_DEDUCED_TYPENAME boost::remove_const<Range>::type
            >::type,
            Iterator
        >::value));
}

void const_propagation_mutable_collection(void)
{
    typedef std::vector<int> coll_t;
    typedef boost::sub_range<coll_t> sub_range_t;

    coll_t c;
    c.push_back(0);

    sub_range_t rng(c);
    const sub_range_t crng(c);

    check_is_iterator<sub_range_t>(rng.begin());
    check_is_iterator<sub_range_t>(rng.end());

    check_is_const_iterator<sub_range_t>(crng.begin());
    check_is_const_iterator<sub_range_t>(crng.end());

    check_mutable_type(rng[0]);
    check_mutable_type(rng.front());
    check_mutable_type(rng.back());
    check_constant_type(crng[0]);
    check_constant_type(crng.front());
    check_constant_type(crng.back());
}

void const_propagation_const_collection(void)
{
    typedef std::vector<int> coll_t;
    typedef boost::sub_range<const coll_t> sub_range_t;

    coll_t c;
    c.push_back(0);

    sub_range_t rng(c);
    const sub_range_t crng(c);

    check_is_const_iterator<sub_range_t>(rng.begin());
    check_is_const_iterator<sub_range_t>(rng.end());

    check_is_const_iterator<sub_range_t>(crng.begin());
    check_is_const_iterator<sub_range_t>(crng.end());

    check_constant_type(rng[0]);
    check_constant_type(rng.front());
    check_constant_type(rng.back());
    check_constant_type(crng[0]);
    check_constant_type(crng.front());
    check_constant_type(crng.back());
}

inline void test_advance()
{
    std::vector<int> l;
    l.push_back(1);
    l.push_back(2);
    typedef boost::sub_range<std::vector<int> > rng_t;
    rng_t r1(l.begin(), l.end());
    BOOST_CHECK(r1.advance_begin(1).advance_end(-1).empty());
    
    rng_t r2(l.begin(), l.end());
    BOOST_CHECK_EQUAL(r2.advance_begin(1).size(), 1u);
    
    rng_t r3(l.begin(), l.end());
    BOOST_CHECK_EQUAL(r3.advance_end(-1).size(), 1u);
}

void ticket_10514()
{
    typedef std::vector<int> vec_t;
    typedef boost::sub_range<vec_t> range_t;
    vec_t v(10);
    range_t r(v.begin(), v.end());
    const range_t& cr = r;
    range_t copy_r = cr;

    BOOST_CHECK(r.begin() == copy_r.begin());
    BOOST_CHECK(r.end() == copy_r.end());

    BOOST_CHECK(cr.begin() == copy_r.begin());
    BOOST_CHECK(cr.end() == copy_r.end());
}

    } // anonymous namespace
} // namespace boost_range_test

boost::unit_test::test_suite* init_unit_test_suite(int, char*[])
{
    boost::unit_test::test_suite* test =
            BOOST_TEST_SUITE( "Boost.Range sub_range test suite" );

    test->add(BOOST_TEST_CASE(&boost_range_test::check_sub_range));

    test->add(BOOST_TEST_CASE(
                  &boost_range_test::const_propagation_const_collection));

    test->add(BOOST_TEST_CASE(
                  &boost_range_test::const_propagation_mutable_collection));

    test->add(BOOST_TEST_CASE(&boost_range_test::test_advance));

    test->add(BOOST_TEST_CASE(&boost_range_test::ticket_10514));

    return test;
}





