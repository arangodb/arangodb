// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//
#include <boost/range/adaptor/formatted.hpp>
#include <boost/cstdint.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

namespace boost_range_test
{
    namespace
    {

    template<typename T>
    std::string make_string(T x)
    {
        std::ostringstream result;
        result << x;
        return result.str();
    }

    template<typename T1, typename T2>
    std::string make_string(T1 x, T2 y)
    {
        std::ostringstream result;
        result << x << y;
        return result.str();
    }

std::string reference_result(const std::vector<boost::int32_t>& v,
                             const std::string& separator,
                             const std::string& prefix,
                             const std::string& postfix)
{
    std::ostringstream out;
    out << prefix;
    if (!v.empty())
    {
        out << v.at(0);
        std::vector<boost::int32_t>::const_iterator it = v.begin();
        for (++it; it != v.end(); ++it)
        {
            out << separator << *it;
        }
    }
    out << postfix;

    return out.str();
}

void test_formatted_0args_impl(const std::vector<boost::int32_t>& v)
{
    std::ostringstream out1;
    out1 << '[' << (v | boost::adaptors::formatted()) << ']';
    BOOST_CHECK_EQUAL(out1.str(), reference_result(v, ",", "[{", "}]"));

    std::ostringstream out2;
    out2 << '[' << boost::adaptors::format(v) << ']';
    BOOST_CHECK_EQUAL(out2.str(), reference_result(v, ",", "[{", "}]"));

    std::ostringstream out3;
    out3 << (v | boost::adaptors::formatted());
    BOOST_CHECK_EQUAL(out3.str(), reference_result(v, ",", "{", "}"));

    std::ostringstream out4;
    out4 << boost::adaptors::format(v);
    BOOST_CHECK_EQUAL(out4.str(), reference_result(v, ",", "{", "}"));
}

template<typename Sep>
void test_formatted_1arg_impl(
        const std::vector<boost::int32_t>& v,
        const Sep& sep)
{
    const std::string ref_sep = make_string(sep);
    std::ostringstream out1;
    out1 << '[' << (v | boost::adaptors::formatted(sep)) << ']';

    BOOST_CHECK_EQUAL(out1.str(), reference_result(v, ref_sep, "[{", "}]"));

    std::ostringstream out2;
    out2 << '[' << boost::adaptors::format(v, sep) << ']';
    BOOST_CHECK_EQUAL(out2.str(), reference_result(v, ref_sep, "[{", "}]"));

    std::ostringstream out3;
    out3 << (v | boost::adaptors::formatted(sep));
    BOOST_CHECK_EQUAL(out3.str(), reference_result(v, ref_sep, "{", "}"));

    std::ostringstream out4;
    out4 << boost::adaptors::format(v, sep);
    BOOST_CHECK_EQUAL(out4.str(), reference_result(v, ref_sep, "{", "}"));
}

void test_formatted_1arg_impl(const std::vector<boost::int32_t>& v)
{
    test_formatted_1arg_impl(v, ',');
    test_formatted_1arg_impl(v, ' ');
    test_formatted_1arg_impl<const char[3]>(v, ":?");
}

template<typename Sep, typename Prefix>
void test_formatted_2args_impl(
        const std::vector<boost::int32_t>& v,
        const Sep& sep,
        const Prefix& prefix
)
{
    const std::string ref_sep = make_string(sep);

    std::ostringstream out1;
    out1 << '[' << (v | boost::adaptors::formatted(sep, prefix)) << ']';
    BOOST_CHECK_EQUAL(
        out1.str(),
        reference_result(v, ref_sep, make_string('[', prefix), "}]"));

    std::ostringstream out2;
    out2 << '[' << boost::adaptors::format(v, sep, prefix) << ']';
    BOOST_CHECK_EQUAL(
        out2.str(),
        reference_result(v, ref_sep, make_string('[', prefix), "}]"));

    std::ostringstream out3;
    out3 << (v | boost::adaptors::formatted(sep, prefix));
    BOOST_CHECK_EQUAL(
        out3.str(),
        reference_result(v, ref_sep, make_string(prefix), "}"));

    std::ostringstream out4;
    out4 << boost::adaptors::format(v, sep, prefix);
    BOOST_CHECK_EQUAL(
        out4.str(),
        reference_result(v, ref_sep, make_string(prefix), "}"));
}

void test_formatted_2args_impl(const std::vector<boost::int32_t>& v)
{
    test_formatted_2args_impl(v, ',', '{');
    test_formatted_2args_impl(v, ':', '(');
    test_formatted_2args_impl<char, const char[3]>(v, ',', "{!");
    test_formatted_2args_impl<const char[3], char>(v, "#$", '{');
    test_formatted_2args_impl<const char[3], const char[3]>(v, "#$", "{!");
}

template<typename Sep, typename Prefix, typename Postfix>
void test_formatted_3args_impl(
        const std::vector<boost::int32_t>& v,
        const Sep& sep,
        const Prefix& prefix,
        const Postfix& postfix
)
{
    const std::string ref_sep = make_string(sep);

    std::ostringstream out1;
    out1 << '[' << (v | boost::adaptors::formatted(sep, prefix, postfix))
         << ']';
    BOOST_CHECK_EQUAL(
        out1.str(),
        reference_result(v, ref_sep, make_string('[', prefix),
                         make_string(postfix, ']')));
}

void test_formatted_3args_impl(const std::vector<boost::int32_t>& v)
{
    test_formatted_3args_impl(v, ',', '{', '}');
    test_formatted_3args_impl(v, ':', '(', ')');
    test_formatted_3args_impl<char, char, const char[3]>(v, ',', '{', "!}");
    test_formatted_3args_impl<char, const char[3], char>(v, ',', "{!", '}');
    test_formatted_3args_impl<const char[3], char, char>(v, "#$", '{', '}');
    test_formatted_3args_impl<
            const char[3], const char[3], const char[3]
    >(v, "#$", "{!", "!}");
}

void test_formatted_impl(const std::vector<boost::int32_t>& v)
{
    test_formatted_0args_impl(v);
    test_formatted_1arg_impl(v);
    test_formatted_2args_impl(v);
    test_formatted_3args_impl(v);
}

void test_formatted1()
{
    std::vector<boost::int32_t> v;
    for (boost::int32_t i = 0; i < 10; ++i)
        v.push_back(i);

    test_formatted_impl(v);
}

void test_formatted2()
{
    std::vector<boost::int32_t> v;
    v.push_back(3);

    test_formatted_impl(v);
}

void test_formatted3()
{
    std::vector<boost::int32_t> v;

    test_formatted_impl(v);
}

void test_formatted4()
{
    std::vector<boost::int32_t> v;
    for (boost::int32_t i = 0; i < 5; ++i)
        v.push_back(i);

    test_formatted_impl(v);
}

struct udt_separator
{
};

template<typename Char, typename Traits>
inline std::basic_ostream<Char,Traits>&
operator<<(std::basic_ostream<Char,Traits>& out, udt_separator)
{
    return out << "[sep]";
}

void test_formatted5()
{
    std::vector<boost::int32_t> v;
    for (boost::int32_t i = 0; i < 5; ++i)
        v.push_back(i);

    std::ostringstream out1;
    out1 << (v | boost::adaptors::formatted(udt_separator()));
    BOOST_CHECK_EQUAL(out1.str(), "{0[sep]1[sep]2[sep]3[sep]4}");

    std::ostringstream out2;
    out2 << boost::adaptors::format(v, udt_separator());
    BOOST_CHECK_EQUAL(out2.str(), "{0[sep]1[sep]2[sep]3[sep]4}");
}

// This test is already covered by the more complex code above. This
// code duplicates coverage to ensure that char literal arrays are handled
// correctly. I was particularly concerned that my test code above may pass
// erroneously by decaying a char literal to a pointer. This function makes
// it very plain that character literal strings work.
void test_formatted_empty()
{
    std::vector<boost::int32_t> v;

    std::ostringstream out1;
    out1 << (v | boost::adaptors::formatted());
    BOOST_CHECK_EQUAL(out1.str(), "{}");

    std::ostringstream out2;
    out2 << boost::adaptors::format(v);
    BOOST_CHECK_EQUAL(out2.str(), "{}");

    std::ostringstream out3;
    out3 << (v | boost::adaptors::formatted(','));
    BOOST_CHECK_EQUAL(out3.str(), "{}");

    std::ostringstream out4;
    out4 << boost::adaptors::format(v, ',');
    BOOST_CHECK_EQUAL(out4.str(), "{}");

    std::ostringstream out5;
    out5 << (v | boost::adaptors::formatted("#$"));
    BOOST_CHECK_EQUAL(out5.str(), "{}");

    std::ostringstream out6;
    out6 << boost::adaptors::format(v, "#$");
    BOOST_CHECK_EQUAL(out6.str(), "{}");

    std::ostringstream out7;
    out7 << (v | boost::adaptors::formatted("", "12", "34"));
    BOOST_CHECK_EQUAL(out7.str(), "1234");

    std::ostringstream out8;
    out8 << boost::adaptors::format(v, "", "12", "34");
    BOOST_CHECK_EQUAL(out8.str(), "1234");
}

    } // anonymous namespace
} // namespace boost_range_test

boost::unit_test::test_suite* init_unit_test_suite(int, char*[] )
{
    boost::unit_test::test_suite* test =
            BOOST_TEST_SUITE( "Boost.Range formatted test suite" );

    test->add(BOOST_TEST_CASE(&boost_range_test::test_formatted1));
    test->add(BOOST_TEST_CASE(&boost_range_test::test_formatted2));
    test->add(BOOST_TEST_CASE(&boost_range_test::test_formatted3));
    test->add(BOOST_TEST_CASE(&boost_range_test::test_formatted4));
    test->add(BOOST_TEST_CASE(&boost_range_test::test_formatted5));

    test->add(BOOST_TEST_CASE(&boost_range_test::test_formatted_empty));

    return test;
}
