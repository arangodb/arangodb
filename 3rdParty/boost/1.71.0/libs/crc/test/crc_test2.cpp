//  Boost CRC unit test program file  ----------------------------------------//

//  Copyright 2011 Daryle Walker.
//  Distributed under the Boost Software License, Version 1.0.  (See the
//  accompanying file LICENSE_1_0.txt or a copy at
//  <http://www.boost.org/LICENSE_1_0.txt>.)

//  See <http://www.boost.org/libs/crc/> for the library's home page.

#include <boost/core/lightweight_test.hpp>
#include <boost/crc.hpp>   // for boost::crc_basic,crc_optimal,augmented_crc,crc

#include <boost/cstdint.hpp>         // for boost::uint16_t, uint32_t, uintmax_t
#include <boost/predef/other/endian.h>
#include <boost/integer.hpp>                                // for boost::uint_t
#include <boost/typeof/typeof.hpp>                             // for BOOST_AUTO
#include <boost/random/linear_congruential.hpp>        // for boost::minstd_rand

#include <algorithm>  // for std::generate_n, for_each
#include <climits>    // for CHAR_BIT
#include <cstddef>    // for std::size_t

// Sanity check
#if CHAR_BIT != 8
#error The expected results assume octet-sized bytes.
#endif

// Control tests at compile-time
#ifndef CONTROL_SUB_BYTE_MISMATCHED_REFLECTION_TEST
#define CONTROL_SUB_BYTE_MISMATCHED_REFLECTION_TEST  1
#endif


// Common definitions  -------------------------------------------------------//

namespace {

// Many CRC configurations use the string "123456789" in ASCII as test data.
unsigned char const  std_data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                      0x38, 0x39 };
std::size_t const    std_data_len = sizeof( std_data ) / sizeof( std_data[0] );

// Checksums of the standard test data for common configurations
boost::uint16_t const  std_crc_ccitt_false_result = 0x29B1u;
boost::uint16_t const  std_crc_ccitt_true_result = 0x2189u;
boost::uint16_t const  std_crc_xmodem_result = 0x31C3u;
boost::uint16_t const  std_crc_16_result = 0xBB3Du;
boost::uint32_t const  std_crc_32_result = 0xCBF43926ul;

// Conversion functions between native- and big-endian representations
#if BOOST_ENDIAN_BIG_BYTE
boost::uint32_t  native_to_big( boost::uint32_t x )  { return x; }
boost::uint32_t  big_to_native( boost::uint32_t x )  { return x; }
#else
union endian_convert
{
    boost::uint32_t  w;
    unsigned char    p[ 4 ];
};

boost::uint32_t  native_to_big( boost::uint32_t x )
{
    endian_convert  e;

    e.p[ 0 ] = x >> 24;
    e.p[ 1 ] = x >> 16;
    e.p[ 2 ] = x >> 8;
    e.p[ 3 ] = x;
    return e.w;
}

boost::uint32_t  big_to_native( boost::uint32_t x )
{
    endian_convert  e;

    e.w = x;
    x = e.p[ 0 ];
    x <<= 8;
    x |= e.p[ 1 ];
    x <<= 8;
    x |= e.p[ 2 ];
    x <<= 8;
    x |= e.p[ 3 ];
    return x;
}
#endif

// Define CRC parameters inside traits classes.  Probably will use this in a
// future version of the CRC libray!
template < std::size_t Bits >
class my_crc_rt_traits
{
public:
    typedef boost::integral_constant<std::size_t, Bits>            register_length_c;
    typedef typename boost::uint_t<Bits>::fast  register_type;
    typedef boost::crc_basic<Bits>              computer_type;

    register_type  divisor_polynominal;
    register_type  initial_remainder;
    bool           reflect_input_byte;
    bool           reflect_output_remainder;
    register_type  final_xor_mask;

    computer_type  make_crc_basic() const
    { return computer_type(divisor_polynominal, initial_remainder,
     final_xor_mask, reflect_input_byte, reflect_output_remainder); }
};

