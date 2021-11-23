// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/arithmetic.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>
#include <cstddef>

template<class U> void test()
{
    {
        U u( 0 );

        unsigned char * p1 = u.data();
        void * p2 = &u;

        BOOST_TEST_EQ( p1, p2 );
    }

    {
        U const u( 0 );

        unsigned char const * p1 = u.data();
        void const * p2 = &u;

        BOOST_TEST_EQ( p1, p2 );
    }
}

template<class T, std::size_t Bits> void test_unaligned()
{
    using namespace boost::endian;

    test< endian_buffer<order::big, T, Bits, align::no> >();
    test< endian_buffer<order::little, T, Bits, align::no> >();
    test< endian_buffer<order::native, T, Bits, align::no> >();

    test< endian_arithmetic<order::big, T, Bits, align::no> >();
    test< endian_arithmetic<order::little, T, Bits, align::no> >();
    test< endian_arithmetic<order::native, T, Bits, align::no> >();
}

template<class T, std::size_t Bits> void test_aligned()
{
    using namespace boost::endian;

    test< endian_buffer<order::big, T, Bits, align::yes> >();
    test< endian_buffer<order::little, T, Bits, align::yes> >();

    test< endian_arithmetic<order::big, T, Bits, align::yes> >();
    test< endian_arithmetic<order::little, T, Bits, align::yes> >();
}

int main()
{
    test_unaligned<boost::int_least8_t, 8>();
    test_unaligned<boost::int_least16_t, 16>();
    test_unaligned<boost::int_least32_t, 24>();
    test_unaligned<boost::int_least32_t, 32>();
    test_unaligned<boost::int_least64_t, 40>();
    test_unaligned<boost::int_least64_t, 48>();
    test_unaligned<boost::int_least64_t, 56>();
    test_unaligned<boost::int_least64_t, 64>();

    test_unaligned<boost::uint_least8_t, 8>();
    test_unaligned<boost::uint_least16_t, 16>();
    test_unaligned<boost::uint_least32_t, 24>();
    test_unaligned<boost::uint_least32_t, 32>();
    test_unaligned<boost::uint_least64_t, 40>();
    test_unaligned<boost::uint_least64_t, 48>();
    test_unaligned<boost::uint_least64_t, 56>();
    test_unaligned<boost::uint_least64_t, 64>();

    test_unaligned<float, 32>();
    test_unaligned<double, 64>();

    test_aligned<boost::int8_t, 8>();
    test_aligned<boost::int16_t, 16>();
    test_aligned<boost::int32_t, 32>();
    test_aligned<boost::int64_t, 64>();

    test_aligned<boost::uint8_t, 8>();
    test_aligned<boost::uint16_t, 16>();
    test_aligned<boost::uint32_t, 32>();
    test_aligned<boost::uint64_t, 64>();

    test_aligned<float, 32>();
    test_aligned<double, 64>();

    return boost::report_errors();
}
