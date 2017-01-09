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

#define BOOST_TEST_MAIN  "Boost.Math GCD & LCM unit tests"

#include <boost/integer/common_factor.hpp>

#include <boost/config.hpp>                // for BOOST_MSVC, etc.
#include <boost/detail/workaround.hpp>
#include <boost/operators.hpp>
#include <boost/core/lightweight_test.hpp>

#include <istream>  // for std::basic_istream
#include <limits>   // for std::numeric_limits
#include <ostream>  // for std::basic_ostream


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
    using boost::integer::gcd;

    // Originally from Boost.Rational tests
    BOOST_TEST_EQ( gcd<T>(  1,  -1), static_cast<T>( 1) );
    BOOST_TEST_EQ( gcd<T>( -1,   1), static_cast<T>( 1) );
    BOOST_TEST_EQ( gcd<T>(  1,   1), static_cast<T>( 1) );
    BOOST_TEST_EQ( gcd<T>( -1,  -1), static_cast<T>( 1) );
    BOOST_TEST_EQ( gcd<T>(  0,   0), static_cast<T>( 0) );
    BOOST_TEST_EQ( gcd<T>(  7,   0), static_cast<T>( 7) );
    BOOST_TEST_EQ( gcd<T>(  0,   9), static_cast<T>( 9) );
    BOOST_TEST_EQ( gcd<T>( -7,   0), static_cast<T>( 7) );
    BOOST_TEST_EQ( gcd<T>(  0,  -9), static_cast<T>( 9) );
    BOOST_TEST_EQ( gcd<T>( 42,  30), static_cast<T>( 6) );
    BOOST_TEST_EQ( gcd<T>(  6,  -9), static_cast<T>( 3) );
    BOOST_TEST_EQ( gcd<T>(-10, -10), static_cast<T>(10) );
    BOOST_TEST_EQ( gcd<T>(-25, -10), static_cast<T>( 5) );
    BOOST_TEST_EQ( gcd<T>(  3,   7), static_cast<T>( 1) );
    BOOST_TEST_EQ( gcd<T>(  8,   9), static_cast<T>( 1) );
    BOOST_TEST_EQ( gcd<T>(  7,  49), static_cast<T>( 7) );
}

// GCD on unmarked signed integer type
void gcd_unmarked_int_test()
{
    using boost::integer::gcd;

    // The regular signed-integer GCD function performs the unsigned version,
    // then does an absolute-value on the result.  Signed types that are not
    // marked as such (due to no std::numeric_limits specialization) may be off
    // by a sign.
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   1,  -1 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(  -1,   1 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   1,   1 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(  -1,  -1 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   0,   0 )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   7,   0 )), MyInt2( 7) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   0,   9 )), MyInt2( 9) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(  -7,   0 )), MyInt2( 7) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   0,  -9 )), MyInt2( 9) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(  42,  30 )), MyInt2( 6) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   6,  -9 )), MyInt2( 3) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>( -10, -10 )), MyInt2(10) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>( -25, -10 )), MyInt2( 5) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   3,   7 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   8,   9 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(gcd<MyInt2>(   7,  49 )), MyInt2( 7) );
}

// GCD on unsigned integer types
template< class T > void gcd_unsigned_test() // unsigned_test_types
{
    using boost::integer::gcd;

    // Note that unmarked types (i.e. have no std::numeric_limits
    // specialization) are treated like non/unsigned types
    BOOST_TEST_EQ( gcd<T>( 1u,   1u), static_cast<T>( 1u) );
    BOOST_TEST_EQ( gcd<T>( 0u,   0u), static_cast<T>( 0u) );
    BOOST_TEST_EQ( gcd<T>( 7u,   0u), static_cast<T>( 7u) );
    BOOST_TEST_EQ( gcd<T>( 0u,   9u), static_cast<T>( 9u) );
    BOOST_TEST_EQ( gcd<T>(42u,  30u), static_cast<T>( 6u) );
    BOOST_TEST_EQ( gcd<T>( 3u,   7u), static_cast<T>( 1u) );
    BOOST_TEST_EQ( gcd<T>( 8u,   9u), static_cast<T>( 1u) );
    BOOST_TEST_EQ( gcd<T>( 7u,  49u), static_cast<T>( 7u) );
}

// GCD at compile-time
void gcd_static_test()
{
    using boost::integer::static_gcd;

    BOOST_TEST_EQ( (static_gcd< 1,  1>::value), 1 );
    BOOST_TEST_EQ( (static_gcd< 0,  0>::value), 0 );
    BOOST_TEST_EQ( (static_gcd< 7,  0>::value), 7 );
    BOOST_TEST_EQ( (static_gcd< 0,  9>::value), 9 );
    BOOST_TEST_EQ( (static_gcd<42, 30>::value), 6 );
    BOOST_TEST_EQ( (static_gcd< 3,  7>::value), 1 );
    BOOST_TEST_EQ( (static_gcd< 8,  9>::value), 1 );
    BOOST_TEST_EQ( (static_gcd< 7, 49>::value), 7 );
}

// TODO: non-built-in signed and unsigned integer tests, with and without
// numeric_limits specialization; polynominal tests; note any changes if
// built-ins switch to binary-GCD algorithm


// LCM tests

