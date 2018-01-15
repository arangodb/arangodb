// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_promote_integral
#endif

#include <climits>
#include <cstddef>
#include <algorithm>
#include <limits>
#include <iostream>
#include <string>
#include <sstream>

#include <boost/test/included/unit_test.hpp>

#include <boost/config.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_unsigned.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/util/promote_integral.hpp>

#if !defined(BOOST_GEOMETRY_NO_MULTIPRECISION_INTEGER)
#include <boost/multiprecision/cpp_int.hpp>
#endif

#if defined(BOOST_GEOMETRY_TEST_DEBUG)
#if defined(BOOST_HAS_INT128) && defined(BOOST_GEOMETRY_ENABLE_INT128)
void print_uint128_t(std::ostream& os, boost::uint128_type i)
{
    if (i == 0)
    {
        os << "0";
        return;
    }

    std::stringstream stream;
    while (i > 0)
    {
        stream << static_cast<int>(i % 10);
        i /= 10;
    }
    std::string str = stream.str();
    std::reverse(str.begin(), str.end());
    os << str;
}

std::ostream& operator<<(std::ostream& os, boost::int128_type i)
{
    if (i < 0)
    {
        os << "-";
        print_uint128_t(os, static_cast<boost::uint128_type>(-i));
    }
    else
    {
        print_uint128_t(os, static_cast<boost::uint128_type>(i));
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, boost::uint128_type i)
{
    print_uint128_t(os, i);
    return os;
}
#endif // BOOST_HAS_INT128 && BOOST_GEOMETRY_ENABLE_INT128
#endif // BOOST_GEOMETRY_TEST_DEBUG

namespace bg = boost::geometry;

template
<
    typename T,
    bool Signed = boost::is_fundamental<T>::type::value
    && ! boost::is_unsigned<T>::type::value
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
    bool Signed = ! boost::is_unsigned<Promoted>::type::value
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

#ifdef BOOST_GEOMETRY_TEST_DEBUG
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

#ifdef BOOST_GEOMETRY_TEST_DEBUG
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
    bool IsFundamental = boost::is_fundamental<T>::type::value
>
struct bit_size_impl : boost::mpl::size_t<0>
{};

template <typename T>
struct bit_size_impl<T, true> : bg::detail::promote_integral::bit_size<T>::type
{};

#if !defined(BOOST_GEOMETRY_NO_MULTIPRECISION_INTEGER)
template
<
    typename Backend,
    boost::multiprecision::expression_template_option ExpressionTemplates
>
struct bit_size_impl
    <
        boost::multiprecision::number<Backend, ExpressionTemplates>,
        false
    > : bg::detail::promote_integral::bit_size
        <
            boost::multiprecision::number<Backend, ExpressionTemplates>
        >
{};
#endif


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

        bool const same_types = boost::is_same
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

        if (BOOST_GEOMETRY_CONDITION((! boost::is_same
                <
                    Type, promoted_integral_type
                >::type::value)))
        {
            test_max_values<Type, promoted_integral_type>::apply();
        }

#ifdef BOOST_GEOMETRY_TEST_DEBUG
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
    bool IsSigned = ! boost::is_unsigned<T>::type::value
>
struct test_promotion
{
    static inline void apply(std::string case_id)
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "*** "
                  << (IsSigned ? "signed" : "unsigned")
                  << " -> signed ***" << std::endl;
#endif

        typedef test_promote_integral<PromoteUnsignedToUnsigned> tester;

        case_id += (PromoteUnsignedToUnsigned ? "-t" : "-f");

        std::size_t min_size = 2 * bit_size<T>() - 1;
        if (BOOST_GEOMETRY_CONDITION(! IsSigned))
        {
            min_size += 2;
        }

#ifdef BOOST_GEOMETRY_TEST_DEBUG
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
#if defined(BOOST_HAS_INT128) && defined(BOOST_GEOMETRY_ENABLE_INT128)
        else if (bit_size<boost::int128_type>() >= min_size)
        {
            tester::template apply<T, boost::int128_type>(case_id);
        }
#endif
        else
        {
#if !defined(BOOST_GEOMETRY_NO_MULTIPRECISION_INTEGER)
            namespace bm = boost::multiprecision;
            typedef bm::number
                <
                    bm::cpp_int_backend
                    <
                        2 * CHAR_BIT * sizeof(T) + (IsSigned ? -1 : 1),
                        2 * CHAR_BIT * sizeof(T) + (IsSigned ? -1 : 1),
                        bm::signed_magnitude,
                        bm::unchecked,
                        void
                    >
                > multiprecision_integer_type;

            tester::template apply<T, multiprecision_integer_type>(case_id);
#else
            tester::template apply<T, T>(case_id);
#endif
        }
    }
};

