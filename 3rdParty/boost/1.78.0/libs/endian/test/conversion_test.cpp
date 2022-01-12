//  conversion_test.cpp  ---------------------------------------------------------------//

//  Copyright Beman Dawes 2010

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//--------------------------------------------------------------------------------------//

#if defined(_MSC_VER)
# pragma warning( disable: 4127 ) // conditional expression is constant
# if _MSC_VER < 1500
#   pragma warning( disable: 4267 ) // '+=': possible loss of data
# endif
#endif

#include <boost/endian/detail/disable_warnings.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/make_unsigned.hpp>
#include <boost/static_assert.hpp>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cstddef>

namespace be = boost::endian;
using std::cout;
using std::endl;
using boost::int8_t;
using boost::uint8_t;
using boost::int16_t;
using boost::uint16_t;
using boost::int32_t;
using boost::uint32_t;
using boost::int64_t;
using boost::uint64_t;

template <class T> inline T std_endian_reverse(T x) BOOST_NOEXCEPT
{
    T tmp(x);
    std::reverse( reinterpret_cast<unsigned char*>(&tmp), reinterpret_cast<unsigned char*>(&tmp) + sizeof(T) );
    return tmp;
}

namespace
{

    //  values for tests

    static unsigned char const test_value_bytes[] = { 0xF1, 0x02, 0xE3, 0x04, 0xD5, 0x06, 0xC7, 0x08 };

    template<class T> void native_value( T& x )
    {
        BOOST_STATIC_ASSERT( boost::is_integral<T>::value && sizeof( T ) <= 8 );
        std::memcpy( &x, test_value_bytes, sizeof( x ) );
    }

    template<class T> void little_value( T& x )
    {
        BOOST_STATIC_ASSERT( boost::is_integral<T>::value && sizeof( T ) <= 8 );

        typedef typename boost::make_unsigned<T>::type U;

        x = 0;

        for( std::size_t i = 0; i < sizeof( x ); ++i )
        {
            x += static_cast<U>( test_value_bytes[ i ] ) << ( 8 * i );
        }
    }

    template<class T> void big_value( T& x )
    {
        BOOST_STATIC_ASSERT( boost::is_integral<T>::value && sizeof( T ) <= 8 );

        typedef typename boost::make_unsigned<T>::type U;

        x = 0;

        for( std::size_t i = 0; i < sizeof( x ); ++i )
        {
            x += static_cast<U>( test_value_bytes[ i ] ) << ( 8 * ( sizeof( x ) - i - 1 ) );
        }
    }

