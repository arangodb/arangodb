//  endian_test.cpp  ---------------------------------------------------------//

//  Copyright Beman Dawes 1999-2008

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  See library home page at http://www.boost.org/libs/endian

//----------------------------------------------------------------------------//

//  This test probes for correct endianness, size, and value.

//  See endian_operations_test for tests of operator correctness and interaction
//  between operand types.

//----------------------------------------------------------------------------//

#include <boost/endian/detail/disable_warnings.hpp>

#include <boost/endian/arithmetic.hpp>
#include <boost/cstdint.hpp>
#include <boost/detail/lightweight_main.hpp>

#include <iostream>
#include <limits>
#include <climits>
#include <cstdlib>    // for atoi(), exit()
#include <cstring>    // for memcmp()

using namespace std;             // Not the best programming practice, but I
using namespace boost;           //   want to verify this combination of using
using namespace boost::endian;   //   namespaces works. See endian_operations_test
//                               //   for tests that don't do "using namespace".

#define VERIFY(predicate) verify( predicate, __LINE__ )
#define VERIFY_SIZE(actual, expected) verify_size( actual, expected, __LINE__ )
#define VERIFY_VALUE_AND_OPS(endian_t,expected_t,expected) verify_value_and_ops<endian_t, expected_t>( expected, __LINE__ )
#define VERIFY_BIG_REPRESENTATION(t) verify_representation<t>( true, __LINE__ )
#define VERIFY_LITTLE_REPRESENTATION(t) verify_representation<t>( false, __LINE__ )
#define VERIFY_NATIVE_REPRESENTATION(t) verify_native_representation<t>( __LINE__ )

namespace
{
  int err_count;

  void verify( bool x, int line )
  {
    if ( x ) return;
    ++err_count;
    cout << "Error: verify failed on line " << line << endl;
  }

  void verify_size( size_t actual, size_t expected, int line )
  {
    if ( actual == expected ) return;
    ++err_count;
    cout << "Error: verify size failed on line " << line << endl;
    cout << " A structure with an expected sizeof() " << expected
         << " had an actual sizeof() " << actual
         << "\n This will cause uses of endian types to fail\n";
  }

  template <class Endian, class Base>
  void verify_value_and_ops( const Base & expected, int line )
  {
    Endian v( expected );
    verify( v == expected, line );

    Endian v2;
    v2.operator=( expected );
    verify( v2 == expected, line );

    ++v; // verify integer_cover_operators being applied to this type -
         // will fail to compile if no endian<> specialization is present

    Endian v3( static_cast<Base>( 1 ) );

    Endian x(static_cast<typename Endian::value_type>(v2+v3));
    if ( x == x ) // silence warning
      return;
  }

  const char * big_rep    = "\x12\x34\x56\x78\x9A\xBC\xDE\xF0";
  const char * little_rep = "\xF0\xDE\xBC\x9A\x78\x56\x34\x12";

  template <class Endian>
  void verify_representation( bool is_big, int line )
  {
    int silence = 0;
    Endian x ( static_cast<typename Endian::value_type>
      (0x123456789abcdef0LL + silence) ); // will truncate

    if ( is_big )
      verify( memcmp( &x,
        reinterpret_cast<const char*>(big_rep)+8-sizeof(Endian),
          sizeof(Endian) ) == 0, line );
    else
      verify( memcmp( &x, little_rep, sizeof(Endian) ) == 0, line );
  }

  template <class Endian>
  inline void verify_native_representation( int line )
  {
#   if BOOST_ENDIAN_BIG_BYTE
      verify_representation<Endian>( true, line );
#   else
      verify_representation<Endian>( false, line );
#   endif
  }

  //  detect_order  -----------------------------------------------------//

  void detect_order()
  {
    union View
    {
      long long i;
      unsigned char c[8];
    };

    View v = { 0x0102030405060708LL };  // initialize v.i

    if ( memcmp( v.c, "\x8\7\6\5\4\3\2\1", 8) == 0 )
    {
      cout << "This machine is little-endian.\n";
  #   if !BOOST_ENDIAN_LITTLE_BYTE
        cout << "yet boost/predef/other/endian.h does not define BOOST_ENDIAN_LITTLE_BYTE.\n"
          "The Boost Endian library must be revised to work correctly on this system.\n"
          "Please report this problem to the Boost mailing list.\n";
        exit(1);
  #   endif
    }
    else if ( memcmp( v.c, "\1\2\3\4\5\6\7\x8", 8) == 0 )
    {
      cout << "This machine is big-endian.\n";
  #   if !BOOST_ENDIAN_BIG_BYTE
        cout << "yet boost/predef/other/endian.h does not define BOOST_ENDIAN_BIG_BYTE.\n"
          "The Boost Endian library must be revised to work correctly on this system.\n"
          "Please report this problem to the Boost mailing list.\n";
        exit(1);
  #   endif
    }
    else
    {
      cout << "This machine is neither strict big-endian nor strict little-endian\n"
        "The Boost Endian library must be revised to work correctly on this system.\n"
          "Please report this problem to the Boost mailing list.\n";
      exit(1);
    }
    cout << "That should not matter and is presented for your information only.\n";
  } // detect_order

