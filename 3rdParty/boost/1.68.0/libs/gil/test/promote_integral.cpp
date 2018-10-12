// Boost.GIL (Generic Image Library)
//
// Copyright (c) 2015, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
//
// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html
//
// Source: Boost.Geometry (aka GGL, Generic Geometry Library)
// Modifications: adapted for Boost.GIL
//  - Rename namespace boost::geometry to boost::gil
//  - Rename define BOOST_GEOMETRY_TEST_DEBUG to BOOST_GIL_TEST_DEBUG
//  - Remove use of macro BOOST_GEOMETRY_CONDITION
//  - Remove support for boost::multiprecision types
//  - Remove support for 128-bit integer types
//  - Update and sort includes
//
#include <boost/gil/promote_integral.hpp>

#include <boost/config.hpp>

#include <climits>
#include <cstddef>
#include <algorithm>
#include <limits>
#include <iostream>
#include <string>
#include <sstream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_promote_integral
#endif
#include <gil_test_common.hpp>

// Uncomment to enable debugging output
//#define BOOST_GIL_TEST_DEBUG 1

namespace bg = boost::gil;

template
<
    typename T,
    bool Signed = std::is_fundamental<T>::type::value && !std::is_unsigned<T>::type::value
>
struct absolute_value
{
    static inline T apply(T const& t)
    {
        return t < 0 ? -t : t;
    }
};

template <typename T>
struct absolute_value<T, false>
{
    static inline T apply(T const& t)
    {
        return t;
    }
};

template
    <
        typename Integral,
        typename Promoted,
        bool Signed = !std::is_unsigned<Promoted>::type::value
    >
struct test_max_values
{
    static inline void apply()
    {
        Promoted min_value = (std::numeric_limits<Integral>::min)();
        min_value *= min_value;
        BOOST_CHECK(absolute_value<Promoted>::apply(min_value) == min_value);
        Promoted max_value = (std::numeric_limits<Integral>::max)();
        max_value *= max_value;
        BOOST_CHECK(absolute_value<Promoted>::apply(max_value) == max_value);

#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "integral min_value^2: " << min_value << std::endl;
        std::cout << "promoted max_value:   "
                  << (std::numeric_limits<Promoted>::max)() << std::endl;
#endif
    }
};

template <typename Integral, typename Promoted>
struct test_max_values<Integral, Promoted, false>
{
    static inline void apply()
    {
        Promoted max_value = (std::numeric_limits<Integral>::max)();
        Promoted max_value_sqr = max_value * max_value;
        BOOST_CHECK(max_value_sqr < (std::numeric_limits<Promoted>::max)()
                    &&
                    max_value_sqr > max_value);

#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "integral max_value^2: " << max_value_sqr << std::endl;
        std::cout << "promoted max_value:   "
                  << (std::numeric_limits<Promoted>::max)() << std::endl;
#endif
    }
};


// helper function that returns the bit size of a type
template
    <
        typename T,
        bool IsFundamental = std::is_fundamental<T>::type::value
    >
struct bit_size_impl : std::integral_constant<std::size_t, 0>
{};

template <typename T>
struct bit_size_impl<T, true> : bg::detail::promote_integral::bit_size<T>::type
{};

template <typename T>
std::size_t bit_size()
{
    return bit_size_impl<T>::type::value;
}

template <bool PromoteUnsignedToUnsigned>
struct test_promote_integral
{
    template <typename Type, typename ExpectedPromotedType>
    static inline void apply(std::string const& case_id)
    {
        typedef typename bg::promote_integral
            <
                Type, PromoteUnsignedToUnsigned
            >::type promoted_integral_type;

        bool const same_types = std::is_same
            <
                promoted_integral_type, ExpectedPromotedType
            >::type::value;

        BOOST_CHECK_MESSAGE(same_types,
                            "case ID: " << case_id
                                        << "input type: " << typeid(Type).name()
                                        << "; detected: "
                                        << typeid(promoted_integral_type).name()
                                        << "; expected: "
                                        << typeid(ExpectedPromotedType).name());

        if (!std::is_same<Type, promoted_integral_type>::type::value)
        {
            test_max_values<Type, promoted_integral_type>::apply();
        }

#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "case ID: " << case_id << std::endl
                  << "type : " << typeid(Type).name()
                  << ", sizeof (bits): " << bit_size<Type>()
                  << ", min value: "
                  << (std::numeric_limits<Type>::min)()
                  << ", max value: "
                  << (std::numeric_limits<Type>::max)()
                  << std::endl;
        std::cout << "detected promoted type : "
                  << typeid(promoted_integral_type).name()
                  << ", sizeof (bits): " << bit_size<promoted_integral_type>()
                  << ", min value: "
                  << (std::numeric_limits<promoted_integral_type>::min)()
                  << ", max value: "
                  << (std::numeric_limits<promoted_integral_type>::max)()
                  << std::endl;
        std::cout << "expected promoted type : "
                  << typeid(ExpectedPromotedType).name()
                  << ", sizeof (bits): " << bit_size<ExpectedPromotedType>()
                  << ", min value: "
                  << (std::numeric_limits<ExpectedPromotedType>::min)()
                  << ", max value: "
                  << (std::numeric_limits<ExpectedPromotedType>::max)()
                  << std::endl;
        std::cout << std::endl;
#endif
    }
};