  template <class T>
  void test()
  {
    T native;
    T big;
    T little;
    native_value(native);
    big_value(big);
    little_value(little);

    //  validate the values used by the tests below

    if( be::order::native == be::order::big )
    {
        BOOST_TEST_EQ(native, big);
        BOOST_TEST_EQ(::std_endian_reverse(native), little);
    }
    else
    {
        BOOST_TEST_EQ(::std_endian_reverse(native), big);
        BOOST_TEST_EQ(native, little);
    }

    //  value-by-value tests

    //  unconditional reverse
    BOOST_TEST_EQ(be::endian_reverse(big), little);
    BOOST_TEST_EQ(be::endian_reverse(little), big);

    //  conditional reverse
    BOOST_TEST_EQ(be::native_to_big(native), big);
    BOOST_TEST_EQ(be::native_to_little(native), little);
    BOOST_TEST_EQ(be::big_to_native(big), native);
    BOOST_TEST_EQ(be::little_to_native(little), native);

    //  generic conditional reverse
    BOOST_TEST_EQ((be::conditional_reverse<be::order::big, be::order::big>(big)), big);
    BOOST_TEST_EQ((be::conditional_reverse<be::order::little,
      be::order::little>(little)), little);
    BOOST_TEST_EQ((be::conditional_reverse<be::order::native,
      be::order::native>(native)), native);
    BOOST_TEST_EQ((be::conditional_reverse<be::order::big,
      be::order::little>(big)), little);
    BOOST_TEST_EQ((be::conditional_reverse<be::order::big,
      be::order::native>(big)), native);
    BOOST_TEST_EQ((be::conditional_reverse<be::order::little,
      be::order::big>(little)), big);
    BOOST_TEST_EQ((be::conditional_reverse<be::order::little,
      be::order::native>(little)), native);
    BOOST_TEST_EQ((be::conditional_reverse<be::order::native,
      be::order::big>(native)), big);
    BOOST_TEST_EQ((be::conditional_reverse<be::order::native,
      be::order::little>(native)), little);

    //  runtime conditional reverse
    BOOST_TEST_EQ((be::conditional_reverse(big, be::order::big, be::order::big)),
      big);
    BOOST_TEST_EQ((be::conditional_reverse(little, be::order::little,
      be::order::little)), little);
    BOOST_TEST_EQ((be::conditional_reverse(native, be::order::native,
      be::order::native)), native);
    BOOST_TEST_EQ((be::conditional_reverse(big, be::order::big,
      be::order::little)), little);
    BOOST_TEST_EQ((be::conditional_reverse(big, be::order::big,
      be::order::native)), native);
    BOOST_TEST_EQ((be::conditional_reverse(little, be::order::little,
      be::order::big)), big);
    BOOST_TEST_EQ((be::conditional_reverse(little, be::order::little,
      be::order::native)), native);
    BOOST_TEST_EQ((be::conditional_reverse(native, be::order::native,
      be::order::big)), big);
    BOOST_TEST_EQ((be::conditional_reverse(native, be::order::native,
      be::order::little)), little);

    //  modify-in-place tests

    T x;

    //  unconditional reverse
    x = big; be::endian_reverse_inplace(x); BOOST_TEST_EQ(x, little);
    x = little; be::endian_reverse_inplace(x); BOOST_TEST_EQ(x, big);

    //  conditional reverse
    x = native; be::native_to_big_inplace(x); BOOST_TEST_EQ(x, big);
    x = native; be::native_to_little_inplace(x);  BOOST_TEST_EQ(x, little);
    x = big; be::big_to_native_inplace(x);  BOOST_TEST_EQ(x, native);
    x = little; be::little_to_native_inplace(x); BOOST_TEST_EQ(x, native);

    //  generic conditional reverse
    x = big; be::conditional_reverse_inplace<be::order::big, be::order::big>(x);
      BOOST_TEST_EQ(x, big);
    x = little; be::conditional_reverse_inplace<be::order::little, be::order::little>(x);
      BOOST_TEST_EQ(x, little);
    x = native; be::conditional_reverse_inplace<be::order::native, be::order::native>(x);
      BOOST_TEST_EQ(x, native);
    x = big; be::conditional_reverse_inplace<be::order::big, be::order::little>(x);
      BOOST_TEST_EQ(x, little);
    x = big; be::conditional_reverse_inplace<be::order::big, be::order::native>(x);
      BOOST_TEST_EQ(x, native);
    x = little; be::conditional_reverse_inplace<be::order::little, be::order::big>(x);
      BOOST_TEST_EQ(x, big);
    x = little; be::conditional_reverse_inplace<be::order::little, be::order::native>(x);
      BOOST_TEST_EQ(x, native);
      x = native; be::conditional_reverse_inplace<be::order::native, be::order::big>(x);
      BOOST_TEST_EQ(x, big);
    x = native; be::conditional_reverse_inplace<be::order::native, be::order::little>(x);
      BOOST_TEST_EQ(x, little);

    //  runtime conditional reverse
    x = big;
      be::conditional_reverse_inplace(x, be::order::big, be::order::big);
      BOOST_TEST_EQ(x, big);
    x = little;
      be::conditional_reverse_inplace(x, be::order::little, be::order::little);
      BOOST_TEST_EQ(x, little);
    x = native;
      be::conditional_reverse_inplace(x, be::order::native, be::order::native);
      BOOST_TEST_EQ(x, native);
    x = big;
      be::conditional_reverse_inplace(x, be::order::big, be::order::little);
      BOOST_TEST_EQ(x, little);
    x = big;
      be::conditional_reverse_inplace(x, be::order::big, be::order::native);
      BOOST_TEST_EQ(x, native);
    x = little;
      be::conditional_reverse_inplace(x, be::order::little, be::order::big);
      BOOST_TEST_EQ(x, big);
    x = little;
      be::conditional_reverse_inplace(x, be::order::little, be::order::native);
      BOOST_TEST_EQ(x, native);
    x = native;
      be::conditional_reverse_inplace(x, be::order::native, be::order::big);
      BOOST_TEST_EQ(x, big);
    x = native;
      be::conditional_reverse_inplace(x, be::order::native, be::order::little);
      BOOST_TEST_EQ(x, little);

  }

//--------------------------------------------------------------------------------------//