template < std::size_t Bits, boost::uintmax_t DivisorPolynominal,
           boost::uintmax_t InitialRemainder, bool ReflectInputBytes,
           bool ReflectOutputRemainder, boost::uintmax_t FinalXorMask >
class my_crc_ct_traits
{
public:
    typedef my_crc_rt_traits<Bits>                           rt_adaptor_type;
    typedef typename rt_adaptor_type::register_type            register_type;
    typedef boost::crc_optimal<Bits, DivisorPolynominal, InitialRemainder,
     FinalXorMask, ReflectInputBytes, ReflectOutputRemainder>  computer_type;

    typedef boost::integral_constant<std::size_t, Bits>  register_length_c;
    typedef boost::integral_constant<register_type, DivisorPolynominal>
      divisor_polynominal_c;
    typedef boost::integral_constant<register_type, InitialRemainder>
      initial_remainder_c;
    typedef boost::integral_constant<bool, ReflectInputBytes>  reflect_input_byte_c;
    typedef boost::integral_constant<bool, ReflectOutputRemainder>
      reflect_output_remainder_c;
    typedef boost::integral_constant<register_type, FinalXorMask>
      final_xor_mask_c;

    operator rt_adaptor_type() const
    {
        rt_adaptor_type const  result = { divisor_polynominal_c::value,
         initial_remainder_c::value, reflect_input_byte_c::value,
         reflect_output_remainder_c::value, final_xor_mask_c::value };

        return result;
    }

    static  computer_type  make_crc_optimal()
    { return boost::crc_optimal<register_length_c::value,
     divisor_polynominal_c::value, initial_remainder_c::value,
     final_xor_mask_c::value, reflect_input_byte_c::value,
     reflect_output_remainder_c::value>(); }
};

template < std::size_t Bits, boost::uintmax_t DivisorPolynominal,
           boost::uintmax_t InitialRemainder, bool ReflectInputBytes,
           bool ReflectOutputRemainder, boost::uintmax_t FinalXorMask,
           boost::uintmax_t StandardTestDataResult >
class my_crc_test_traits
{
public:
    typedef my_crc_ct_traits<Bits, DivisorPolynominal, InitialRemainder,
     ReflectInputBytes, ReflectOutputRemainder, FinalXorMask>  ct_traits_type;
    typedef my_crc_rt_traits<Bits>  rt_traits_type;

    typedef typename rt_traits_type::register_type  register_type;

    typedef boost::integral_constant<std::size_t, Bits>  register_length_c;
    typedef boost::integral_constant<register_type, DivisorPolynominal>
      divisor_polynominal_c;
    typedef boost::integral_constant<register_type, InitialRemainder>
      initial_remainder_c;
    typedef boost::integral_constant<bool, ReflectInputBytes>  reflect_input_byte_c;
    typedef boost::integral_constant<bool, ReflectOutputRemainder>
      reflect_output_remainder_c;
    typedef boost::integral_constant<register_type, FinalXorMask>
      final_xor_mask_c;
    typedef boost::integral_constant<register_type, StandardTestDataResult>
      standard_test_data_CRC_c;

    typedef typename ct_traits_type::computer_type  computer_ct_type;
    typedef typename rt_traits_type::computer_type  computer_rt_type;

    static  computer_ct_type  make_crc_optimal()
    { return ct_traits_type::make_crc_optimal(); }
    static  computer_rt_type  make_crc_basic()
    { return ct_traits_type().operator rt_traits_type().make_crc_basic(); }
};

// Now make some example CRC profiles
typedef my_crc_test_traits<16u, 0x8005u, 0u, true, true, 0u, std_crc_16_result>
  my_crc_16_traits;
typedef my_crc_test_traits<16u, 0x1021u, 0xFFFFu, false, false, 0u,
 std_crc_ccitt_false_result>  my_crc_ccitt_false_traits;
typedef my_crc_test_traits<16u, 0x1021u, 0u, true, true, 0u,
 std_crc_ccitt_true_result>  my_crc_ccitt_true_traits;
typedef my_crc_test_traits<16u, 0x1021u, 0u, false, false, 0u,
 std_crc_xmodem_result>  my_crc_xmodem_traits;
