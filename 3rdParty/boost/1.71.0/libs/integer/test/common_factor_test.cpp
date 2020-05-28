//  Boost GCD & LCM common_factor.hpp test program  --------------------------//

//  (C) Copyright Daryle Walker 2001, 2006.
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version including documentation.

//  Revision History
//  01 Dec 2006  Various fixes for old compilers (Joaquin M Lopez Munoz)
//  10 Nov 2006  Make long long and __int64 mutually exclusive (Daryle Walker)
//  04 Nov 2006  Use more built-in numeric types, binary-GCD (Daryle Walker)
//  03 Nov 2006  Use custom numeric types (Daryle Walker)
//  02 Nov 2006  Change to Boost.Test's unit test system (Daryle Walker)
//  07 Nov 2001  Initial version (Daryle Walker)

#define BOOST_TEST_MAIN  "Boost.integer GCD & LCM unit tests"

#include <boost/config.hpp>              // for BOOST_MSVC, etc.
#include <boost/detail/workaround.hpp>
#include <boost/integer/common_factor.hpp>  // for boost::integer::gcd, etc.
#include <boost/mpl/list.hpp>            // for boost::mpl::list
#include <boost/operators.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/random.hpp>
#include <boost/rational.hpp>

#include <istream>  // for std::basic_istream
#include <limits>   // for std::numeric_limits
#include <ostream>  // for std::basic_ostream

#ifdef BOOST_INTEGER_HAS_GMPXX_H
#include <gmpxx.h>
#endif

#include "multiprecision_config.hpp"

#ifndef DISABLE_MP_TESTS
#include <boost/multiprecision/cpp_int.hpp>
#endif

namespace {

// TODO: add polynominal/non-real type; especially after any switch to the
// binary-GCD algorithm for built-in types

// Custom integer class (template)
template < typename IntType, int ID = 0 >
class my_wrapped_integer
    : private ::boost::shiftable1<my_wrapped_integer<IntType, ID>,
        ::boost::operators<my_wrapped_integer<IntType, ID> > >
{
    // Helper type-aliases
    typedef my_wrapped_integer    self_type;
    typedef IntType self_type::*  bool_type;

    // Member data
    IntType  v_;

public:
    // Template parameters
    typedef IntType  int_type;

    BOOST_STATIC_CONSTANT(int,id = ID);

    // Lifetime management (use automatic destructor and copy constructor)
    my_wrapped_integer( int_type const &v = int_type() )  : v_( v )  {}

    // Accessors
    int_type  value() const  { return this->v_; }

    // Operators (use automatic copy assignment)
    operator bool_type() const  { return this->v_ ? &self_type::v_ : 0; }

    self_type &  operator ++()  { ++this->v_; return *this; }
    self_type &  operator --()  { --this->v_; return *this; }

    self_type  operator ~() const  { return self_type( ~this->v_ ); }
    self_type  operator !() const  { return self_type( !this->v_ ); }
    self_type  operator +() const  { return self_type( +this->v_ ); }
    self_type  operator -() const  { return self_type( -this->v_ ); }

    bool  operator  <( self_type const &r ) const  { return this->v_ < r.v_; }
    bool  operator ==( self_type const &r ) const  { return this->v_ == r.v_; }

    self_type &operator *=(self_type const &r) {this->v_ *= r.v_; return *this;}
    self_type &operator /=(self_type const &r) {this->v_ /= r.v_; return *this;}
    self_type &operator %=(self_type const &r) {this->v_ %= r.v_; return *this;}
    self_type &operator +=(self_type const &r) {this->v_ += r.v_; return *this;}
    self_type &operator -=(self_type const &r) {this->v_ -= r.v_; return *this;}
    self_type &operator<<=(self_type const &r){this->v_ <<= r.v_; return *this;}
    self_type &operator>>=(self_type const &r){this->v_ >>= r.v_; return *this;}
    self_type &operator &=(self_type const &r) {this->v_ &= r.v_; return *this;}
    self_type &operator |=(self_type const &r) {this->v_ |= r.v_; return *this;}
    self_type &operator ^=(self_type const &r) {this->v_ ^= r.v_; return *this;}

    // Input & output
    friend std::istream & operator >>( std::istream &i, self_type &x )
    { return i >> x.v_; }

    friend std::ostream & operator <<( std::ostream &o, self_type const &x )
    { return o << x.v_; }

};  // my_wrapped_integer

template < typename IntType, int ID >
my_wrapped_integer<IntType, ID>  abs( my_wrapped_integer<IntType, ID> const &x )
{ return ( x < my_wrapped_integer<IntType, ID>(0) ) ? -x : +x; }

typedef my_wrapped_integer<int>          MyInt1;
typedef my_wrapped_integer<unsigned>     MyUnsigned1;
typedef my_wrapped_integer<int, 1>       MyInt2;
typedef my_wrapped_integer<unsigned, 1>  MyUnsigned2;

// Without these explicit instantiations, MSVC++ 6.5/7.0 does not find
// some friend operators in certain contexts.
MyInt1       dummy1;
MyUnsigned1  dummy2;
MyInt2       dummy3;
MyUnsigned2  dummy4;

// Various types to test with each GCD/LCM
typedef ::boost::mpl::list<signed char, short, int, long,
#if BOOST_WORKAROUND(BOOST_MSVC, <= 1500)
#elif defined(BOOST_HAS_LONG_LONG)
 boost::long_long_type,
#elif defined(BOOST_HAS_MS_INT64)
 __int64,
#endif
 MyInt1
#ifndef DISABLE_MP_TESTS
   , boost::multiprecision::cpp_int
#endif
>  signed_test_types;
typedef ::boost::mpl::list<unsigned char, unsigned short, unsigned,
 unsigned long,
#if BOOST_WORKAROUND(BOOST_MSVC, <= 1500)
#elif defined(BOOST_HAS_LONG_LONG)
 boost::ulong_long_type,
#elif defined(BOOST_HAS_MS_INT64)
 unsigned __int64,
#endif
 MyUnsigned1, MyUnsigned2 /*, boost::multiprecision::uint256_t*/>  unsigned_test_types;

}  // namespace