  template <class UDT>
  void udt_test()
  {
    UDT udt, tmp;
    int64_t big;
    int64_t little;
    int64_t native;
    big_value(big);
    little_value(little);
    native_value(native);

    udt.member1 = big;
    udt.member2 = little;
    udt.member3 = native;

    tmp = be::conditional_reverse<be::order::big, be::order::little>(udt);
    BOOST_TEST_EQ(tmp.member1, be::endian_reverse(big));
    BOOST_TEST_EQ(tmp.member2, be::endian_reverse(little));
    BOOST_TEST_EQ(tmp.member3, be::endian_reverse(native));

    be::conditional_reverse_inplace<be::order::big, be::order::little>(udt);
    BOOST_TEST_EQ(udt.member1, be::endian_reverse(big));
    BOOST_TEST_EQ(udt.member2, be::endian_reverse(little));
    BOOST_TEST_EQ(udt.member3, be::endian_reverse(native));

    udt.member1 = big;
    udt.member2 = little;
    udt.member3 = native;
    tmp.member1 = tmp.member2 = tmp.member3 = 0;

    tmp = be::conditional_reverse<be::order::big, be::order::big>(udt);
    BOOST_TEST_EQ(tmp.member1, big);
    BOOST_TEST_EQ(tmp.member2, little);
    BOOST_TEST_EQ(tmp.member3, native);

    be::conditional_reverse_inplace<be::order::big, be::order::big>(udt);
    BOOST_TEST_EQ(udt.member1, big);
    BOOST_TEST_EQ(udt.member2, little);
    BOOST_TEST_EQ(udt.member3, native);
  }
}  // unnamed namespace

//--------------------------------------------------------------------------------------//

  //  User-defined types

  namespace user
  {
    //  UDT1 supplies both endian_reverse and endian_reverse_inplace
    struct UDT1
    {
      int64_t member1;
      int64_t member2;
      int64_t member3;
    };

    UDT1 endian_reverse(const UDT1& udt) BOOST_NOEXCEPT
    {
      UDT1 tmp;
      tmp.member1 = boost::endian::endian_reverse(udt.member1);
      tmp.member2 = boost::endian::endian_reverse(udt.member2);
      tmp.member3 = boost::endian::endian_reverse(udt.member3);
      return tmp;
    }

    void endian_reverse_inplace(UDT1& udt) BOOST_NOEXCEPT
    {
      boost::endian::endian_reverse_inplace(udt.member1);
      boost::endian::endian_reverse_inplace(udt.member2);
      boost::endian::endian_reverse_inplace(udt.member3);
    }

    //  UDT2 supplies only endian_reverse
    struct UDT2
    {
      int64_t member1;
      int64_t member2;
      int64_t member3;
    };

    UDT2 endian_reverse(const UDT2& udt) BOOST_NOEXCEPT
    {
      UDT2 tmp;
      tmp.member1 = boost::endian::endian_reverse(udt.member1);
      tmp.member2 = boost::endian::endian_reverse(udt.member2);
      tmp.member3 = boost::endian::endian_reverse(udt.member3);
      return tmp;
    }

    //  UDT3 supplies neither endian_reverse nor endian_reverse_inplace,
    //  so udt_test<UDT3>() should fail to compile
    struct UDT3
    {
      int64_t member1;
      int64_t member2;
      int64_t member3;
    };

  }  // namespace user

//--------------------------------------------------------------------------------------//

int cpp_main(int, char * [])
{
  if( be::order::native == be::order::little )
  {
    cout << "Little endian" << endl;
  }
  else if( be::order::native == be::order::big )
  {
    cout << "Big endian" << endl;
  }
  else
  {
    cout << "Unknown endian" << endl;
  }

  cout << "byte swap intrinsics: " BOOST_ENDIAN_INTRINSIC_MSG << endl;

  //std::cerr << std::hex;

  cout << "int8_t" << endl;
  test<int8_t>();
  cout << "uint8_t" << endl;
  test<uint8_t>();

  cout << "int16_t" << endl;
  test<int16_t>();
  cout << "uint16_t" << endl;
  test<uint16_t>();

  cout << "int32_t" << endl;
  test<int32_t>();
  cout << "uint32_t" << endl;
  test<uint32_t>();

  cout << "int64_t" << endl;
  test<int64_t>();
  cout << "uint64_t" << endl;
  test<uint64_t>();

  cout << "UDT 1" << endl;
  udt_test<user::UDT1>();

  cout << "UDT 2" << endl;
  udt_test<user::UDT2>();

#ifdef BOOST_ENDIAN_COMPILE_FAIL
  cout << "UDT 3" << endl;
  udt_test<user::UDT3>();    // should fail to compile since has not endian_reverse()
#endif

  return ::boost::report_errors();
}

int main( int argc, char* argv[] )
{
    try
    {
        return cpp_main( argc, argv );
    }
    catch( std::exception const & x )
    {
        BOOST_ERROR( x.what() );
        return boost::report_errors();
    }
}

#include <boost/endian/detail/disable_warnings_pop.hpp>