typedef my_crc_test_traits<32u, 0x04C11DB7ul, 0xFFFFFFFFul, true, true,
 0xFFFFFFFFul, std_crc_32_result>  my_crc_32_traits;

template<class Test>
void run_crc_test_policies()
{
    Test()(my_crc_16_traits());
    Test()(my_crc_ccitt_false_traits());
    Test()(my_crc_ccitt_true_traits());
    Test()(my_crc_xmodem_traits());
    Test()(my_crc_32_traits());
}

// Need to test when ReflectInputBytes and ReflectOutputRemainder differ
// (Grabbed from table at <http://regregex.bbcmicro.net/crc-catalogue.htm>.)
typedef my_crc_test_traits<6u, 0x19u, 0u, true, false, 0u, 0x19u>
  my_crc_6_darc_traits;
typedef my_crc_test_traits<12u, 0x80Fu, 0u, false, true, 0u, 0xDAFu>
  my_crc_12_3gpp_traits;

template<class Test>
void run_crc_extended_test_policies()
{
    Test()(my_crc_16_traits());
    Test()(my_crc_ccitt_false_traits());
    Test()(my_crc_ccitt_true_traits());
    Test()(my_crc_xmodem_traits());
    Test()(my_crc_32_traits());
#if CONTROL_SUB_BYTE_MISMATCHED_REFLECTION_TEST
    Test()(my_crc_6_darc_traits());
#endif
    Test()(my_crc_12_3gpp_traits());
}

// Bit mask constants
template < std::size_t BitIndex >
struct high_bit_mask_c
    : boost::detail::high_bit_mask_c<BitIndex>
{};
template < std::size_t BitCount >
struct low_bits_mask_c
    : boost::detail::low_bits_mask_c<BitCount>
{};

}  // anonymous namespace


// Unit tests  ---------------------------------------------------------------//

struct computation_comparison_test {
template<class CRCPolicy>
void operator()(CRCPolicy)
{
    BOOST_AUTO( crc_f, CRCPolicy::make_crc_optimal() );
    BOOST_AUTO( crc_s, CRCPolicy::make_crc_basic() );
    typename CRCPolicy::register_type const  func_result
     = boost::crc<CRCPolicy::register_length_c::value,
        CRCPolicy::divisor_polynominal_c::value,
        CRCPolicy::initial_remainder_c::value,
        CRCPolicy::final_xor_mask_c::value,
        CRCPolicy::reflect_input_byte_c::value,
        CRCPolicy::reflect_output_remainder_c::value>( std_data, std_data_len );

    crc_f.process_bytes( std_data, std_data_len );
    crc_s.process_bytes( std_data, std_data_len );

    BOOST_TEST_EQ( crc_f.checksum(),
     CRCPolicy::standard_test_data_CRC_c::value );
    BOOST_TEST_EQ( crc_s.checksum(),
     CRCPolicy::standard_test_data_CRC_c::value );
    BOOST_TEST_EQ( CRCPolicy::standard_test_data_CRC_c::value,
     func_result );
}
};