#define BOOST_NO_MACRO_EXPAND /**/

// Specialize numeric_limits for _some_ of our types
namespace std
{

template < >
class numeric_limits< MyInt1 >
{
    typedef MyInt1::int_type             int_type;
    typedef numeric_limits<int_type>  limits_type;

public:
    BOOST_STATIC_CONSTANT(bool, is_specialized = limits_type::is_specialized);

    static MyInt1 min BOOST_NO_MACRO_EXPAND() throw()  { return (limits_type::min)(); }
    static MyInt1 max BOOST_NO_MACRO_EXPAND() throw()  { return (limits_type::max)(); }

    BOOST_STATIC_CONSTANT(int, digits      = limits_type::digits);
    BOOST_STATIC_CONSTANT(int, digits10    = limits_type::digits10);
#ifndef BOOST_NO_CXX11_NUMERIC_LIMITS
    BOOST_STATIC_CONSTANT(int, max_digits10    = limits_type::max_digits10);
#endif
    BOOST_STATIC_CONSTANT(bool, is_signed  = limits_type::is_signed);
    BOOST_STATIC_CONSTANT(bool, is_integer = limits_type::is_integer);
    BOOST_STATIC_CONSTANT(bool, is_exact   = limits_type::is_exact);
    BOOST_STATIC_CONSTANT(int, radix       = limits_type::radix);
    static MyInt1 epsilon() throw()      { return limits_type::epsilon(); }
    static MyInt1 round_error() throw()  { return limits_type::round_error(); }

    BOOST_STATIC_CONSTANT(int, min_exponent   = limits_type::min_exponent);
    BOOST_STATIC_CONSTANT(int, min_exponent10 = limits_type::min_exponent10);
    BOOST_STATIC_CONSTANT(int, max_exponent   = limits_type::max_exponent);
    BOOST_STATIC_CONSTANT(int, max_exponent10 = limits_type::max_exponent10);