  //  check_data  ------------------------------------------------------------//

  void check_data()
  {
    big_int8_t big_8;
    big_int16_t big_16;
    big_int24_t big_24;
    big_int32_t big_32;
    big_int40_t big_40;
    big_int48_t big_48;
    big_int56_t big_56;
    big_int64_t big_64;

    big_uint8_t big_u8;
    big_uint16_t big_u16;
    big_uint24_t big_u24;
    big_uint32_t big_u32;
    big_uint40_t big_u40;
    big_uint48_t big_u48;
    big_uint56_t big_u56;
    big_uint64_t big_u64;

    little_int8_t little_8;
    little_int16_t little_16;
    little_int24_t little_24;
    little_int32_t little_32;
    little_int40_t little_40;
    little_int48_t little_48;
    little_int56_t little_56;
    little_int64_t little_64;

    little_uint8_t little_u8;
    little_uint16_t little_u16;
    little_uint24_t little_u24;
    little_uint32_t little_u32;
    little_uint40_t little_u40;
    little_uint48_t little_u48;
    little_uint56_t little_u56;
    little_uint64_t little_u64;

    native_int8_t native_8;
    native_int16_t native_16;
    native_int24_t native_24;
    native_int32_t native_32;
    native_int40_t native_40;
    native_int48_t native_48;
    native_int56_t native_56;
    native_int64_t native_64;

    native_uint8_t native_u8;
    native_uint16_t native_u16;
    native_uint24_t native_u24;
    native_uint32_t native_u32;
    native_uint40_t native_u40;
    native_uint48_t native_u48;
    native_uint56_t native_u56;
    native_uint64_t native_u64;

    big_int16_at  big_align_int16;
    big_int32_at  big_align_int32;
    big_int64_at  big_align_int64;

    big_uint16_at big_align_uint16;
    big_uint32_at big_align_uint32;
    big_uint64_at big_align_uint64;

    little_int16_at  little_align_int16;
    little_int32_at  little_align_int32;
    little_int64_at  little_align_int64;

    little_uint16_at little_align_uint16;
    little_uint32_at little_align_uint32;
    little_uint64_at little_align_uint64;

    VERIFY(big_8.data() == reinterpret_cast<const char *>(&big_8));
    VERIFY(big_16.data() == reinterpret_cast<const char *>(&big_16));
    VERIFY(big_24.data() == reinterpret_cast<const char *>(&big_24));
    VERIFY(big_32.data() == reinterpret_cast<const char *>(&big_32));
    VERIFY(big_40.data() == reinterpret_cast<const char *>(&big_40));
    VERIFY(big_48.data() == reinterpret_cast<const char *>(&big_48));
    VERIFY(big_56.data() == reinterpret_cast<const char *>(&big_56));
    VERIFY(big_64.data() == reinterpret_cast<const char *>(&big_64));

    VERIFY(big_u8.data() == reinterpret_cast<const char *>(&big_u8));
    VERIFY(big_u16.data() == reinterpret_cast<const char *>(&big_u16));
    VERIFY(big_u24.data() == reinterpret_cast<const char *>(&big_u24));
    VERIFY(big_u32.data() == reinterpret_cast<const char *>(&big_u32));
    VERIFY(big_u40.data() == reinterpret_cast<const char *>(&big_u40));
    VERIFY(big_u48.data() == reinterpret_cast<const char *>(&big_u48));
    VERIFY(big_u56.data() == reinterpret_cast<const char *>(&big_u56));
    VERIFY(big_u64.data() == reinterpret_cast<const char *>(&big_u64));

    VERIFY(little_8.data() == reinterpret_cast<const char *>(&little_8));
    VERIFY(little_16.data() == reinterpret_cast<const char *>(&little_16));
    VERIFY(little_24.data() == reinterpret_cast<const char *>(&little_24));
    VERIFY(little_32.data() == reinterpret_cast<const char *>(&little_32));
    VERIFY(little_40.data() == reinterpret_cast<const char *>(&little_40));
    VERIFY(little_48.data() == reinterpret_cast<const char *>(&little_48));
    VERIFY(little_56.data() == reinterpret_cast<const char *>(&little_56));
    VERIFY(little_64.data() == reinterpret_cast<const char *>(&little_64));

    VERIFY(little_u8.data() == reinterpret_cast<const char *>(&little_u8));
    VERIFY(little_u16.data() == reinterpret_cast<const char *>(&little_u16));
    VERIFY(little_u24.data() == reinterpret_cast<const char *>(&little_u24));
    VERIFY(little_u32.data() == reinterpret_cast<const char *>(&little_u32));
    VERIFY(little_u40.data() == reinterpret_cast<const char *>(&little_u40));
    VERIFY(little_u48.data() == reinterpret_cast<const char *>(&little_u48));
    VERIFY(little_u56.data() == reinterpret_cast<const char *>(&little_u56));
    VERIFY(little_u64.data() == reinterpret_cast<const char *>(&little_u64));

    VERIFY(native_8.data() == reinterpret_cast<const char *>(&native_8));
    VERIFY(native_16.data() == reinterpret_cast<const char *>(&native_16));
    VERIFY(native_24.data() == reinterpret_cast<const char *>(&native_24));
    VERIFY(native_32.data() == reinterpret_cast<const char *>(&native_32));
    VERIFY(native_40.data() == reinterpret_cast<const char *>(&native_40));
    VERIFY(native_48.data() == reinterpret_cast<const char *>(&native_48));
    VERIFY(native_56.data() == reinterpret_cast<const char *>(&native_56));
    VERIFY(native_64.data() == reinterpret_cast<const char *>(&native_64));

    VERIFY(native_u8.data() == reinterpret_cast<const char *>(&native_u8));
    VERIFY(native_u16.data() == reinterpret_cast<const char *>(&native_u16));
    VERIFY(native_u24.data() == reinterpret_cast<const char *>(&native_u24));
    VERIFY(native_u32.data() == reinterpret_cast<const char *>(&native_u32));
    VERIFY(native_u40.data() == reinterpret_cast<const char *>(&native_u40));
    VERIFY(native_u48.data() == reinterpret_cast<const char *>(&native_u48));
    VERIFY(native_u56.data() == reinterpret_cast<const char *>(&native_u56));
    VERIFY(native_u64.data() == reinterpret_cast<const char *>(&native_u64));

    VERIFY(big_align_int16.data() == reinterpret_cast<const char *>(&big_align_int16));
    VERIFY(big_align_int32.data() == reinterpret_cast<const char *>(&big_align_int32));
    VERIFY(big_align_int64.data() == reinterpret_cast<const char *>(&big_align_int64));

    VERIFY(big_align_uint16.data() == reinterpret_cast<const char *>(&big_align_uint16));
    VERIFY(big_align_uint32.data() == reinterpret_cast<const char *>(&big_align_uint32));
    VERIFY(big_align_uint64.data() == reinterpret_cast<const char *>(&big_align_uint64));

    VERIFY(little_align_int16.data() == reinterpret_cast<const char *>(&little_align_int16));
    VERIFY(little_align_int32.data() == reinterpret_cast<const char *>(&little_align_int32));
    VERIFY(little_align_int64.data() == reinterpret_cast<const char *>(&little_align_int64));

    VERIFY(little_align_uint16.data() == reinterpret_cast<const char *>(&little_align_uint16));
    VERIFY(little_align_uint32.data() == reinterpret_cast<const char *>(&little_align_uint32));
    VERIFY(little_align_uint64.data() == reinterpret_cast<const char *>(&little_align_uint64));

  }