template
        <
                typename T,
                bool PromoteUnsignedToUnsigned = false,
                bool IsSigned = !std::is_unsigned<T>::type::value
        >
struct test_promotion
{
    static inline void apply(std::string case_id)
    {
#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "*** "
                  << (IsSigned ? "signed" : "unsigned")
                  << " -> signed ***" << std::endl;
#endif

        typedef test_promote_integral<PromoteUnsignedToUnsigned> tester;

        case_id += (PromoteUnsignedToUnsigned ? "-t" : "-f");

        std::size_t min_size = 2 * bit_size<T>() - 1;
        if (!IsSigned)
        {
            min_size += 2;
        }

#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "min size: " << min_size << std::endl;
#endif

        if (bit_size<short>() >= min_size)
        {
            tester::template apply<T, short>(case_id);
        }
        else if (bit_size<int>() >= min_size)
        {
            tester::template apply<T, int>(case_id);
        }
        else if (bit_size<long>() >= min_size)
        {
            tester::template apply<T, long>(case_id);
        }
#if defined(BOOST_HAS_LONG_LONG)
        else if (bit_size<boost::long_long_type>() >= min_size)
        {
            tester::template apply<T, boost::long_long_type>(case_id);
        }
#endif
        else
        {
            tester::template apply<T, T>(case_id);
        }
    }
};

template <typename T>
struct test_promotion<T, true, false>
{
    static inline void apply(std::string case_id)
    {
#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "*** unsigned -> unsigned ***" << std::endl;
#endif
        case_id += "-t";

        typedef test_promote_integral<true> tester;

        std::size_t min_size = 2 * bit_size<T>();

#ifdef BOOST_GIL_TEST_DEBUG
        std::cout << "min size: " << min_size << std::endl;
#endif

        if (bit_size<unsigned short>() >= min_size)
        {
            tester::apply<T, unsigned short>(case_id);
        }
        else if (bit_size<unsigned int>() >= min_size)
        {
            tester::apply<T, unsigned int>(case_id);
        }
        else if (bit_size<unsigned long>() >= min_size)
        {
            tester::apply<T, unsigned long>(case_id);
        }
        else if (bit_size<std::size_t>() >= min_size)
        {
            tester::apply<T, std::size_t>(case_id);
        }
#if defined(BOOST_HAS_LONG_LONG)
        else if (bit_size<boost::ulong_long_type>() >= min_size)
        {
            tester::template apply<T, boost::ulong_long_type>(case_id);
        }
#endif
        else
        {
            tester::apply<T, T>(case_id);
        }
    }
};

BOOST_AUTO_TEST_CASE( test_char )
{
    test_promotion<char>::apply("char");
    test_promotion<char, true>::apply("char");
    test_promotion<signed char>::apply("schar");
    test_promotion<signed char, true>::apply("schar");
    test_promotion<unsigned char>::apply("uchar");
    test_promotion<unsigned char, true>::apply("uchar");
}

BOOST_AUTO_TEST_CASE( test_short )
{
    test_promotion<short>::apply("short");
    test_promotion<short, true>::apply("short");
    test_promotion<unsigned short>::apply("ushort");
    test_promotion<unsigned short, true>::apply("ushort");
}

BOOST_AUTO_TEST_CASE( test_int )
{
    test_promotion<int>::apply("int");
    test_promotion<int, true>::apply("int");
    test_promotion<unsigned int>::apply("uint");
    test_promotion<unsigned int, true>::apply("uint");
}

BOOST_AUTO_TEST_CASE( test_long )
{
    test_promotion<long>::apply("long");
    test_promotion<long, true>::apply("long");
    test_promotion<unsigned long>::apply("ulong");
    test_promotion<unsigned long, true>::apply("ulong");
}

BOOST_AUTO_TEST_CASE( test_std_size_t )
{
    test_promotion<std::size_t>::apply("size_t");
    test_promotion<std::size_t, true>::apply("size_t");
}

#ifdef BOOST_HAS_LONG_LONG
BOOST_AUTO_TEST_CASE( test_long_long )
{
    test_promotion<boost::long_long_type>::apply("long long");
    test_promotion<boost::long_long_type, true>::apply("long long");
    test_promotion<boost::ulong_long_type>::apply("ulong long");
    test_promotion<boost::ulong_long_type, true>::apply("ulong long");
}
#endif

BOOST_AUTO_TEST_CASE( test_floating_point )
{
    typedef test_promote_integral<true> tester1;
    typedef test_promote_integral<false> tester2;

    // for floating-point types we do not do any promotion
    tester1::apply<float, float>("fp-f");
    tester1::apply<double, double>("fp-d");
    tester1::apply<long double, long double>("fp-ld");

    tester2::apply<float, float>("fp-f");
    tester2::apply<double, double>("fp-d");
    tester2::apply<long double, long double>("fp-ld");
}