    BOOST_STATIC_CONSTANT(bool, has_infinity             = limits_type::has_infinity);
    BOOST_STATIC_CONSTANT(bool, has_quiet_NaN            = limits_type::has_quiet_NaN);
    BOOST_STATIC_CONSTANT(bool, has_signaling_NaN        = limits_type::has_signaling_NaN);
    BOOST_STATIC_CONSTANT(float_denorm_style, has_denorm = limits_type::has_denorm);
    BOOST_STATIC_CONSTANT(bool, has_denorm_loss          = limits_type::has_denorm_loss);

    static MyInt1 infinity() throw()      { return limits_type::infinity(); }
    static MyInt1 quiet_NaN() throw()     { return limits_type::quiet_NaN(); }
    static MyInt1 signaling_NaN() throw() {return limits_type::signaling_NaN();}
    static MyInt1 denorm_min() throw()    { return limits_type::denorm_min(); }

    BOOST_STATIC_CONSTANT(bool, is_iec559  = limits_type::is_iec559);
    BOOST_STATIC_CONSTANT(bool, is_bounded = limits_type::is_bounded);
    BOOST_STATIC_CONSTANT(bool, is_modulo  = limits_type::is_modulo);

    BOOST_STATIC_CONSTANT(bool, traps                    = limits_type::traps);
    BOOST_STATIC_CONSTANT(bool, tinyness_before          = limits_type::tinyness_before);
    BOOST_STATIC_CONSTANT(float_round_style, round_style = limits_type::round_style);

};  // std::numeric_limits<MyInt1>

template < >
class numeric_limits< MyUnsigned1 >
{
    typedef MyUnsigned1::int_type        int_type;
    typedef numeric_limits<int_type>  limits_type;

public:
    BOOST_STATIC_CONSTANT(bool, is_specialized = limits_type::is_specialized);

    static MyUnsigned1 min BOOST_NO_MACRO_EXPAND() throw()  { return (limits_type::min)(); }
    static MyUnsigned1 max BOOST_NO_MACRO_EXPAND() throw()  { return (limits_type::max)(); }

    BOOST_STATIC_CONSTANT(int, digits      = limits_type::digits);
    BOOST_STATIC_CONSTANT(int, digits10    = limits_type::digits10);
#ifndef BOOST_NO_CXX11_NUMERIC_LIMITS
    BOOST_STATIC_CONSTANT(int, max_digits10    = limits_type::max_digits10);
#endif
    BOOST_STATIC_CONSTANT(bool, is_signed  = limits_type::is_signed);
    BOOST_STATIC_CONSTANT(bool, is_integer = limits_type::is_integer);
    BOOST_STATIC_CONSTANT(bool, is_exact   = limits_type::is_exact);
    BOOST_STATIC_CONSTANT(int, radix       = limits_type::radix);
    static MyUnsigned1 epsilon() throw()      { return limits_type::epsilon(); }
    static MyUnsigned1 round_error() throw(){return limits_type::round_error();}

    BOOST_STATIC_CONSTANT(int, min_exponent   = limits_type::min_exponent);
    BOOST_STATIC_CONSTANT(int, min_exponent10 = limits_type::min_exponent10);
    BOOST_STATIC_CONSTANT(int, max_exponent   = limits_type::max_exponent);
    BOOST_STATIC_CONSTANT(int, max_exponent10 = limits_type::max_exponent10);

    BOOST_STATIC_CONSTANT(bool, has_infinity             = limits_type::has_infinity);
    BOOST_STATIC_CONSTANT(bool, has_quiet_NaN            = limits_type::has_quiet_NaN);
    BOOST_STATIC_CONSTANT(bool, has_signaling_NaN        = limits_type::has_signaling_NaN);
    BOOST_STATIC_CONSTANT(float_denorm_style, has_denorm = limits_type::has_denorm);
    BOOST_STATIC_CONSTANT(bool, has_denorm_loss          = limits_type::has_denorm_loss);

