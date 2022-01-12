// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/config/pragma_message.hpp>

#if defined(__GNUC__) && !defined(__clang__) && __cplusplus < 201100L

BOOST_PRAGMA_MESSAGE("Skipping test under GCC in C++98 mode")
int main() {}

#else

#if defined(__clang__)
# pragma clang diagnostic ignored "-Wlong-long"
#endif

#include <boost/container_hash/hash.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>
#include <vector>
#include <list>
#include <utility>
#include <complex>
#include <limits>
#include <climits>
#include <cfloat>
#include <cstddef>

// This test checks whether hash values have changed

template<class T> std::size_t hv( T const& t )
{
    return boost::hash<T>()( t );
}

int main()
{
    // char
    BOOST_TEST_EQ( hv('\x00'), 0 );
    BOOST_TEST_EQ( hv('A'), 'A' );
    BOOST_TEST_EQ( hv('\x7F'), 0x7F );

    // signed char
    BOOST_TEST_EQ( hv((signed char)0), 0 );
    BOOST_TEST_EQ( hv((signed char)+1), +1 );
    BOOST_TEST_EQ( hv((signed char)-1), (std::size_t)-1 );
    BOOST_TEST_EQ( hv((signed char)+127), 127 );
    BOOST_TEST_EQ( hv((signed char)-128), (std::size_t)-128 );

    // unsigned char
    BOOST_TEST_EQ( hv((unsigned char)0), 0 );
    BOOST_TEST_EQ( hv((unsigned char)1), 1 );
    BOOST_TEST_EQ( hv((unsigned char)255), 255 );

    // short
    BOOST_TEST_EQ( hv((short)0), 0 );
    BOOST_TEST_EQ( hv((short)+1), 1 );
    BOOST_TEST_EQ( hv((short)-1), (std::size_t)-1 );
    BOOST_TEST_EQ( hv((short)+32767), 32767 );
    BOOST_TEST_EQ( hv((short)-32768), (std::size_t)-32768 );

    // unsigned short
    BOOST_TEST_EQ( hv((unsigned short)0), 0 );
    BOOST_TEST_EQ( hv((unsigned short)1), 1 );
    BOOST_TEST_EQ( hv((unsigned short)65535), 65535 );

    // int
    BOOST_TEST_EQ( hv(0), 0 );
    BOOST_TEST_EQ( hv(+1), 1 );
    BOOST_TEST_EQ( hv(-1), (std::size_t)-1 );
    BOOST_TEST_EQ( hv(+32767), 32767 );
    BOOST_TEST_EQ( hv(-32768), (std::size_t)-32768 );

    // unsigned int
    BOOST_TEST_EQ( hv((unsigned)0), 0 );
    BOOST_TEST_EQ( hv((unsigned)1), 1 );
    BOOST_TEST_EQ( hv((unsigned)65535), 65535 );
    BOOST_TEST_EQ( hv((unsigned)-1), (std::size_t)(unsigned)-1 );

    // long
    BOOST_TEST_EQ( hv(0L), 0 );
    BOOST_TEST_EQ( hv(+1L), 1 );
    BOOST_TEST_EQ( hv(-1L), (std::size_t)-1 );
    BOOST_TEST_EQ( hv(+32767L), 32767 );
    BOOST_TEST_EQ( hv(-32768L), (std::size_t)-32768 );

    // unsigned long
    BOOST_TEST_EQ( hv(0UL), 0 );
    BOOST_TEST_EQ( hv(1UL), 1 );
    BOOST_TEST_EQ( hv(65535UL), 65535 );
    BOOST_TEST_EQ( hv((unsigned long)-1), (std::size_t)(unsigned long)-1 );

    // long long
    BOOST_TEST_EQ( hv(0LL), 0 );
    BOOST_TEST_EQ( hv(+1LL), 1 );
    BOOST_TEST_EQ( hv(-1LL), (std::size_t)-1 );
    BOOST_TEST_EQ( hv(+32767LL), 32767 );
    BOOST_TEST_EQ( hv(-32768LL), (std::size_t)-32768 );

    // unsigned long long
    BOOST_TEST_EQ( hv(0ULL), 0 );
    BOOST_TEST_EQ( hv(1ULL), 1 );
    BOOST_TEST_EQ( hv(65535ULL), 65535 );
#if SIZE_MAX == 4294967295U
    BOOST_TEST_EQ( hv((unsigned long long)-1), 3221225537U );
#else
    BOOST_TEST_EQ( hv((unsigned long long)-1), (std::size_t)-1 );
#endif

#if defined(BOOST_HAS_INT128)

    typedef boost::int128_type int128;

    BOOST_TEST_EQ( hv((int128)0), 0 );
    BOOST_TEST_EQ( hv((int128)1), 1 );
    BOOST_TEST_EQ( hv((int128)-1), (std::size_t)-1 );
    BOOST_TEST_EQ( hv((int128)+32767), 32767 );
    BOOST_TEST_EQ( hv((int128)-32768), (std::size_t)-32768 );

    typedef boost::uint128_type uint128;

    BOOST_TEST_EQ( hv((uint128)0), 0 );
    BOOST_TEST_EQ( hv((uint128)1), 1 );
    BOOST_TEST_EQ( hv((uint128)65535), 65535 );

#if defined(BOOST_GCC) && BOOST_GCC < 100000

    // This looks like some sort of miscompilation.
    // Under CI, both GHA and Appveyor GCCs produce this value.
    // But the exact same test on godbolt.org produces the correct
    // value, below.

    // BOOST_TEST_EQ( hv((uint128)-1), 18446744073709551615ULL );

#else

    BOOST_TEST_EQ( hv((uint128)-1), 13835058055282163777ULL );

#endif

#endif

    // float
    BOOST_TEST_EQ( hv(0.0f), 0 );
    BOOST_TEST_EQ( hv(-0.0f), 0 );
    BOOST_TEST_EQ( hv(1.0f), 1065353216U );
    BOOST_TEST_EQ( hv(-1.0f), 3212836864U );
    BOOST_TEST_EQ( hv(3.14f), 1078523331U );
    BOOST_TEST_EQ( hv(-3.14f), 3226006979U );
    BOOST_TEST_EQ( hv(1e-38f), 7136238U );
    BOOST_TEST_EQ( hv(-1e-38f), 2154619886U );
    BOOST_TEST_EQ( hv(1e+38f), 2123789977U );
    BOOST_TEST_EQ( hv(-1e+38f), 4271273625U );

#if !defined(__GLIBCXX__)

    BOOST_TEST_EQ( hv(std::numeric_limits<float>::infinity()), 2139095040U );
    BOOST_TEST_EQ( hv(-std::numeric_limits<float>::infinity()), 4286578688U );

#elif SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::numeric_limits<float>::infinity()), 4294967295U );
    BOOST_TEST_EQ( hv(-std::numeric_limits<float>::infinity()), 4294967294U );

#else

    BOOST_TEST_EQ( hv(std::numeric_limits<float>::infinity()), 18446744073709551615ULL );
    BOOST_TEST_EQ( hv(-std::numeric_limits<float>::infinity()), 18446744073709551614ULL );

#endif

    // double
    BOOST_TEST_EQ( hv(0.0), 0 );
    BOOST_TEST_EQ( hv(-0.0), 0 );

#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(1.0), 1072693248U );
    BOOST_TEST_EQ( hv(-1.0), 3220176896U );
    BOOST_TEST_EQ( hv(3.14), 2660156064U );
    BOOST_TEST_EQ( hv(-3.14), 512672416U );
    BOOST_TEST_EQ( hv(1e-308), 1553872728U );
    BOOST_TEST_EQ( hv(-1e-308), 3701356376U );
    BOOST_TEST_EQ( hv(1e+308), 2577739707U );
    BOOST_TEST_EQ( hv(-1e+308), 430256059U );

#if !defined(__GLIBCXX__)

    BOOST_TEST_EQ( hv(std::numeric_limits<double>::infinity()), 2146435072U );
    BOOST_TEST_EQ( hv(-std::numeric_limits<double>::infinity()), 4293918720U );

#else

    BOOST_TEST_EQ( hv(std::numeric_limits<double>::infinity()), 4294967295U );
    BOOST_TEST_EQ( hv(-std::numeric_limits<double>::infinity()), 4294967294U );

#endif

#else

    BOOST_TEST_EQ( hv(1.0), 4607182418800017408ULL );
    BOOST_TEST_EQ( hv(-1.0), 13830554455654793216ULL );
    BOOST_TEST_EQ( hv(3.14), 4614253070214989087ULL );
    BOOST_TEST_EQ( hv(-3.14), 13837625107069764895ULL );
    BOOST_TEST_EQ( hv(1e-308), 2024022533073106ULL );
    BOOST_TEST_EQ( hv(-1e-308), 9225396059387848914ULL );
    BOOST_TEST_EQ( hv(1e+308), 9214871658872686752ULL );
    BOOST_TEST_EQ( hv(-1e+308), 18438243695727462560ULL );

#if !defined(__GLIBCXX__)

    BOOST_TEST_EQ( hv(std::numeric_limits<double>::infinity()), 9218868437227405312ULL );
    BOOST_TEST_EQ( hv(-std::numeric_limits<double>::infinity()), 18442240474082181120ULL );

#else

    BOOST_TEST_EQ( hv(std::numeric_limits<double>::infinity()), 18446744073709551615ULL );
    BOOST_TEST_EQ( hv(-std::numeric_limits<double>::infinity()), 18446744073709551614ULL );

#endif

#endif

    // long double
    BOOST_TEST_EQ( hv(0.0L), 0 );
    BOOST_TEST_EQ( hv(-0.0L), 0 );

#if defined(_WIN32) && !defined(__GNUC__) // Under MS ABI, long double == double

#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(1.0L), 1072693248U );
    BOOST_TEST_EQ( hv(-1.0L), 3220176896U );
    BOOST_TEST_EQ( hv(3.14L), 2660156064U );
    BOOST_TEST_EQ( hv(-3.14L), 512672416U );
    BOOST_TEST_EQ( hv(std::numeric_limits<long double>::infinity()), 2146435072U );
    BOOST_TEST_EQ( hv(-std::numeric_limits<long double>::infinity()), 4293918720U );

#else

    BOOST_TEST_EQ( hv(1.0L), 4607182418800017408ULL );
    BOOST_TEST_EQ( hv(-1.0L), 13830554455654793216ULL );
    BOOST_TEST_EQ( hv(3.14L), 4614253070214989087ULL );
    BOOST_TEST_EQ( hv(-3.14L), 13837625107069764895ULL );
    BOOST_TEST_EQ( hv(std::numeric_limits<long double>::infinity()), 9218868437227405312ULL );
    BOOST_TEST_EQ( hv(-std::numeric_limits<long double>::infinity()), 18442240474082181120ULL );

#endif

#else

#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(1.0L), 2684370943U );
    BOOST_TEST_EQ( hv(-1.0L), 2684403711U );
    BOOST_TEST_EQ( hv(3.14L), 83002659U );
    BOOST_TEST_EQ( hv(-3.14L), 82969891U );

#if !defined(__GLIBCXX__)

    BOOST_TEST_EQ( hv(std::numeric_limits<long double>::infinity()), 0xA0007FFFu );
    BOOST_TEST_EQ( hv(-std::numeric_limits<long double>::infinity()), 0xA000FFFFu );

#else

    BOOST_TEST_EQ( hv(std::numeric_limits<long double>::infinity()), 4294967295U );
    BOOST_TEST_EQ( hv(-std::numeric_limits<long double>::infinity()), 4294967294U );

#endif

#else

    BOOST_TEST_EQ( hv(1.0L), 11529215046068486143ULL );
    BOOST_TEST_EQ( hv(-1.0L), 11529215046068518911ULL );
    BOOST_TEST_EQ( hv(3.14L), 12059468778148142067ULL );
    BOOST_TEST_EQ( hv(-3.14L), 12059468778147191795ULL );

#if !defined(__GLIBCXX__)

    BOOST_TEST_EQ( hv(std::numeric_limits<long double>::infinity()), 11529215046068502527ULL );
    BOOST_TEST_EQ( hv(-std::numeric_limits<long double>::infinity()), 11529215046068535295ULL );

#else

    BOOST_TEST_EQ( hv(std::numeric_limits<long double>::infinity()), 18446744073709551615ULL );
    BOOST_TEST_EQ( hv(-std::numeric_limits<long double>::infinity()), 18446744073709551614ULL );

#endif

#endif

#endif

    // C array
    {
        int a1[] = { 0 };
        int a2[] = { 0, 0 };
        int a3[] = { 0, 0, 0 };

#if SIZE_MAX == 4294967295U

        BOOST_TEST_EQ( hv(a1), 3864292196U );
        BOOST_TEST_EQ( hv(a2), 2842917718U );
        BOOST_TEST_EQ( hv(a3), 325752138U );

#else

        BOOST_TEST_EQ( hv(a1), 3864292196ULL );
        BOOST_TEST_EQ( hv(a2), 14642545639667855512ULL );
        BOOST_TEST_EQ( hv(a3), 17867750819888810972ULL );

#endif
    }

    // string
#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::string()), 0 );
    BOOST_TEST_EQ( hv(std::string("abc")), 1849538372U );
    BOOST_TEST_EQ( hv(std::string("\0", 1)), 3864292196U );
    BOOST_TEST_EQ( hv(std::string("\0\0", 2)), 2842917718U );
    BOOST_TEST_EQ( hv(std::string("\0\0\0", 3)), 325752138U );

#else

    BOOST_TEST_EQ( hv(std::string()), 0 );
    BOOST_TEST_EQ( hv(std::string("abc")), 6420922261882292859ULL );
    BOOST_TEST_EQ( hv(std::string("\0", 1)), 3864292196ULL );
    BOOST_TEST_EQ( hv(std::string("\0\0", 2)), 14642545639667855512ULL );
    BOOST_TEST_EQ( hv(std::string("\0\0\0", 3)), 17867750819888810972ULL );

#endif

    // pointer
    BOOST_TEST_EQ( hv((void*)0), 0 );
    BOOST_TEST_EQ( hv((void*)0x200014A0), 603985716U );

    // complex<int>
    BOOST_TEST_EQ( hv(std::complex<int>(0, 0)), 0U );
    BOOST_TEST_EQ( hv(std::complex<int>(+1, 0)), 1U );
    BOOST_TEST_EQ( hv(std::complex<int>(0, +1)), 65U );

#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::complex<int>(-1, 0)), 4294967295U );
    BOOST_TEST_EQ( hv(std::complex<int>(0, -1)), 3221225536U );

#else

    BOOST_TEST_EQ( hv(std::complex<int>(-1, 0)), 18446744073709551615ULL );
    BOOST_TEST_EQ( hv(std::complex<int>(0, -1)), 13835058055282163776ULL );

#endif

    // complex<float>
    BOOST_TEST_EQ( hv(std::complex<float>(0.0f, 0.0f)), 0U );
    BOOST_TEST_EQ( hv(std::complex<float>(+1.0f, 0.0f)), 1065353216U );
    BOOST_TEST_EQ( hv(std::complex<float>(-1.0f, 0.0f)), 3212836864U );

#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::complex<float>(0.0f, +1.0f)), 3495952384U );
    BOOST_TEST_EQ( hv(std::complex<float>(0.0f, -1.0f)), 2959081472U );

#else

    BOOST_TEST_EQ( hv(std::complex<float>(0.0f, +1.0f)), 67920461824ULL );
    BOOST_TEST_EQ( hv(std::complex<float>(0.0f, -1.0f)), 209117511680ULL );

#endif

    // complex<double>
    BOOST_TEST_EQ( hv(std::complex<double>(0.0, 0.0)), 0U );

#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::complex<double>(+1.0, 0.0)), 1072693248U );
    BOOST_TEST_EQ( hv(std::complex<double>(-1.0, 0.0)), 3220176896U );
    BOOST_TEST_EQ( hv(std::complex<double>(0.0, +1.0)), 873201664U );
    BOOST_TEST_EQ( hv(std::complex<double>(0.0, -1.0)), 2483814400U );

#else

    BOOST_TEST_EQ( hv(std::complex<double>(+1.0, 0.0)), 4607182418800017408ULL );
    BOOST_TEST_EQ( hv(std::complex<double>(-1.0, 0.0)), 13830554455654793216ULL );
    BOOST_TEST_EQ( hv(std::complex<double>(0.0, +1.0)), 3750372589692780544ULL );
    BOOST_TEST_EQ( hv(std::complex<double>(0.0, -1.0)), 10667901617333862400ULL );

#endif

    // pair
#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::make_pair(0, 0)), 2842917718U );
    BOOST_TEST_EQ( hv(std::make_pair(1, 2)), 2507434894U );
    BOOST_TEST_EQ( hv(std::make_pair(-1, -2)), 1874100199 );

#else

    BOOST_TEST_EQ( hv(std::make_pair(0, 0)), 14642545639667855512ULL );
    BOOST_TEST_EQ( hv(std::make_pair(1, 2)), 3370697991563800380ULL );
    BOOST_TEST_EQ( hv(std::make_pair(-1, -2)), 4139767141999124554ULL );

#endif

    // vector<char>
#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::vector<char>(0)), 0 );
    BOOST_TEST_EQ( hv(std::vector<char>(1)), 3864292196U );
    BOOST_TEST_EQ( hv(std::vector<char>(2)), 2842917718U );
    BOOST_TEST_EQ( hv(std::vector<char>(3)), 325752138U );

#else

    BOOST_TEST_EQ( hv(std::vector<char>(0)), 0 );
    BOOST_TEST_EQ( hv(std::vector<char>(1)), 3864292196ULL );
    BOOST_TEST_EQ( hv(std::vector<char>(2)), 14642545639667855512ULL );
    BOOST_TEST_EQ( hv(std::vector<char>(3)), 17867750819888810972ULL );

#endif

    // vector<int>
#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::vector<int>(0)), 0 );
    BOOST_TEST_EQ( hv(std::vector<int>(1)), 3864292196U );
    BOOST_TEST_EQ( hv(std::vector<int>(2)), 2842917718U );
    BOOST_TEST_EQ( hv(std::vector<int>(3)), 325752138U );

#else

    BOOST_TEST_EQ( hv(std::vector<int>(0)), 0 );
    BOOST_TEST_EQ( hv(std::vector<int>(1)), 3864292196ULL );
    BOOST_TEST_EQ( hv(std::vector<int>(2)), 14642545639667855512ULL );
    BOOST_TEST_EQ( hv(std::vector<int>(3)), 17867750819888810972ULL );

#endif

    // vector<vector<int>>
#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::vector<std::vector<int> >(0)), 0 );
    BOOST_TEST_EQ( hv(std::vector<std::vector<int> >(1)), 3864292196U );
    BOOST_TEST_EQ( hv(std::vector<std::vector<int> >(2)), 2842917718U );
    BOOST_TEST_EQ( hv(std::vector<std::vector<int> >(3)), 325752138U );

#else

    BOOST_TEST_EQ( hv(std::vector<std::vector<int> >(0)), 0 );
    BOOST_TEST_EQ( hv(std::vector<std::vector<int> >(1)), 3864292196ULL );
    BOOST_TEST_EQ( hv(std::vector<std::vector<int> >(2)), 14642545639667855512ULL );
    BOOST_TEST_EQ( hv(std::vector<std::vector<int> >(3)), 17867750819888810972ULL );

#endif

    // list<char>
#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::list<char>(0)), 0 );
    BOOST_TEST_EQ( hv(std::list<char>(1)), 3864292196U );
    BOOST_TEST_EQ( hv(std::list<char>(2)), 2842917718U );
    BOOST_TEST_EQ( hv(std::list<char>(3)), 325752138U );

#else

    BOOST_TEST_EQ( hv(std::list<char>(0)), 0 );
    BOOST_TEST_EQ( hv(std::list<char>(1)), 3864292196ULL );
    BOOST_TEST_EQ( hv(std::list<char>(2)), 14642545639667855512ULL );
    BOOST_TEST_EQ( hv(std::list<char>(3)), 17867750819888810972ULL );

#endif

    // list<int>
#if SIZE_MAX == 4294967295U

    BOOST_TEST_EQ( hv(std::list<int>(0)), 0 );
    BOOST_TEST_EQ( hv(std::list<int>(1)), 3864292196U );
    BOOST_TEST_EQ( hv(std::list<int>(2)), 2842917718U );
    BOOST_TEST_EQ( hv(std::list<int>(3)), 325752138U );

#else

    BOOST_TEST_EQ( hv(std::list<int>(0)), 0 );
    BOOST_TEST_EQ( hv(std::list<int>(1)), 3864292196ULL );
    BOOST_TEST_EQ( hv(std::list<int>(2)), 14642545639667855512ULL );
    BOOST_TEST_EQ( hv(std::list<int>(3)), 17867750819888810972ULL );

#endif

    return boost::report_errors();
}

#endif