template <typename T>
struct test_promotion<T, true, false>
{
    static inline void apply(std::string case_id)
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "*** unsigned -> unsigned ***" << std::endl;
#endif
        case_id += "-t";

        typedef test_promote_integral<true> tester;

        std::size_t min_size = 2 * bit_size<T>();

#ifdef BOOST_GEOMETRY_TEST_DEBUG
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
#if defined(BOOST_HAS_INT128) && defined(BOOST_GEOMETRY_ENABLE_INT128)
        else if (bit_size<boost::uint128_type>() >= min_size)
        {
            tester::template apply<T, boost::uint128_type>(case_id);
        }
#endif
        else
        {
#if !defined(BOOST_GEOMETRY_NO_MULTIPRECISION_INTEGER)
            namespace bm = boost::multiprecision;
            typedef bm::number
                <
                    bm::cpp_int_backend
                    <
                        2 * CHAR_BIT * sizeof(T),
                        2 * CHAR_BIT * sizeof(T),
                        bm::unsigned_magnitude,
                        bm::unchecked,
                        void
                    >
                > multiprecision_integer_type;

            tester::apply<T, multiprecision_integer_type>(case_id);
#else
            tester::apply<T, T>(case_id);
#endif
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

#if defined(BOOST_HAS_INT128) && defined(BOOST_GEOMETRY_ENABLE_INT128)
BOOST_AUTO_TEST_CASE( test_int128 )
{
    test_promotion<boost::int128_type>::apply("int128_t");
    test_promotion<boost::int128_type, true>::apply("int128_t");
    test_promotion<boost::uint128_type>::apply("uint128_t");
    test_promotion<boost::uint128_type, true>::apply("uint128_t");
}
#endif

#if !defined(BOOST_GEOMETRY_NO_MULTIPRECISION_INTEGER)
BOOST_AUTO_TEST_CASE( test_user_types )
{
    namespace bm = boost::multiprecision;
    typedef bm::number
        <
            bm::cpp_int_backend
                <
                    17,
                    17,
                    bm::signed_magnitude,
                    bm::unchecked,
                    void
                >
        > user_signed_type1;

    typedef bm::number
        <
            bm::cpp_int_backend
                <
                    17,
                    17,
                    bm::unsigned_magnitude,
                    bm::unchecked,
                    void
                >
        > user_unsigned_type1;

    typedef bm::number
        <
            bm::cpp_int_backend
                <
                    500,
                    500,
                    bm::signed_magnitude,
                    bm::unchecked,
                    void
                >
        > user_signed_type2;

    typedef bm::number
        <
            bm::cpp_int_backend
                <
                    500,
                    500,
                    bm::unsigned_magnitude,
                    bm::unchecked,
                    void
                >
        > user_unsigned_type2;

    // for user defined number types we do not do any promotion
    typedef test_promote_integral<true> tester1;
    typedef test_promote_integral<false> tester2;
    tester1::apply<user_signed_type1, user_signed_type1>("u1s");
    tester1::apply<user_signed_type2, user_signed_type2>("u2s");
    tester1::apply<user_unsigned_type1, user_unsigned_type1>("u1u");
    tester1::apply<user_unsigned_type2, user_unsigned_type2>("u2u");

    tester2::apply<user_signed_type1, user_signed_type1>("u1s");
    tester2::apply<user_signed_type2, user_signed_type2>("u2s");
    tester2::apply<user_unsigned_type1, user_unsigned_type1>("u1u");
    tester2::apply<user_unsigned_type2, user_unsigned_type2>("u1u");
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

#ifdef HAVE_TTMATH
    tester1::apply<ttmath_big, ttmath_big>("fp-tt");
    tester2::apply<ttmath_big, ttmath_big>("fp-tt");
#endif
}