    static MyUnsigned1 infinity() throw()    { return limits_type::infinity(); }
    static MyUnsigned1 quiet_NaN() throw()  { return limits_type::quiet_NaN(); }
    static MyUnsigned1 signaling_NaN() throw()
        { return limits_type::signaling_NaN(); }
    static MyUnsigned1 denorm_min() throw(){ return limits_type::denorm_min(); }

    BOOST_STATIC_CONSTANT(bool, is_iec559  = limits_type::is_iec559);
    BOOST_STATIC_CONSTANT(bool, is_bounded = limits_type::is_bounded);
    BOOST_STATIC_CONSTANT(bool, is_modulo  = limits_type::is_modulo);

    BOOST_STATIC_CONSTANT(bool, traps                    = limits_type::traps);
    BOOST_STATIC_CONSTANT(bool, tinyness_before          = limits_type::tinyness_before);
    BOOST_STATIC_CONSTANT(float_round_style, round_style = limits_type::round_style);

};  // std::numeric_limits<MyUnsigned1>

#if BOOST_WORKAROUND(BOOST_MSVC,<1300)
// MSVC 6.0 lacks operator<< for __int64, see
// http://support.microsoft.com/default.aspx?scid=kb;en-us;168440

inline ostream& operator<<(ostream& os, __int64 i)
{
    char buf[20];
    sprintf(buf,"%I64d", i);
    os << buf;
    return os;
}

inline ostream& operator<<(ostream& os, unsigned __int64 i)
{
    char buf[20];
    sprintf(buf,"%I64u", i);
    os << buf;
    return os;
}
#endif

}  // namespace std

// GCD tests

// GCD on signed integer types
template< class T > void gcd_int_test() // signed_test_types
{
#ifndef BOOST_MSVC
    using boost::integer::gcd;
    using boost::integer::gcd_evaluator;
#else
    using namespace boost::integer;
#endif

    // Originally from Boost.Rational tests
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(1), static_cast<T>(-1)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(-1), static_cast<T>(1)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(1), static_cast<T>(1)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(-1), static_cast<T>(-1)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(0), static_cast<T>(0)), static_cast<T>( 0) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(7), static_cast<T>(0)), static_cast<T>( 7) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(0), static_cast<T>(9)), static_cast<T>( 9) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(-7), static_cast<T>(0)), static_cast<T>( 7) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(0), static_cast<T>(-9)), static_cast<T>( 9) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(42), static_cast<T>(30)), static_cast<T>( 6) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(6), static_cast<T>(-9)), static_cast<T>( 3) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(-10), static_cast<T>(-10)), static_cast<T>(10) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(-25), static_cast<T>(-10)), static_cast<T>( 5) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(3), static_cast<T>(7)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(8), static_cast<T>(9)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(7), static_cast<T>(49)), static_cast<T>( 7) );
    // Again with function object:
    BOOST_TEST_EQ(gcd_evaluator<T>()(1, -1), static_cast<T>(1));
    BOOST_TEST_EQ(gcd_evaluator<T>()(-1, 1), static_cast<T>(1));
    BOOST_TEST_EQ(gcd_evaluator<T>()(1, 1), static_cast<T>(1));
    BOOST_TEST_EQ(gcd_evaluator<T>()(-1, -1), static_cast<T>(1));
    BOOST_TEST_EQ(gcd_evaluator<T>()(0, 0), static_cast<T>(0));
    BOOST_TEST_EQ(gcd_evaluator<T>()(7, 0), static_cast<T>(7));
    BOOST_TEST_EQ(gcd_evaluator<T>()(0, 9), static_cast<T>(9));
    BOOST_TEST_EQ(gcd_evaluator<T>()(-7, 0), static_cast<T>(7));
    BOOST_TEST_EQ(gcd_evaluator<T>()(0, -9), static_cast<T>(9));
    BOOST_TEST_EQ(gcd_evaluator<T>()(42, 30), static_cast<T>(6));
    BOOST_TEST_EQ(gcd_evaluator<T>()(6, -9), static_cast<T>(3));
    BOOST_TEST_EQ(gcd_evaluator<T>()(-10, -10), static_cast<T>(10));
    BOOST_TEST_EQ(gcd_evaluator<T>()(-25, -10), static_cast<T>(5));
    BOOST_TEST_EQ(gcd_evaluator<T>()(3, 7), static_cast<T>(1));
    BOOST_TEST_EQ(gcd_evaluator<T>()(8, 9), static_cast<T>(1));
    BOOST_TEST_EQ(gcd_evaluator<T>()(7, 49), static_cast<T>(7));
}