// LCM on signed integer types
template< class T > void lcm_int_test() // signed_test_types
{
    using boost::integer::lcm;

    // Originally from Boost.Rational tests
    BOOST_TEST_EQ( lcm<T>(  1,  -1), static_cast<T>( 1) );
    BOOST_TEST_EQ( lcm<T>( -1,   1), static_cast<T>( 1) );
    BOOST_TEST_EQ( lcm<T>(  1,   1), static_cast<T>( 1) );
    BOOST_TEST_EQ( lcm<T>( -1,  -1), static_cast<T>( 1) );
    BOOST_TEST_EQ( lcm<T>(  0,   0), static_cast<T>( 0) );
    BOOST_TEST_EQ( lcm<T>(  6,   0), static_cast<T>( 0) );
    BOOST_TEST_EQ( lcm<T>(  0,   7), static_cast<T>( 0) );
    BOOST_TEST_EQ( lcm<T>( -5,   0), static_cast<T>( 0) );
    BOOST_TEST_EQ( lcm<T>(  0,  -4), static_cast<T>( 0) );
    BOOST_TEST_EQ( lcm<T>( 18,  30), static_cast<T>(90) );
    BOOST_TEST_EQ( lcm<T>( -6,   9), static_cast<T>(18) );
    BOOST_TEST_EQ( lcm<T>(-10, -10), static_cast<T>(10) );
    BOOST_TEST_EQ( lcm<T>( 25, -10), static_cast<T>(50) );
    BOOST_TEST_EQ( lcm<T>(  3,   7), static_cast<T>(21) );
    BOOST_TEST_EQ( lcm<T>(  8,   9), static_cast<T>(72) );
    BOOST_TEST_EQ( lcm<T>(  7,  49), static_cast<T>(49) );
}

// LCM on unmarked signed integer type
void lcm_unmarked_int_test()
{
    using boost::integer::lcm;

    // The regular signed-integer LCM function performs the unsigned version,
    // then does an absolute-value on the result.  Signed types that are not
    // marked as such (due to no std::numeric_limits specialization) may be off
    // by a sign.
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   1,  -1 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(  -1,   1 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   1,   1 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(  -1,  -1 )), MyInt2( 1) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   0,   0 )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   6,   0 )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   0,   7 )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(  -5,   0 )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   0,  -4 )), MyInt2( 0) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(  18,  30 )), MyInt2(90) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(  -6,   9 )), MyInt2(18) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>( -10, -10 )), MyInt2(10) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(  25, -10 )), MyInt2(50) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   3,   7 )), MyInt2(21) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   8,   9 )), MyInt2(72) );
    BOOST_TEST_EQ( abs(lcm<MyInt2>(   7,  49 )), MyInt2(49) );
}

// LCM on unsigned integer types
template< class T > void lcm_unsigned_test() // unsigned_test_types
{
    using boost::integer::lcm;

    // Note that unmarked types (i.e. have no std::numeric_limits
    // specialization) are treated like non/unsigned types
    BOOST_TEST_EQ( lcm<T>( 1u,   1u), static_cast<T>( 1u) );
    BOOST_TEST_EQ( lcm<T>( 0u,   0u), static_cast<T>( 0u) );
    BOOST_TEST_EQ( lcm<T>( 6u,   0u), static_cast<T>( 0u) );
    BOOST_TEST_EQ( lcm<T>( 0u,   7u), static_cast<T>( 0u) );
    BOOST_TEST_EQ( lcm<T>(18u,  30u), static_cast<T>(90u) );
    BOOST_TEST_EQ( lcm<T>( 3u,   7u), static_cast<T>(21u) );
    BOOST_TEST_EQ( lcm<T>( 8u,   9u), static_cast<T>(72u) );
    BOOST_TEST_EQ( lcm<T>( 7u,  49u), static_cast<T>(49u) );
}

// LCM at compile-time
void lcm_static_test()
{
    using boost::integer::static_lcm;

    BOOST_TEST_EQ( (static_lcm< 1,  1>::value),  1 );
    BOOST_TEST_EQ( (static_lcm< 0,  0>::value),  0 );
    BOOST_TEST_EQ( (static_lcm< 6,  0>::value),  0 );
    BOOST_TEST_EQ( (static_lcm< 0,  7>::value),  0 );
    BOOST_TEST_EQ( (static_lcm<18, 30>::value), 90 );
    BOOST_TEST_EQ( (static_lcm< 3,  7>::value), 21 );
    BOOST_TEST_EQ( (static_lcm< 8,  9>::value), 72 );
    BOOST_TEST_EQ( (static_lcm< 7, 49>::value), 49 );
}

// TODO: see GCD to-do

// main

// Various types to test with each GCD/LCM

#define TEST_SIGNED_( test ) \
    test<signed char>(); \
    test<short>(); \
    test<int>(); \
    test<long>(); \
    test<MyInt1>();

#ifdef BOOST_HAS_LONG_LONG
# define TEST_SIGNED( test ) \
    TEST_SIGNED_( test ) \
    test<boost::long_long_type>();
#elif defined(BOOST_HAS_MS_INT64)
# define TEST_SIGNED( test ) \
    TEST_SIGNED_( test ) \
    test<__int64>();
#endif

#define TEST_UNSIGNED_( test ) \
    test<unsigned char>(); \
    test<unsigned short>(); \
    test<unsigned>(); \
    test<unsigned long>(); \
    test<MyUnsigned1>(); \
    test<MyUnsigned2>();

#ifdef BOOST_HAS_LONG_LONG
# define TEST_UNSIGNED( test ) \
    TEST_UNSIGNED_( test ) \
    test<boost::ulong_long_type>();
#elif defined(BOOST_HAS_MS_INT64)
# define TEST_UNSIGNED( test ) \
    TEST_UNSIGNED_( test ) \
    test<unsigned __int64>();
#endif

int main()
{
    TEST_SIGNED( gcd_int_test )
    gcd_unmarked_int_test();
    TEST_UNSIGNED( gcd_unsigned_test )
    gcd_static_test();

    TEST_SIGNED( lcm_int_test )
    lcm_unmarked_int_test();
    TEST_UNSIGNED( lcm_unsigned_test )
    lcm_static_test();

    return boost::report_errors();
}