struct accessor_and_split_run_test {
template<class CRCPolicy>
void operator()(CRCPolicy)
{
    typedef typename CRCPolicy::computer_ct_type  optimal_crc_type;
    typedef typename CRCPolicy::computer_rt_type    basic_crc_type;

    // Test accessors
    optimal_crc_type  faster_crc1;
    basic_crc_type    slower_crc1( faster_crc1.get_truncated_polynominal(),
     faster_crc1.get_initial_remainder(), faster_crc1.get_final_xor_value(),
     faster_crc1.get_reflect_input(), faster_crc1.get_reflect_remainder() );

    BOOST_TEST_EQ( faster_crc1.get_interim_remainder(),
     slower_crc1.get_initial_remainder() );

    // Process the first half of the standard data
    std::size_t const  mid_way = std_data_len / 2u;

    faster_crc1.process_bytes( std_data, mid_way );
    slower_crc1.process_bytes( std_data, mid_way );

    BOOST_TEST_EQ( faster_crc1.checksum(), slower_crc1.checksum() );

    // Process the second half of the standard data, testing more accessors
    unsigned char const * const  std_data_end = std_data + std_data_len;
    boost::crc_optimal<optimal_crc_type::bit_count,
     optimal_crc_type::truncated_polynominal,
     optimal_crc_type::initial_remainder, optimal_crc_type::final_xor_value,
     optimal_crc_type::reflect_input, optimal_crc_type::reflect_remainder>
      faster_crc2( faster_crc1.get_interim_remainder() );
    boost::crc_basic<basic_crc_type::bit_count>  slower_crc2(
     slower_crc1.get_truncated_polynominal(),
     slower_crc1.get_interim_remainder(), slower_crc1.get_final_xor_value(),
     slower_crc1.get_reflect_input(), slower_crc1.get_reflect_remainder() );

    faster_crc2.process_block( std_data + mid_way, std_data_end );
    slower_crc2.process_block( std_data + mid_way, std_data_end );

    BOOST_TEST_EQ( slower_crc2.checksum(), faster_crc2.checksum() );
    BOOST_TEST_EQ( faster_crc2.checksum(),
     CRCPolicy::standard_test_data_CRC_c::value );
    BOOST_TEST_EQ( CRCPolicy::standard_test_data_CRC_c::value,
     slower_crc2.checksum() );
}
};

struct reset_and_single_bit_error_test {
template<class CRCPolicy>
void operator()(CRCPolicy)
{
    // A single-bit error in a CRC can be guaranteed to be detected if the
    // modulo-2 polynomial divisor has at least two non-zero coefficients.  The
    // implicit highest coefficient is always one, so that leaves an explicit
    // coefficient, i.e. at least one of the polynomial's bits is set.
    BOOST_TEST( CRCPolicy::divisor_polynominal_c::value &
     low_bits_mask_c<CRCPolicy::register_length_c::value>::value );

    // Create a random block of data
    boost::uint32_t    ran_data[ 256 ];
    std::size_t const  ran_length = sizeof(ran_data) / sizeof(ran_data[0]);

    std::generate_n( ran_data, ran_length, boost::minstd_rand() );

    // Create computers and compute the checksum of the data
    BOOST_AUTO( optimal_tester, CRCPolicy::make_crc_optimal() );
    BOOST_AUTO( basic_tester, CRCPolicy::make_crc_basic() );

    optimal_tester.process_bytes( ran_data, sizeof(ran_data) );
    basic_tester.process_bytes( ran_data, sizeof(ran_data) );

    BOOST_AUTO( const optimal_checksum, optimal_tester.checksum() );
    BOOST_AUTO( const basic_checksum, basic_tester.checksum() );

    BOOST_TEST_EQ( optimal_checksum, basic_checksum );

    // Do the checksum again, while testing the capability to reset the current
    // remainder (to either a default or a given value)
    optimal_tester.reset();
    basic_tester.reset( CRCPolicy::initial_remainder_c::value );

    optimal_tester.process_bytes( ran_data, sizeof(ran_data) );
    basic_tester.process_bytes( ran_data, sizeof(ran_data) );

    BOOST_TEST_EQ( optimal_tester.checksum(), basic_tester.checksum() );
    BOOST_TEST_EQ( optimal_tester.checksum(), optimal_checksum );
    BOOST_TEST_EQ( basic_tester.checksum(), basic_checksum );

    // Introduce a single-bit error
    ran_data[ ran_data[0] % ran_length ] ^= ( 1u << (ran_data[ 1 ] % 32u) );

    // Compute the checksum of the errorenous data, while continuing to test
    // the remainder-resetting methods
    optimal_tester.reset( CRCPolicy::initial_remainder_c::value );
    basic_tester.reset();

    optimal_tester.process_bytes( ran_data, sizeof(ran_data) );
    basic_tester.process_bytes( ran_data, sizeof(ran_data) );

    BOOST_TEST_EQ( basic_tester.checksum(), optimal_tester.checksum() );
    BOOST_TEST_NE( optimal_checksum, optimal_tester.checksum() );
    BOOST_TEST_NE( basic_checksum, basic_tester.checksum() );
}
};