// GCD on unmarked signed integer type
void gcd_unmarked_int_test()
{
#ifndef BOOST_MSVC
    using boost::integer::gcd;
#else
    using namespace boost::integer;
#endif

    // The regular signed-integer GCD function performs the unsigned version,
    // then does an absolute-value on the result.  Signed types that are not
    // marked as such (due to no std::numeric_limits specialization) may be off
    // by a sign.
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(1), static_cast<MyInt2>(-1) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(-1), static_cast<MyInt2>(1) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(1), static_cast<MyInt2>(1) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(-1), static_cast<MyInt2>(-1) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(0), static_cast<MyInt2>(0) )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(7), static_cast<MyInt2>(0) )), MyInt2( 7) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(0), static_cast<MyInt2>(9) )), MyInt2( 9) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(-7), static_cast<MyInt2>(0) )), MyInt2( 7) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(0), static_cast<MyInt2>(-9) )), MyInt2( 9) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(42), static_cast<MyInt2>(30))), MyInt2( 6) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(6), static_cast<MyInt2>(-9) )), MyInt2( 3) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(-10), static_cast<MyInt2>(-10) )), MyInt2(10) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(-25), static_cast<MyInt2>(-10) )), MyInt2( 5) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(3), static_cast<MyInt2>(7) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(8), static_cast<MyInt2>(9) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::gcd(static_cast<MyInt2>(7), static_cast<MyInt2>(49) )), MyInt2( 7) );
}

// GCD on unsigned integer types
template< class T > void gcd_unsigned_test() // unsigned_test_types
{
#ifndef BOOST_MSVC
    using boost::integer::gcd;
#else
    using namespace boost::integer;
#endif

    // Note that unmarked types (i.e. have no std::numeric_limits
    // specialization) are treated like non/unsigned types
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(1u), static_cast<T>(1u)), static_cast<T>( 1u) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(0u), static_cast<T>(0u)), static_cast<T>( 0u) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(7u), static_cast<T>(0u)), static_cast<T>( 7u) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(0u), static_cast<T>(9u)), static_cast<T>( 9u) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(42u), static_cast<T>(30u)), static_cast<T>( 6u) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(3u), static_cast<T>(7u)), static_cast<T>( 1u) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(8u), static_cast<T>(9u)), static_cast<T>( 1u) );
    BOOST_TEST_EQ( boost::integer::gcd(static_cast<T>(7u), static_cast<T>(49u)), static_cast<T>( 7u) );
}

// GCD at compile-time
void gcd_static_test()
{
#ifndef BOOST_MSVC
    using boost::integer::static_gcd;
#else
    using namespace boost::integer;
#endif

    // Can't use "BOOST_TEST_EQ", otherwise the "value" member will be
    // disqualified as compile-time-only constant, needing explicit definition
    BOOST_TEST( (static_gcd< 1,  1>::value) == 1 );
    BOOST_TEST( (static_gcd< 0,  0>::value) == 0 );
    BOOST_TEST( (static_gcd< 7,  0>::value) == 7 );
    BOOST_TEST( (static_gcd< 0,  9>::value) == 9 );
    BOOST_TEST( (static_gcd<42, 30>::value) == 6 );
    BOOST_TEST( (static_gcd< 3,  7>::value) == 1 );
    BOOST_TEST( (static_gcd< 8,  9>::value) == 1 );
    BOOST_TEST( (static_gcd< 7, 49>::value) == 7 );
}