  //  check_size  ------------------------------------------------------------//

  void check_size()
  {
    VERIFY( numeric_limits<signed char>::digits == 7 );
    VERIFY( numeric_limits<unsigned char>::digits == 8 );

    VERIFY_SIZE( sizeof( big_int8_t ), 1 );
    VERIFY_SIZE( sizeof( big_int16_t ), 2 );
    VERIFY_SIZE( sizeof( big_int24_t ), 3 );
    VERIFY_SIZE( sizeof( big_int32_t ), 4 );
    VERIFY_SIZE( sizeof( big_int40_t ), 5 );
    VERIFY_SIZE( sizeof( big_int48_t ), 6 );
    VERIFY_SIZE( sizeof( big_int56_t ), 7 );
    VERIFY_SIZE( sizeof( big_int64_t ), 8 );

    VERIFY_SIZE( sizeof( big_uint8_t ), 1 );
    VERIFY_SIZE( sizeof( big_uint16_t ), 2 );
    VERIFY_SIZE( sizeof( big_uint24_t ), 3 );
    VERIFY_SIZE( sizeof( big_uint32_t ), 4 );
    VERIFY_SIZE( sizeof( big_uint40_t ), 5 );
    VERIFY_SIZE( sizeof( big_uint48_t ), 6 );
    VERIFY_SIZE( sizeof( big_uint56_t ), 7 );
    VERIFY_SIZE( sizeof( big_uint64_t ), 8 );

    VERIFY_SIZE( sizeof( little_int8_t ), 1 );
    VERIFY_SIZE( sizeof( little_int16_t ), 2 );
    VERIFY_SIZE( sizeof( little_int24_t ), 3 );
    VERIFY_SIZE( sizeof( little_int32_t ), 4 );
    VERIFY_SIZE( sizeof( little_int40_t ), 5 );
    VERIFY_SIZE( sizeof( little_int48_t ), 6 );
    VERIFY_SIZE( sizeof( little_int56_t ), 7 );
    VERIFY_SIZE( sizeof( little_int64_t ), 8 );

    VERIFY_SIZE( sizeof( little_uint8_t ), 1 );
    VERIFY_SIZE( sizeof( little_uint16_t ), 2 );
    VERIFY_SIZE( sizeof( little_uint24_t ), 3 );
    VERIFY_SIZE( sizeof( little_uint32_t ), 4 );
    VERIFY_SIZE( sizeof( little_uint40_t ), 5 );
    VERIFY_SIZE( sizeof( little_uint48_t ), 6 );
    VERIFY_SIZE( sizeof( little_uint56_t ), 7 );
    VERIFY_SIZE( sizeof( little_uint64_t ), 8 );

    VERIFY_SIZE( sizeof( native_int8_t ), 1 );
    VERIFY_SIZE( sizeof( native_int16_t ), 2 );
    VERIFY_SIZE( sizeof( native_int24_t ), 3 );
    VERIFY_SIZE( sizeof( native_int32_t ), 4 );
    VERIFY_SIZE( sizeof( native_int40_t ), 5 );
    VERIFY_SIZE( sizeof( native_int48_t ), 6 );
    VERIFY_SIZE( sizeof( native_int56_t ), 7 );
    VERIFY_SIZE( sizeof( native_int64_t ), 8 );

    VERIFY_SIZE( sizeof( native_uint8_t ), 1 );
    VERIFY_SIZE( sizeof( native_uint16_t ), 2 );
    VERIFY_SIZE( sizeof( native_uint24_t ), 3 );
    VERIFY_SIZE( sizeof( native_uint32_t ), 4 );
    VERIFY_SIZE( sizeof( native_uint40_t ), 5 );
    VERIFY_SIZE( sizeof( native_uint48_t ), 6 );
    VERIFY_SIZE( sizeof( native_uint56_t ), 7 );
    VERIFY_SIZE( sizeof( native_uint64_t ), 8 );

    VERIFY_SIZE(sizeof(big_int8_at), 1);
    VERIFY_SIZE(sizeof(big_int16_at), 2);
    VERIFY_SIZE( sizeof( big_int32_at ), 4 );
    VERIFY_SIZE( sizeof( big_int64_at ), 8 );

    VERIFY_SIZE(sizeof(big_uint8_at), 1);
    VERIFY_SIZE(sizeof(big_uint16_at), 2);
    VERIFY_SIZE( sizeof( big_uint32_at ), 4 );
    VERIFY_SIZE( sizeof( big_uint64_at ), 8 );

    VERIFY_SIZE(sizeof(little_int8_at), 1);
    VERIFY_SIZE(sizeof(little_int16_at), 2);
    VERIFY_SIZE( sizeof( little_int32_at ), 4 );
    VERIFY_SIZE( sizeof( little_int64_at ), 8 );

    VERIFY_SIZE(sizeof(little_uint8_at), 1);
    VERIFY_SIZE(sizeof(little_uint16_at), 2);
    VERIFY_SIZE( sizeof( little_uint32_at ), 4 );
    VERIFY_SIZE( sizeof( little_uint64_at ), 8 );
  } // check_size