void augmented_crc_test()
{
    using std::size_t;
    using boost::uint32_t;
    using boost::augmented_crc;

    // Common CRC parameters, all others are zero/false
    static  size_t const    bits = 32u;
    static  uint32_t const  poly = 0x04C11DB7ul;

    // Create a random block of data, with space at the end for a CRC
    static  size_t const  data_length = 256u;
    static  size_t const  run_length = data_length + 1u;

    uint32_t      run_data[ run_length ];
    uint32_t &    run_crc = run_data[ data_length ];
    size_t const  data_size = sizeof( run_data ) - sizeof( run_crc );

    std::generate_n( run_data, data_length, boost::minstd_rand() );
    run_crc = 0u;

    // The augmented-CRC routine needs to push an appropriate number of zero
    // bits (the register size) through before the checksum can be extracted.
    // The other CRC methods, which are un-augmented, don't need to do this.
    uint32_t const  checksum = boost::crc<bits, poly, 0u, 0u, false, false>(
     run_data, data_size );

    BOOST_TEST_EQ( (augmented_crc<bits, poly>)(run_data, sizeof( run_data
     )), checksum );

    // Now appending a message's CRC to the message should lead to a zero-value
    // checksum.  Note that the CRC must be read from the largest byte on down,
    // i.e. big-endian!
    run_crc = native_to_big( checksum );
    BOOST_TEST_EQ( (augmented_crc<bits, poly>)(run_data, sizeof( run_data
     )), 0u );

    // Check again with the non-augmented methods
    boost::crc_basic<bits>  crc_b( poly );

    crc_b.process_bytes( run_data, data_size );
    BOOST_TEST_EQ( crc_b.checksum(), checksum );

    // Introduce a single-bit error, now the checksum shouldn't match!
    uint32_t const  affected_word_index = run_data[ 0 ] % data_length;
    uint32_t const  affected_bit_index = run_data[ 1 ] % 32u;
    uint32_t const  affecting_mask = 1ul << affected_bit_index;

    run_data[ affected_word_index ] ^= affecting_mask;

    crc_b.reset();
    crc_b.process_bytes( run_data, data_size );
    BOOST_TEST_NE( crc_b.checksum(), checksum );

    BOOST_TEST_NE( (augmented_crc<bits, poly>)(run_data, sizeof( run_data )),
     0u );

    run_crc = 0u;
    BOOST_TEST_NE( (augmented_crc<bits, poly>)(run_data, sizeof( run_data )),
     checksum );

    // Now introduce the single error in the checksum instead
    run_data[ affected_word_index ] ^= affecting_mask;
    run_crc = native_to_big( checksum ) ^ affecting_mask;
    BOOST_TEST_NE( (augmented_crc<bits, poly>)(run_data, sizeof( run_data )),
     0u );

    // Repeat these tests with a non-zero initial remainder.  Before we can
    // check the results against a non-augmented CRC computer, realize that they
    // interpret the inital remainder differently.  However, the two standards
    // can convert between each other.
    // (checksum2 initial value is as a scratch pad.  So are the address and new
    //  value of run_crc, but it's also useful for the next sub-step.)
    // (TODO: getting the equivalent unaugmented-CRC initial-remainder given an
    //  augmented-CRC initial-remainder is done by putting said augmented-CRC
    //  initial-remainder through the augmented-CRC computation with a
    //  zero-value message.  I don't know how to go the other way, yet.)
    run_crc = 0u;
    uint32_t        checksum2 = run_data[ run_data[2] % data_length ];
    uint32_t const  initial_residue = checksum2 + !checksum2;  // ensure nonzero
    uint32_t const  initial_residue_unaugmented = augmented_crc<bits, poly>(
                     &run_crc, sizeof(run_crc), initial_residue );

    BOOST_TEST_NE( initial_residue, 0u );
    crc_b.reset( initial_residue_unaugmented );
    crc_b.process_bytes( run_data, data_size );
    checksum2 = crc_b.checksum();

    BOOST_TEST_EQ( run_crc, 0u );
    BOOST_TEST_EQ( (augmented_crc<bits, poly>)(run_data, sizeof( run_data ),
     initial_residue), checksum2 );
    run_crc = native_to_big( checksum2 );
    BOOST_TEST_EQ( (augmented_crc<bits, poly>)(run_data, sizeof( run_data ),
     initial_residue), 0u );

    // Use the inital remainder argument to split a CRC-computing run
    size_t const    split_index = data_length / 2u;
    uint32_t const  intermediate = augmented_crc<bits, poly>( run_data,
                     sizeof(run_crc) * split_index, initial_residue );

    BOOST_TEST_EQ( (augmented_crc<bits, poly>)(&run_data[ split_index ],
     sizeof( run_data ) - sizeof( run_crc ) * split_index, intermediate), 0u );
    run_crc = 0u;
    BOOST_TEST_EQ( (augmented_crc<bits, poly>)(&run_data[ split_index ],
     sizeof( run_data ) - sizeof( run_crc ) * split_index, intermediate),
     checksum2 );

    // Repeat the single-bit error test, with a non-zero initial-remainder
    run_data[ run_data[3] % data_length ] ^= ( 1ul << (run_data[4] % 32u) );
    run_crc = native_to_big( checksum2 );
    BOOST_TEST_NE( (augmented_crc<bits, poly>)(run_data, sizeof( run_data ),
     initial_residue), 0u );
}
    