void gcd_method_test()
{
   // Verify that the 3 different methods all yield the same result:
   boost::random::mt19937 gen;
   boost::random::uniform_int_distribution<int> d(0, ((std::numeric_limits<int>::max)() / 2));

   for (unsigned int i = 0; i < 10000; ++i)
   {
      int v1 = d(gen);
      int v2 = d(gen);
      int g = boost::integer::gcd_detail::Euclid_gcd(v1, v2);
      BOOST_TEST(v1 % g == 0);
      BOOST_TEST(v2 % g == 0);
      BOOST_TEST_EQ(g, boost::integer::gcd_detail::mixed_binary_gcd(v1, v2));
      BOOST_TEST_EQ(g, boost::integer::gcd_detail::Stein_gcd(v1, v2));
   }
}

// LCM tests

// LCM on signed integer types
template< class T > void lcm_int_test() // signed_test_types
{
#ifndef BOOST_MSVC
    using boost::integer::lcm;
    using boost::integer::lcm_evaluator;
#else
    using namespace boost::integer;
#endif

    // Originally from Boost.Rational tests
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(1), static_cast<T>(-1)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(-1), static_cast<T>(1)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(1), static_cast<T>(1)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(-1), static_cast<T>(-1)), static_cast<T>( 1) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(0), static_cast<T>(0)), static_cast<T>( 0) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(6), static_cast<T>(0)), static_cast<T>( 0) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(0), static_cast<T>(7)), static_cast<T>( 0) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(-5), static_cast<T>(0)), static_cast<T>( 0) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(0), static_cast<T>(-4)), static_cast<T>( 0) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(18), static_cast<T>(30)), static_cast<T>(90) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(-6), static_cast<T>(9)), static_cast<T>(18) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(-10), static_cast<T>(-10)), static_cast<T>(10) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(25), static_cast<T>(-10)), static_cast<T>(50) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(3), static_cast<T>(7)), static_cast<T>(21) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(8), static_cast<T>(9)), static_cast<T>(72) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(7), static_cast<T>(49)), static_cast<T>(49) );
    // Again with function object:
    BOOST_TEST_EQ(lcm_evaluator<T>()(1, -1), static_cast<T>(1));
    BOOST_TEST_EQ(lcm_evaluator<T>()(-1, 1), static_cast<T>(1));
    BOOST_TEST_EQ(lcm_evaluator<T>()(1, 1), static_cast<T>(1));
    BOOST_TEST_EQ(lcm_evaluator<T>()(-1, -1), static_cast<T>(1));
    BOOST_TEST_EQ(lcm_evaluator<T>()(0, 0), static_cast<T>(0));
    BOOST_TEST_EQ(lcm_evaluator<T>()(6, 0), static_cast<T>(0));
    BOOST_TEST_EQ(lcm_evaluator<T>()(0, 7), static_cast<T>(0));
    BOOST_TEST_EQ(lcm_evaluator<T>()(-5, 0), static_cast<T>(0));
    BOOST_TEST_EQ(lcm_evaluator<T>()(0, -4), static_cast<T>(0));
    BOOST_TEST_EQ(lcm_evaluator<T>()(18, 30), static_cast<T>(90));
    BOOST_TEST_EQ(lcm_evaluator<T>()(-6, 9), static_cast<T>(18));
    BOOST_TEST_EQ(lcm_evaluator<T>()(-10, -10), static_cast<T>(10));
    BOOST_TEST_EQ(lcm_evaluator<T>()(25, -10), static_cast<T>(50));
    BOOST_TEST_EQ(lcm_evaluator<T>()(3, 7), static_cast<T>(21));
    BOOST_TEST_EQ(lcm_evaluator<T>()(8, 9), static_cast<T>(72));
    BOOST_TEST_EQ(lcm_evaluator<T>()(7, 49), static_cast<T>(49));
}

