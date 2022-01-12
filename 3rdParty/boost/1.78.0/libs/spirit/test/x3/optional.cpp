/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/struct.hpp>
#include <boost/fusion/include/vector.hpp>

#include <iostream>
#include "test.hpp"
#include "utils.hpp"

#ifdef _MSC_VER
// bogus https://developercommunity.visualstudio.com/t/buggy-warning-c4709/471956
# pragma warning(disable: 4709) // comma operator within array index expression
#endif

struct adata
{
    int a;
    boost::optional<int> b;
};

BOOST_FUSION_ADAPT_STRUCT(adata,
    a, b
)

struct test_attribute_type
{
    template <typename Context>
    void operator()(Context& ctx) const
    {
        BOOST_TEST(typeid(decltype(_attr(ctx))).name() == typeid(boost::optional<int>).name());
    }
};

int
main()
{
    using boost::spirit::x3::traits::is_optional;

    static_assert(is_optional<boost::optional<int>>(), "is_optional problem");

    using spirit_test::test;
    using spirit_test::test_attr;

    using boost::spirit::x3::int_;
    using boost::spirit::x3::omit;
    using boost::spirit::x3::ascii::char_;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(-int_);

    {
        BOOST_TEST((test("1234", -int_)));
        BOOST_TEST((test("abcd", -int_, false)));

        boost::optional<int> n;
        BOOST_TEST(test_attr("", -int_, n))
            && BOOST_TEST(!n);
        BOOST_TEST(test_attr("123", -int_, n))
            && BOOST_TEST(n) && BOOST_TEST_EQ(*n, 123);

        boost::optional<std::string> s;
        BOOST_TEST(test_attr("", -+char_, s))
            && BOOST_TEST(!s);
        BOOST_TEST(test_attr("abc", -+char_, s))
            && BOOST_TEST(s) && BOOST_TEST_EQ(*s, "abc");
    }

    {   // test propagation of unused
        using boost::fusion::at_c;
        using boost::fusion::vector;

        vector<char, char> v;
        BOOST_TEST((test_attr("a1234c", char_ >> -omit[int_] >> char_, v)));
        BOOST_TEST((at_c<0>(v) == 'a'));
        BOOST_TEST((at_c<1>(v) == 'c'));

        v = boost::fusion::vector<char, char>();
        BOOST_TEST((test_attr("a1234c", char_ >> omit[-int_] >> char_, v)));
        BOOST_TEST((at_c<0>(v) == 'a'));
        BOOST_TEST((at_c<1>(v) == 'c'));

        char ch;
        BOOST_TEST((test_attr(",c", -(',' >> char_), ch)));
        BOOST_TEST((ch == 'c'));
    }

    {   // test action
        boost::optional<int> n = 0;
        BOOST_TEST((test_attr("1234", (-int_)[test_attribute_type()], n)));
        BOOST_TEST((n.get() == 1234));
    }

    {
        std::string s;
        BOOST_TEST((test_attr("abc", char_ >> -(char_ >> char_), s)));
        BOOST_TEST(s == "abc");
    }

    {
        boost::optional<int> n = 0;
        auto f = [&](auto& ctx){ n = _attr(ctx); };

        BOOST_TEST((test("1234", (-int_)[f])));
        BOOST_TEST(n.get() == 1234);

        n = boost::optional<int>();
        BOOST_TEST((test("abcd", (-int_)[f], false)));
        BOOST_TEST(!n);
    }

    {
        std::vector<adata> v;
        BOOST_TEST((test_attr("a 1 2 a 2", *('a' >> int_ >> -int_), v
          , char_(' '))));
        BOOST_TEST(2 == v.size() &&
            1 == v[0].a && v[0].b && 2 == *(v[0].b) &&
            2 == v[1].a && !v[1].b);
    }

    { // test move only types
        boost::optional<move_only> o;
        BOOST_TEST(test_attr("s", -synth_move_only, o));
        BOOST_TEST(o);
    }

    return boost::report_errors();
}