// Optimal computer, via the single-run function
unsigned crc_f1( const void * buffer, std::size_t byte_count )
{
    return boost::crc<3u, 0x03u, 0u, 0u, false, false>( buffer, byte_count );
}

void sub_nybble_polynominal_test()
{
    // The CRC standard is a SDH/SONET Low Order LCAS control word with CRC-3
    // taken from ITU-T G.707 (12/03) XIII.2.

    // Four samples, each four bytes; should all have a CRC of zero
    unsigned char const  samples[4][4]
      = {
            { 0x3Au, 0xC4u, 0x08u, 0x06u },
            { 0x42u, 0xC5u, 0x0Au, 0x41u },
            { 0x4Au, 0xC5u, 0x08u, 0x22u },
            { 0x52u, 0xC4u, 0x08u, 0x05u }
        };

    // Basic computer
    boost::crc_basic<3u>  crc_1( 0x03u );

    crc_1.process_bytes( samples[0], 4u );
    BOOST_TEST_EQ( crc_1.checksum(), 0u );

    crc_1.reset();
    crc_1.process_bytes( samples[1], 4u );
    BOOST_TEST_EQ( crc_1.checksum(), 0u );

    crc_1.reset();
    crc_1.process_bytes( samples[2], 4u );
    BOOST_TEST_EQ( crc_1.checksum(), 0u );

    crc_1.reset();
    crc_1.process_bytes( samples[3], 4u );
    BOOST_TEST_EQ( crc_1.checksum(), 0u );

    BOOST_TEST_EQ( crc_f1(samples[ 0 ], 4u), 0u );
    BOOST_TEST_EQ( crc_f1(samples[ 1 ], 4u), 0u );
    BOOST_TEST_EQ( crc_f1(samples[ 2 ], 4u), 0u );
    BOOST_TEST_EQ( crc_f1(samples[ 3 ], 4u), 0u );

    // TODO: do similar tests with boost::augmented_crc<3, 0x03>
    // (Now I think that this can't be done right now, since that function reads
    // byte-wise, so the register size needs to be a multiple of CHAR_BIT.)
}

// Optimal computer, via the single-run function
unsigned crc_f2( const void * buffer, std::size_t byte_count )
{
    return boost::crc<7u, 0x09u, 0u, 0u, false, false>( buffer, byte_count );
}