// LCM on unmarked signed integer type
void lcm_unmarked_int_test()
{
#ifndef BOOST_MSVC
    using boost::integer::lcm;
#else
    using namespace boost::integer;
#endif

    // The regular signed-integer LCM function performs the unsigned version,
    // then does an absolute-value on the result.  Signed types that are not
    // marked as such (due to no std::numeric_limits specialization) may be off
    // by a sign.
    BOOST_TEST_EQ( abs(boost::integer::lcm(   static_cast<MyInt2>(1), static_cast<MyInt2>(-1) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(-1), static_cast<MyInt2>(1) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(1), static_cast<MyInt2>(1) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(-1), static_cast<MyInt2>(-1) )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(0), static_cast<MyInt2>(0) )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(6), static_cast<MyInt2>(0) )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(0), static_cast<MyInt2>(7) )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(-5), static_cast<MyInt2>(0) )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(0), static_cast<MyInt2>(-4) )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(18), static_cast<MyInt2>(30) )), MyInt2(90) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(-6), static_cast<MyInt2>(9) )), MyInt2(18) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(-10), static_cast<MyInt2>(-10) )), MyInt2(10) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(25), static_cast<MyInt2>(-10) )), MyInt2(50) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(3), static_cast<MyInt2>(7) )), MyInt2(21) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(8), static_cast<MyInt2>(9) )), MyInt2(72) );
    BOOST_TEST_EQ( abs(boost::integer::lcm(static_cast<MyInt2>(7), static_cast<MyInt2>(49) )), MyInt2(49) );
}

// LCM on unsigned integer types
template< class T > void lcm_unsigned_test() // unsigned_test_types
{
#ifndef BOOST_MSVC
    using boost::integer::lcm;
#else
    using namespace boost::integer;
#endif

    // Note that unmarked types (i.e. have no std::numeric_limits
    // specialization) are treated like non/unsigned types
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(1u), static_cast<T>(1u)), static_cast<T>( 1u) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(0u), static_cast<T>(0u)), static_cast<T>( 0u) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(6u), static_cast<T>(0u)), static_cast<T>( 0u) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(0u), static_cast<T>(7u)), static_cast<T>( 0u) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(18u), static_cast<T>(30u)), static_cast<T>(90u) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(3u), static_cast<T>(7u)), static_cast<T>(21u) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(8u), static_cast<T>(9u)), static_cast<T>(72u) );
    BOOST_TEST_EQ( boost::integer::lcm(static_cast<T>(7u), static_cast<T>(49u)), static_cast<T>(49u) );
}

// LCM at compile-time
void lcm_static_test()
{
#ifndef BOOST_MSVC
    using boost::integer::static_lcm;
#else
    using namespace boost::integer;
#endif

    // Can't use "BOOST_TEST_EQ", otherwise the "value" member will be
    // disqualified as compile-time-only constant, needing explicit definition
    BOOST_TEST( (static_lcm< 1,  1>::value) ==  1 );
    BOOST_TEST( (static_lcm< 0,  0>::value) ==  0 );
    BOOST_TEST( (static_lcm< 6,  0>::value) ==  0 );
    BOOST_TEST( (static_lcm< 0,  7>::value) ==  0 );
    BOOST_TEST( (static_lcm<18, 30>::value) == 90 );
    BOOST_TEST( (static_lcm< 3,  7>::value) == 21 );
    BOOST_TEST( (static_lcm< 8,  9>::value) == 72 );
    BOOST_TEST( (static_lcm< 7, 49>::value) == 49 );
}

void variadics()
{
   unsigned i[] = { 44, 56, 76, 88 };
   BOOST_TEST_EQ(boost::integer::gcd_range(i, i + 4).first, 4);
   BOOST_TEST_EQ(boost::integer::gcd_range(i, i + 4).second, i + 4);
   BOOST_TEST_EQ(boost::integer::lcm_range(i, i + 4).first, 11704);
   BOOST_TEST_EQ(boost::integer::lcm_range(i, i + 4).second, i + 4);

   unsigned i_gcd_unity[] = { 44, 56, 1, 88 };
   BOOST_TEST_EQ(boost::integer::gcd_range(i_gcd_unity, i_gcd_unity + 4).first, 1);
   BOOST_TEST_EQ(boost::integer::gcd_range(i_gcd_unity, i_gcd_unity + 4).second, i_gcd_unity + 3);

   unsigned i_lcm_unity[] = { 44, 56, 0, 88 };
   BOOST_TEST_EQ(boost::integer::lcm_range(i_lcm_unity, i_lcm_unity + 4).first, 0);
   BOOST_TEST_EQ(boost::integer::lcm_range(i_lcm_unity, i_lcm_unity + 4).second, i_lcm_unity + 3);

#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
   BOOST_TEST_EQ(boost::integer::gcd(i[0], i[1], i[2], i[3]), 4);
   BOOST_TEST_EQ(boost::integer::lcm(i[0], i[1], i[2], i[3]), 11704);
#endif
}