  //  check_alignment  -------------------------------------------------------//

  void check_alignment()
  {
    // structs with offsets % 2 == 1 for type of size > 1 to ensure no alignment
    // bytes added for any size > 1

    struct big_struct
    {
      big_int8_t    v0;
      big_int16_t    v1;
      big_int24_t    v3;
      char      v6;
      big_int32_t    v7;
      big_int40_t    v11;
      char      v16;
      big_int48_t    v17;
      big_int56_t    v23;
      char      v30;
      big_int64_t    v31;
    };

    struct big_u_struct
    {
      big_uint8_t    v0;
      big_uint16_t    v1;
      big_uint24_t    v3;
      char       v6;
      big_uint32_t    v7;
      big_uint40_t    v11;
      char       v16;
      big_uint48_t    v17;
      big_uint56_t    v23;
      char       v30;
      big_uint64_t    v31;
    };

    struct little_struct
    {
      little_int8_t    v0;
      little_int16_t    v1;
      little_int24_t    v3;
      char         v6;
      little_int32_t    v7;
      little_int40_t    v11;
      char         v16;
      little_int48_t    v17;
      little_int56_t    v23;
      char         v30;
      little_int64_t    v31;
    };

    struct little_u_struct
    {
      little_uint8_t    v0;
      little_uint16_t    v1;
      little_uint24_t    v3;
      char          v6;
      little_uint32_t    v7;
      little_uint40_t    v11;
      char          v16;
      little_uint48_t    v17;
      little_uint56_t    v23;
      char          v30;
      little_uint64_t    v31;
    };

    struct native_struct
    {
      native_int8_t    v0;
      native_int16_t    v1;
      native_int24_t    v3;
      char         v6;
      native_int32_t    v7;
      native_int40_t    v11;
      char         v16;
      native_int48_t    v17;
      native_int56_t    v23;
      char         v30;
      native_int64_t    v31;
    };

    struct native_u_struct
    {
      native_uint8_t    v0;
      native_uint16_t    v1;
      native_uint24_t    v3;
      char          v6;
      native_uint32_t    v7;
      native_uint40_t    v11;
      char          v16;
      native_uint48_t    v17;
      native_uint56_t    v23;
      char          v30;
      native_uint64_t    v31;
    };

    //  aligned test cases

    struct big_aligned_struct
    {
      big_int16_at    v0;
      big_int32_at    v1;
      char          v3;
      // on a 32-bit system, the padding here may be 3 rather than 7 bytes
      big_int64_at    v4;
    };

    struct little_aligned_struct
    {
      little_int16_at    v0;
      little_int32_at    v1;
      char          v3;
      // on a 32-bit system, the padding here may be 3 rather than 7 bytes
      little_int64_at    v4;
    };

    int saved_err_count = err_count;

    VERIFY_SIZE( sizeof(big_struct), 39 );
    VERIFY_SIZE( sizeof(big_u_struct), 39 );
    VERIFY_SIZE( sizeof(little_struct), 39 );
    VERIFY_SIZE( sizeof(little_u_struct), 39 );
    VERIFY_SIZE( sizeof(native_struct), 39 );
    VERIFY_SIZE( sizeof(native_u_struct), 39 );
    VERIFY( sizeof(big_aligned_struct) <= 24 );
    VERIFY( sizeof(little_aligned_struct) <= 24 );

    if ( saved_err_count == err_count )
    {
      cout <<
        "Size and alignment for structures of endian types are as expected.\n";
    }
  } // check_alignment