void sub_octet_polynominal_test()
{
    // The CRC standard is a SDH/SONET J0/J1/J2/N1/N2/TR TTI (trace message)
    // with CRC-7, o.a. ITU-T G.707 Annex B, G.832 Annex A.

    // Two samples, each sixteen bytes
    // Sample 1 is '\x80' + ASCII("123456789ABCDEF")
    // Sample 2 is '\x80' + ASCII("TTI UNAVAILABLE")
    unsigned char const  samples[2][16]
      = {
            { 0x80u, 0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x38u,
              0x39u, 0x41u, 0x42u, 0x43u, 0x44u, 0x45u, 0x46u },
            { 0x80u, 0x54u, 0x54u, 0x49u, 0x20u, 0x55u, 0x4Eu, 0x41u, 0x56u,
              0x41u, 0x49u, 0x4Cu, 0x41u, 0x42u, 0x4Cu, 0x45u }
        };
    unsigned const       results[2] = { 0x62u, 0x23u };

    // Basic computer
    boost::crc_basic<7u>  crc_1( 0x09u );

    crc_1.process_bytes( samples[0], 16u );
    BOOST_TEST_EQ( crc_1.checksum(), results[0] );

    crc_1.reset();
    crc_1.process_bytes( samples[1], 16u );
    BOOST_TEST_EQ( crc_1.checksum(), results[1] );

    BOOST_TEST_EQ( crc_f2(samples[ 0 ], 16u), results[0] );
    BOOST_TEST_EQ( crc_f2(samples[ 1 ], 16u), results[1] );

    // TODO: do similar tests with boost::augmented_crc<7, 0x09>
    // (Now I think that this can't be done right now, since that function reads
    // byte-wise, so the register size needs to be a multiple of CHAR_BIT.)
}

void one_bit_polynominal_test()
{
    // Try a CRC based on the (x + 1) polynominal, which is a factor in
    // many real-life polynominals and doesn't fit evenly in a byte.
    boost::crc_basic<1u>  crc_1( 1u );

    crc_1.process_bytes( std_data, std_data_len );
    BOOST_TEST_EQ( crc_1.checksum(), 1u );

    // Do it again, but using crc_optimal.  The real purpose of this is to test
    // crc_optimal::process_byte, which doesn't get exercised anywhere else in
    // this file (directly or indirectly).
    boost::crc_optimal<1u, 1u, 0u, 0u, false, false>  crc_2;

    for ( std::size_t  i = 0u ; i < std_data_len ; ++i )
        crc_2.process_byte( std_data[i] );
    BOOST_TEST_EQ( crc_2.checksum(), 1u );
}

struct function_object_test {
template<class CRCPolicy>
void operator()(CRCPolicy)
{
    typename CRCPolicy::computer_ct_type  crc_c;

    crc_c = std::for_each( std_data, std_data + std_data_len, crc_c );
    BOOST_TEST_EQ( crc_c(), CRCPolicy::standard_test_data_CRC_c::value );
}
};

// Ticket #2492: crc_optimal with reversed CRC16
// <https://svn.boost.org/trac/boost/ticket/2492>
void issue_2492_test()
{
    // I'm trusting that the original bug reporter got his/her calculations
    // correct.
    boost::uint16_t const  expected_result = 0xF990u;

    boost::crc_optimal<16, 0x100Bu, 0xFFFFu, 0x0000, true, false>  boost_crc_1;
    boost::crc_basic<16>  boost_crc_2( 0x100Bu, 0xFFFFu, 0x0000, true, false );

    // This should be right...
    boost_crc_1.process_byte( 0u );
    BOOST_TEST_EQ( boost_crc_1.checksum(), expected_result );

    // ...but the reporter said this didn't reflect, giving 0x099F as the
    // (wrong) result.  However, I get the right answer!
    boost_crc_2.process_byte( 0u );
    BOOST_TEST_EQ( boost_crc_2.checksum(), expected_result );
}

int main()
{
    run_crc_extended_test_policies<computation_comparison_test>();
    run_crc_test_policies<accessor_and_split_run_test>();
    run_crc_test_policies<reset_and_single_bit_error_test>();
    augmented_crc_test();
    sub_nybble_polynominal_test();
    sub_octet_polynominal_test();
    one_bit_polynominal_test();
    run_crc_test_policies<function_object_test>();
    issue_2492_test();
    return boost::report_errors();
}