// Test case from Boost.Rational, need to make sure we don't break the rational lib:
template <class T> void gcd_and_lcm_on_rationals()
{
   typedef boost::rational<T> rational;
   BOOST_TEST_EQ(boost::integer::gcd(rational(1, 4), rational(1, 3)),
      rational(1, 12));
   BOOST_TEST_EQ(boost::integer::lcm(rational(1, 4), rational(1, 3)),
      rational(1));
}

#ifndef DISABLE_MP_TESTS
#define TEST_SIGNED_( test ) \
    test<signed char>(); \
    test<short>(); \
    test<int>(); \
    test<long>(); \
    test<MyInt1>(); \
    test<boost::multiprecision::cpp_int>(); \
    test<boost::multiprecision::int512_t>();
#else
#define TEST_SIGNED_( test ) \
    test<signed char>(); \
    test<short>(); \
    test<int>(); \
    test<long>(); \
    test<MyInt1>();
#endif

#ifdef BOOST_HAS_LONG_LONG
# define TEST_SIGNED__( test ) \
    TEST_SIGNED_( test ) \
    test<boost::long_long_type>();
#elif defined(BOOST_HAS_MS_INT64)
# define TEST_SIGNED__( test ) \
    TEST_SIGNED_( test ) \
    test<__int64>();
#endif
#ifndef DISABLE_MP_TESTS
#define TEST_UNSIGNED_( test ) \
    test<unsigned char>(); \
    test<unsigned short>(); \
    test<unsigned>(); \
    test<unsigned long>(); \
    test<MyUnsigned1>(); \
    test<MyUnsigned2>(); \
    test<boost::multiprecision::uint512_t>();
#else
#define TEST_UNSIGNED_( test ) \
    test<unsigned char>(); \
    test<unsigned short>(); \
    test<unsigned>(); \
    test<unsigned long>(); \
    test<MyUnsigned1>(); \
    test<MyUnsigned2>();
#endif

#ifdef BOOST_HAS_LONG_LONG
# define TEST_UNSIGNED( test ) \
    TEST_UNSIGNED_( test ) \
    test<boost::ulong_long_type>();
#elif defined(BOOST_HAS_MS_INT64)
# define TEST_UNSIGNED( test ) \
    TEST_UNSIGNED_( test ) \
    test<unsigned __int64>();
#endif

#ifdef BOOST_INTEGER_HAS_GMPXX_H
#  define TEST_SIGNED(test)\
      TEST_SIGNED__(test)\
      test<mpz_class>();
#  define TEST_SIGNED_NO_GMP(test) TEST_SIGNED__(test)
#else
#  define TEST_SIGNED(test) TEST_SIGNED__(test)
#  define TEST_SIGNED_NO_GMP(test) TEST_SIGNED__(test)
#endif

int main()
{
   TEST_SIGNED(gcd_int_test)
   gcd_unmarked_int_test();
   TEST_UNSIGNED(gcd_unsigned_test)
   gcd_static_test();
   gcd_method_test();

   TEST_SIGNED(lcm_int_test)
   lcm_unmarked_int_test();
   TEST_UNSIGNED(lcm_unsigned_test)
   lcm_static_test();
   variadics();
   TEST_SIGNED_NO_GMP(gcd_and_lcm_on_rationals)

   return boost::report_errors();
}