  //  check_representation_and_range_and_ops  --------------------------------//

  void check_representation_and_range_and_ops()
  {
    // unaligned integer types
    VERIFY_BIG_REPRESENTATION( big_int8_t );
    VERIFY_VALUE_AND_OPS( big_int8_t, int_least8_t,  0x7e );
    VERIFY_VALUE_AND_OPS( big_int8_t, int_least8_t, -0x80 );

    VERIFY_BIG_REPRESENTATION( big_int16_t );
    VERIFY_VALUE_AND_OPS( big_int16_t, int_least16_t,  0x7ffe );
    VERIFY_VALUE_AND_OPS( big_int16_t, int_least16_t, -0x8000 );

    VERIFY_BIG_REPRESENTATION( big_int24_t );
    VERIFY_VALUE_AND_OPS( big_int24_t, int_least32_t,  0x7ffffe );
    VERIFY_VALUE_AND_OPS( big_int24_t, int_least32_t, -0x800000 );

    VERIFY_BIG_REPRESENTATION( big_int32_t );
    VERIFY_VALUE_AND_OPS( big_int32_t, int_least32_t,  0x7ffffffe );
    VERIFY_VALUE_AND_OPS( big_int32_t, int_least32_t, -0x7fffffff-1 );

    VERIFY_BIG_REPRESENTATION( big_int40_t );
    VERIFY_VALUE_AND_OPS( big_int40_t, int_least64_t,  0x7ffffffffeLL );
    VERIFY_VALUE_AND_OPS( big_int40_t, int_least64_t, -0x8000000000LL );

    VERIFY_BIG_REPRESENTATION( big_int48_t );
    VERIFY_VALUE_AND_OPS( big_int48_t, int_least64_t,  0x7ffffffffffeLL );
    VERIFY_VALUE_AND_OPS( big_int48_t, int_least64_t, -0x800000000000LL );

    VERIFY_BIG_REPRESENTATION( big_int56_t );
    VERIFY_VALUE_AND_OPS( big_int56_t, int_least64_t,  0x7ffffffffffffeLL );
    VERIFY_VALUE_AND_OPS( big_int56_t, int_least64_t, -0x80000000000000LL );

    VERIFY_BIG_REPRESENTATION( big_int64_t );
    VERIFY_VALUE_AND_OPS( big_int64_t, int_least64_t,  0x7ffffffffffffffeLL );
    VERIFY_VALUE_AND_OPS( big_int64_t, int_least64_t, -0x7fffffffffffffffLL-1 );

    VERIFY_BIG_REPRESENTATION( big_uint8_t );
    VERIFY_VALUE_AND_OPS( big_uint8_t, uint_least8_t,  0xff );

    VERIFY_BIG_REPRESENTATION( big_uint16_t );
    VERIFY_VALUE_AND_OPS( big_uint16_t, uint_least16_t, 0xffff );

    VERIFY_BIG_REPRESENTATION( big_uint24_t );
    VERIFY_VALUE_AND_OPS( big_uint24_t, uint_least32_t, 0xffffff );

    VERIFY_BIG_REPRESENTATION( big_uint32_t );
    VERIFY_VALUE_AND_OPS( big_uint32_t, uint_least32_t, 0xffffffff );

    VERIFY_BIG_REPRESENTATION( big_uint40_t );
    VERIFY_VALUE_AND_OPS( big_uint40_t, uint_least64_t, 0xffffffffffLL );

    VERIFY_BIG_REPRESENTATION( big_uint48_t );
    VERIFY_VALUE_AND_OPS( big_uint48_t, uint_least64_t, 0xffffffffffffLL );

    VERIFY_BIG_REPRESENTATION( big_uint56_t );
    VERIFY_VALUE_AND_OPS( big_uint56_t, uint_least64_t, 0xffffffffffffffLL );

    VERIFY_BIG_REPRESENTATION( big_uint64_t );
    VERIFY_VALUE_AND_OPS( big_uint64_t, uint_least64_t, 0xffffffffffffffffULL );

    VERIFY_LITTLE_REPRESENTATION( little_int8_t );
    VERIFY_VALUE_AND_OPS( little_int8_t, int_least8_t,   0x7e );
    VERIFY_VALUE_AND_OPS( little_int8_t, int_least8_t,  -0x80 );

    VERIFY_LITTLE_REPRESENTATION( little_int16_t );
    VERIFY_VALUE_AND_OPS( little_int16_t, int_least16_t,  0x7ffe );
    VERIFY_VALUE_AND_OPS( little_int16_t, int_least16_t, -0x8000 );

    VERIFY_LITTLE_REPRESENTATION( little_int24_t );
    VERIFY_VALUE_AND_OPS( little_int24_t, int_least32_t,  0x7ffffe );
    VERIFY_VALUE_AND_OPS( little_int24_t, int_least32_t, -0x800000 );

    VERIFY_LITTLE_REPRESENTATION( little_int32_t );
    VERIFY_VALUE_AND_OPS( little_int32_t, int_least32_t,  0x7ffffffe );
    VERIFY_VALUE_AND_OPS( little_int32_t, int_least32_t, -0x7fffffff-1 );

    VERIFY_LITTLE_REPRESENTATION( little_int40_t );
    VERIFY_VALUE_AND_OPS( little_int40_t, int_least64_t,  0x7ffffffffeLL );
    VERIFY_VALUE_AND_OPS( little_int40_t, int_least64_t, -0x8000000000LL );

    VERIFY_LITTLE_REPRESENTATION( little_int48_t );
    VERIFY_VALUE_AND_OPS( little_int48_t, int_least64_t,  0x7ffffffffffeLL );
    VERIFY_VALUE_AND_OPS( little_int48_t, int_least64_t, -0x800000000000LL );

    VERIFY_LITTLE_REPRESENTATION( little_int56_t );
    VERIFY_VALUE_AND_OPS( little_int56_t, int_least64_t,  0x7ffffffffffffeLL );
    VERIFY_VALUE_AND_OPS( little_int56_t, int_least64_t, -0x80000000000000LL );

    VERIFY_LITTLE_REPRESENTATION( little_int64_t );
    VERIFY_VALUE_AND_OPS( little_int64_t, int_least64_t,  0x7ffffffffffffffeLL );
    VERIFY_VALUE_AND_OPS( little_int64_t, int_least64_t, -0x7fffffffffffffffLL-1 );

    VERIFY_LITTLE_REPRESENTATION( little_uint8_t );
    VERIFY_VALUE_AND_OPS( little_uint8_t, uint_least8_t, 0xff );

    VERIFY_LITTLE_REPRESENTATION( little_uint16_t );
    VERIFY_VALUE_AND_OPS( little_uint16_t, uint_least16_t, 0xffff );

    VERIFY_LITTLE_REPRESENTATION( little_uint24_t );
    VERIFY_VALUE_AND_OPS( little_uint24_t, uint_least32_t, 0xffffff );

    VERIFY_LITTLE_REPRESENTATION( little_uint32_t );
    VERIFY_VALUE_AND_OPS( little_uint32_t, uint_least32_t, 0xffffffff );

    VERIFY_LITTLE_REPRESENTATION( little_uint40_t );
    VERIFY_VALUE_AND_OPS( little_uint40_t, uint_least64_t, 0xffffffffffLL );

    VERIFY_LITTLE_REPRESENTATION( little_uint48_t );
    VERIFY_VALUE_AND_OPS( little_uint48_t, uint_least64_t, 0xffffffffffffLL );

    VERIFY_LITTLE_REPRESENTATION( little_uint56_t );
    VERIFY_VALUE_AND_OPS( little_uint56_t, uint_least64_t, 0xffffffffffffffLL );

    VERIFY_LITTLE_REPRESENTATION( little_uint64_t );
    VERIFY_VALUE_AND_OPS( little_uint64_t, uint_least64_t, 0xffffffffffffffffULL );

    VERIFY_NATIVE_REPRESENTATION( native_int8_t );
    VERIFY_VALUE_AND_OPS( native_int8_t, int_least8_t,   0x7e );
    VERIFY_VALUE_AND_OPS( native_int8_t, int_least8_t,  -0x80 );

    VERIFY_NATIVE_REPRESENTATION( native_int16_t );
    VERIFY_VALUE_AND_OPS( native_int16_t, int_least16_t,  0x7ffe );
    VERIFY_VALUE_AND_OPS( native_int16_t, int_least16_t, -0x8000 );

    VERIFY_NATIVE_REPRESENTATION( native_int24_t );
    VERIFY_VALUE_AND_OPS( native_int24_t, int_least32_t,  0x7ffffe );
    VERIFY_VALUE_AND_OPS( native_int24_t, int_least32_t, -0x800000 );

    VERIFY_NATIVE_REPRESENTATION( native_int32_t );
    VERIFY_VALUE_AND_OPS( native_int32_t, int_least32_t,  0x7ffffffe );
    VERIFY_VALUE_AND_OPS( native_int32_t, int_least32_t, -0x7fffffff-1 );

    VERIFY_NATIVE_REPRESENTATION( native_int40_t );
    VERIFY_VALUE_AND_OPS( native_int40_t, int_least64_t,  0x7ffffffffeLL );
    VERIFY_VALUE_AND_OPS( native_int40_t, int_least64_t, -0x8000000000LL );

    VERIFY_NATIVE_REPRESENTATION( native_int48_t );
    VERIFY_VALUE_AND_OPS( native_int48_t, int_least64_t,  0x7ffffffffffeLL );
    VERIFY_VALUE_AND_OPS( native_int48_t, int_least64_t, -0x800000000000LL );

    VERIFY_NATIVE_REPRESENTATION( native_int56_t );
    VERIFY_VALUE_AND_OPS( native_int56_t, int_least64_t,  0x7ffffffffffffeLL );
    VERIFY_VALUE_AND_OPS( native_int56_t, int_least64_t, -0x80000000000000LL );

    VERIFY_NATIVE_REPRESENTATION( native_int64_t );
    VERIFY_VALUE_AND_OPS( native_int64_t, int_least64_t,  0x7ffffffffffffffeLL );
    VERIFY_VALUE_AND_OPS( native_int64_t, int_least64_t, -0x7fffffffffffffffLL-1 );

    VERIFY_NATIVE_REPRESENTATION( native_uint8_t );
    VERIFY_VALUE_AND_OPS( native_uint8_t, uint_least8_t, 0xff );

    VERIFY_NATIVE_REPRESENTATION( native_uint16_t );
    VERIFY_VALUE_AND_OPS( native_uint16_t, uint_least16_t, 0xffff );

    VERIFY_NATIVE_REPRESENTATION( native_uint24_t );
    VERIFY_VALUE_AND_OPS( native_uint24_t, uint_least32_t, 0xffffff );

    VERIFY_NATIVE_REPRESENTATION( native_uint32_t );
    VERIFY_VALUE_AND_OPS( native_uint32_t, uint_least32_t, 0xffffffff );

    VERIFY_NATIVE_REPRESENTATION( native_uint40_t );
    VERIFY_VALUE_AND_OPS( native_uint40_t, uint_least64_t, 0xffffffffffLL );

    VERIFY_NATIVE_REPRESENTATION( native_uint48_t );
    VERIFY_VALUE_AND_OPS( native_uint48_t, uint_least64_t, 0xffffffffffffLL );

    VERIFY_NATIVE_REPRESENTATION( native_uint56_t );
    VERIFY_VALUE_AND_OPS( native_uint56_t, uint_least64_t, 0xffffffffffffffLL );

    VERIFY_NATIVE_REPRESENTATION( native_uint64_t );
    VERIFY_VALUE_AND_OPS( native_uint64_t, uint_least64_t, 0xffffffffffffffffULL );

    // aligned integer types
    VERIFY_BIG_REPRESENTATION( big_int16_at );
    VERIFY_VALUE_AND_OPS( big_int16_at, int_least16_t,  0x7ffe );
    VERIFY_VALUE_AND_OPS( big_int16_at, int_least16_t, -0x8000 );

    VERIFY_BIG_REPRESENTATION( big_int32_at );
    VERIFY_VALUE_AND_OPS( big_int32_at, int_least32_t,  0x7ffffffe );
    VERIFY_VALUE_AND_OPS( big_int32_at, int_least32_t, -0x7fffffff-1 );

    VERIFY_BIG_REPRESENTATION( big_int64_at );
    VERIFY_VALUE_AND_OPS( big_int64_at, int_least64_t,  0x7ffffffffffffffeLL );
    VERIFY_VALUE_AND_OPS( big_int64_at, int_least64_t, -0x7fffffffffffffffLL-1 );

    VERIFY_BIG_REPRESENTATION( big_uint16_at );
    VERIFY_VALUE_AND_OPS( big_uint16_at, uint_least16_t, 0xffff );

    VERIFY_BIG_REPRESENTATION( big_uint32_at );
    VERIFY_VALUE_AND_OPS( big_uint32_at, uint_least32_t, 0xffffffff );

    VERIFY_BIG_REPRESENTATION( big_uint64_at );
    VERIFY_VALUE_AND_OPS( big_uint64_at, uint_least64_t, 0xffffffffffffffffULL );

    VERIFY_LITTLE_REPRESENTATION( little_int16_at );
    VERIFY_VALUE_AND_OPS( little_int16_at, int_least16_t,  0x7ffe );
    VERIFY_VALUE_AND_OPS( little_int16_at, int_least16_t, -0x8000 );

    VERIFY_LITTLE_REPRESENTATION( little_int32_at );
    VERIFY_VALUE_AND_OPS( little_int32_at, int_least32_t,  0x7ffffffe );
    VERIFY_VALUE_AND_OPS( little_int32_at, int_least32_t, -0x7fffffff-1 );

    VERIFY_LITTLE_REPRESENTATION( little_int64_at );
    VERIFY_VALUE_AND_OPS( little_int64_at, int_least64_t,  0x7ffffffffffffffeLL );
    VERIFY_VALUE_AND_OPS( little_int64_at, int_least64_t, -0x7fffffffffffffffLL-1 );

    VERIFY_LITTLE_REPRESENTATION( little_uint16_at );
    VERIFY_VALUE_AND_OPS( little_uint16_at, uint_least16_t, 0xffff );

    VERIFY_LITTLE_REPRESENTATION( little_uint32_at );
    VERIFY_VALUE_AND_OPS( little_uint32_at, uint_least32_t, 0xffffffff );

    VERIFY_LITTLE_REPRESENTATION( little_uint64_at );
    VERIFY_VALUE_AND_OPS( little_uint64_at, uint_least64_t, 0xffffffffffffffffULL );

  } // check_representation_and_range

/*

  class MyInt
  {
    int32_t mx;
  public:
    MyInt(int32_t x = 0) : mx(x) {}
    operator int32_t() const {return mx;}

    //friend int32_t operator+(const MyInt& x) {return x;}
  };

  void check_udt()
  {
    typedef boost::endian::endian_arithmetic< order::big, MyInt, 32 >  mybig_int32_ut;

    mybig_int32_ut v(10);
    cout << "+v is " << +v << endl;
    v += 1;
    cout << "v is " << +v << endl;
    v -= 2;
    cout << "v is " << +v << endl;
    v *= 2;
    cout << "v is " << +v << endl;
    ++v;
    cout << "v is " << +v << endl;
    --v;
    cout << "v is " << +v << endl;
//    cout << "v+v is " << +(v+v) << endl;
  }

  void check_udt_le()
  {
    typedef boost::endian::endian_arithmetic< order::little, MyInt, 32 >  mylittle_int32_ut;

    mylittle_int32_ut v(10);
    cout << "+v is " << +v << endl;
    v += 1;
    cout << "v is " << +v << endl;
    v -= 2;
    cout << "v is " << +v << endl;
    v *= 2;
    cout << "v is " << +v << endl;
    ++v;
    cout << "v is " << +v << endl;
    --v;
    cout << "v is " << +v << endl;
//    cout << "v+v is " << +(v+v) << endl;
  }

*/

  long iterations = 10000;

  template< class Endian >
  Endian timing_test( const char * s)
  {
    cout << s << " timing test, " << iterations << " iterations: ";
//    progress_timer t;

    Endian v = 1;
    for ( long i = 0; i < iterations; ++i )
    {
      v += 1;
      v *= 3;
      ++v;
      v *= i;
      if ( i == 0 ) VERIFY_VALUE_AND_OPS( Endian, typename Endian::value_type, 21 );
    }
    return v;
  }

} // unnamed namespace

//  main  ------------------------------------------------------------------------------//

int cpp_main( int argc, char * argv[] )
{
  cout << "Usage: "
       << argv[0] << " [#],\n where # specifies iteration count\n"
          " default iteration count is " << iterations << endl;

  if ( argc > 1 )
    iterations = atol( argv[1] );
  if ( iterations < 1 ) iterations = 1;

  detect_order();
  check_size();
  check_alignment();
  check_representation_and_range_and_ops();
  check_data();
  //check_udt();
  //check_udt_le();

  //timing_test<big_int32_t> ( "big_int32_t" );
  //timing_test<big_int32_at>( "big_int32_at" );
  //timing_test<little_int32_t> ( "little_int32_t" );
  //timing_test<little_int32_at>( "little_int32_at" );

  cout << "\n" << err_count << " errors detected\nTest "
       << (err_count==0 ? "passed\n\n" : "failed\n\n");

  return err_count ? 1 : 0;
} // main
