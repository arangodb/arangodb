// Copyright (c) 2017 Levon Tarakchyan
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/config.hpp"

#include "boost/core/lightweight_test.hpp"
#include "boost/variant.hpp"
#include "boost/variant/apply_visitor.hpp"
#include "boost/variant/multivisitors.hpp"
#include "boost/lexical_cast.hpp"

#define lcs(val) boost::lexical_cast<std::string>(val)

struct construction_logger
{
    int val_;

    construction_logger(int val) : val_(val)
    {
        std::cout << val_ << " constructed\n";
    }

    construction_logger(const construction_logger& cl) :
        val_(cl.val_)
    {
        std::cout << val_ << " copy constructed\n";
    }

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    construction_logger(construction_logger&& cl) :
        val_(cl.val_)
    {
        std::cout << val_ << " move constructed\n";
    }
#endif

    friend std::ostream& operator << (std::ostream& os, const construction_logger& cl)
    {
        return os << cl.val_;
    }

    friend std::istream& operator << (std::istream& is, construction_logger& cl)
    {
        return is >> cl.val_;
    }
};

struct lex_streamer_explicit : boost::static_visitor<std::string>
{
    template <class T>
    std::string operator()(const T& val) const
    {
        return lcs(val);
    }

    template <class T, class V>
    std::string operator()(const T& val, const V& val2) const
    {
        return lcs(val) + '+' + lcs(val2);
    }

    template <class T, class V, class P, class S>
    std::string operator()(const T& val, const V& val2, const P& val3, const S& val4) const
    {
        return lcs(val) + '+' + lcs(val2) + '+' + lcs(val3) + '+' + lcs(val4);
    }
};

struct lvalue_rvalue_detector : boost::static_visitor<std::string>
{
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    template <class T>
    std::string operator()(T&&) const
    {
        return std::is_lvalue_reference<T>::value ? "lvalue reference"
                                                  : "rvalue reference";
    }

    template <class T, class V>
    std::string operator()(T&& t, V&& v) const
    {
        return operator()(std::forward<T>(t)) + ", " + operator()(std::forward<V>(v));
    }

    template <class T, class V, class P>
    std::string operator()(T&& t, V&& v, P&& p) const
    {
        return operator()(std::forward<T>(t), std::forward<V>(v)) + ", " + operator()(std::forward<P>(p));
    }

    template <class T, class V, class P, class S>
    std::string operator()(T&& t, V&& v, P&& p, S&& s) const
    {
        return operator()(std::forward<T>(t), std::forward<V>(v), std::forward<P>(p)) + ", " + operator()(std::forward<S>(s));
    }
#else
    template <class T>
    std::string operator()(T&) const
    {
        return "lvalue reference";
    }

    template <class T, class V>
    std::string operator()(T&, V&) const
    {
        return "lvalue reference, lvalue reference";
    }

    template <class T, class V, class P>
    std::string operator()(T&, V&, P&) const
    {
        return "lvalue reference, lvalue reference, lvalue reference";
    }

    template <class T, class V, class P, class S>
    std::string operator()(T&, V&, P&, S&) const
    {
        return "lvalue reference, lvalue reference, lvalue reference, lvalue reference";
    }
#endif
};

typedef boost::variant<construction_logger, std::string> variant_type;

void test_const_ref_parameter(const variant_type& test_var)
{
    std::cout << "Testing const lvalue reference visitable\n";

    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), test_var) == "lvalue reference");
}

void test_const_ref_parameter2(const variant_type& test_var, const variant_type& test_var2)
{
    std::cout << "Testing const lvalue reference visitable\n";

    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), test_var, test_var2) == "lvalue reference, lvalue reference");
}

void test_const_ref_parameter4(const variant_type& test_var, const variant_type& test_var2, const variant_type& test_var3, const variant_type& test_var4)
{
    std::cout << "Testing const lvalue reference visitable with multivisitor\n";

    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), test_var, test_var2, test_var3, test_var4)
            == "lvalue reference, lvalue reference, lvalue reference, lvalue reference");
}

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_NO_CXX11_REF_QUALIFIERS)

void test_rvalue_parameter(variant_type&& test_var)
{
    std::cout << "Testing rvalue visitable\n";

    const auto expected_val = lcs(test_var);
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), std::move(test_var)) == "rvalue reference");
}

void test_rvalue_parameter2(variant_type&& test_var, variant_type&& test_var2)
{
    std::cout << "Testing rvalue visitable\n";

    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), std::move(test_var), std::move(test_var2)) == "rvalue reference, rvalue reference");
}

void test_rvalue_parameter4(variant_type&& test_var, variant_type&& test_var2, variant_type&& test_var3, variant_type&& test_var4)
{
#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    std::cout << "Testing rvalue visitable with multivisitor\n";

    auto result = boost::apply_visitor(lvalue_rvalue_detector(), std::move(test_var), std::move(test_var2), std::move(test_var3), std::move(test_var4));
    std::cout << "result: " << result << std::endl;
    BOOST_TEST(result == "rvalue reference, rvalue reference, rvalue reference, rvalue reference");
#else
    (void)test_var;
    (void)test_var2;
    (void)test_var3;
    (void)test_var4;
#endif
}

#endif

#ifndef BOOST_NO_CXX14_DECLTYPE_AUTO

#define FORWARD(x) std::forward<decltype(x)>(x)

void test_cpp14_visitor(const variant_type& test_var)
{
    std::cout << "Testing const lvalue visitable for c++14\n";

    BOOST_TEST(boost::apply_visitor([](auto&& v) { return lvalue_rvalue_detector()(FORWARD(v)); }, test_var) == "lvalue reference");
}

void test_cpp14_mutable_visitor(const variant_type& test_var)
{
    std::cout << "Testing const lvalue visitable for c++14 with inline mutable lambda\n";

    BOOST_TEST(boost::apply_visitor([](auto&& v) mutable -> auto { return lvalue_rvalue_detector()(FORWARD(v)); }, test_var) == "lvalue reference");
}

void test_cpp14_visitor(const variant_type& test_var, const variant_type& test_var2)
{
    std::cout << "Testing const lvalue visitable for c++14\n";

    BOOST_TEST(boost::apply_visitor([](auto&& v, auto&& vv) { return lvalue_rvalue_detector()(FORWARD(v), FORWARD(vv)); }, test_var, test_var2)
            == "lvalue reference, lvalue reference");
}

void test_cpp14_visitor(const variant_type& test_var, const variant_type& test_var2, const variant_type& test_var3)
{
#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    std::cout << "Testing const lvalue visitable for c++14\n";

    auto result = boost::apply_visitor([](auto&& v, auto&& t, auto&& p) { return lvalue_rvalue_detector()(FORWARD(v), FORWARD(t), FORWARD(p)); },
                test_var, test_var2, test_var3);
    std::cout << "result: " << result << std::endl;
    BOOST_TEST(result == "lvalue reference, lvalue reference, lvalue reference");
#else
    (void)test_var;
    (void)test_var2;
    (void)test_var3;
#endif
}

void test_cpp14_visitor(variant_type& test_var)
{
    std::cout << "Testing lvalue visitable for c++14\n";

    BOOST_TEST(boost::apply_visitor([](auto& v) { return lvalue_rvalue_detector()(v); }, test_var) == "lvalue reference");
}

void test_cpp14_mutable_visitor(variant_type& test_var)
{
    std::cout << "Testing lvalue visitable for c++14 with inline mutable lambda\n";

    BOOST_TEST(boost::apply_visitor([](auto& v) mutable -> auto { return lvalue_rvalue_detector()(v); }, test_var) == "lvalue reference");
}

void test_cpp14_visitor(variant_type& test_var, variant_type& test_var2)
{
    std::cout << "Testing lvalue visitable for c++14\n";

    BOOST_TEST(boost::apply_visitor([](auto& v, auto& vv) { return lvalue_rvalue_detector()(v, vv); }, test_var, test_var2)
            == "lvalue reference, lvalue reference");
}

void test_cpp14_visitor(variant_type& test_var, variant_type& test_var2, variant_type& test_var3)
{
#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    std::cout << "Testing lvalue visitable for c++14\n";

    auto result = boost::apply_visitor([](auto& v, auto& t, auto& p) { return lvalue_rvalue_detector()(v, t, p); },
                test_var, test_var2, test_var3);
    std::cout << "result: " << result << std::endl;
    BOOST_TEST(result == "lvalue reference, lvalue reference, lvalue reference");
#else
    (void)test_var;
    (void)test_var2;
    (void)test_var3;
#endif
}

void test_cpp14_visitor(variant_type&& test_var)
{
    std::cout << "Testing rvalue visitable for c++14\n";

    BOOST_TEST(boost::apply_visitor([](auto&& v) { return lvalue_rvalue_detector()(FORWARD(v)); }, std::move(test_var)) == "rvalue reference");
}

void test_cpp14_visitor(variant_type&& test_var, variant_type&& test_var2)
{
    std::cout << "Testing rvalue visitable for c++14\n";

    BOOST_TEST(boost::apply_visitor([](auto&& v, auto&& vv) { return lvalue_rvalue_detector()(FORWARD(v), FORWARD(vv)); }, std::move(test_var), std::move(test_var2))
            == "rvalue reference, rvalue reference");
}

void test_cpp14_visitor(variant_type&& test_var, variant_type&& test_var2, variant_type&& test_var3)
{
#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    std::cout << "Testing rvalue visitable for c++14\n";

    auto result = boost::apply_visitor([](auto&& v, auto&& t, auto&& p) { return lvalue_rvalue_detector()(FORWARD(v), FORWARD(t), FORWARD(p)); },
                std::move(test_var), std::move(test_var2), std::move(test_var3));
    std::cout << "result: " << result << std::endl;
    BOOST_TEST(result == "rvalue reference, rvalue reference, rvalue reference");
#else
    (void)test_var;
    (void)test_var2;
    (void)test_var3;
#endif
}

#endif

void run_const_lvalue_ref_tests()
{
    const variant_type v1(1), v2(2), v3(3), v4(4);
    test_const_ref_parameter(v1);
    test_const_ref_parameter2(v1, v2);
    test_const_ref_parameter4(v1, v2, v3, v4);
}

void run_rvalue_ref_tests()
{
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_NO_CXX11_REF_QUALIFIERS)
    variant_type v1(10), v2(20), v3(30);
    test_rvalue_parameter(boost::move(v1));
    test_rvalue_parameter2(boost::move(v2), boost::move(v3));

    variant_type vv1(100), vv2(200), vv3(300), vv4(400);
    test_rvalue_parameter4(boost::move(vv1), boost::move(vv2), boost::move(vv3), boost::move(vv4));
#endif
}

void run_mixed_tests()
{
    variant_type v1(1), v2(2);

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
#ifndef BOOST_NO_CXX11_REF_QUALIFIERS
    std::cout << "Testing lvalue + rvalue visitable\n";
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), v1, variant_type(10)) == "lvalue reference, rvalue reference");

    std::cout << "Testing rvalue + lvalue visitable\n";
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), variant_type(10), v1) == "rvalue reference, lvalue reference");

#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    std::cout << "Testing rvalue + lvalue + rvalue visitable\n";
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), variant_type(10), v1, variant_type(20)) == "rvalue reference, lvalue reference, rvalue reference");

    std::cout << "Testing lvalue + rvalue + lvalue + rvalue visitable\n";
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), v1, variant_type(10), v2, variant_type(20)) == "lvalue reference, rvalue reference, lvalue reference, rvalue reference");
#endif

#endif // #ifndef BOOST_NO_CXX11_REF_QUALIFIERS

#else // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    std::cout << "Testing lvalue + rvalue visitable\n";
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), v1, v1) == "lvalue reference, lvalue reference");

    std::cout << "Testing rvalue + lvalue visitable\n";
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), static_cast<const variant_type&>(variant_type(10)), v1) == "lvalue reference, lvalue reference");

#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    std::cout << "Testing rvalue + lvalue + rvalue visitable\n";
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), static_cast<const variant_type&>(variant_type(10)), v1, static_cast<const variant_type&>(variant_type(20))) == "lvalue reference, lvalue reference, lvalue reference");

    std::cout << "Testing lvalue + rvalue + lvalue + rvalue visitable\n";
    BOOST_TEST(boost::apply_visitor(lvalue_rvalue_detector(), v1, static_cast<const variant_type&>(variant_type(10)), v2, static_cast<const variant_type&>(variant_type(20))) == "lvalue reference, lvalue reference, lvalue reference, lvalue reference");
#endif
#endif
}

void run_cpp14_mixed_tests()
{
#ifndef BOOST_NO_CXX14_DECLTYPE_AUTO
    variant_type v1(1), v2(2);

    std::cout << "Testing lvalue + rvalue visitable\n";
    BOOST_TEST(boost::apply_visitor([](auto&& v, auto&& t) { return lvalue_rvalue_detector()(FORWARD(v), FORWARD(t)); },
                v1, variant_type(10)) == "lvalue reference, rvalue reference");

    std::cout << "Testing rvalue + lvalue visitable\n";
    BOOST_TEST(boost::apply_visitor([](auto&& v, auto&& t) { return lvalue_rvalue_detector()(FORWARD(v), FORWARD(t)); },
                variant_type(10), v1) == "rvalue reference, lvalue reference");

#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    std::cout << "Testing rvalue + lvalue + lvalue visitable\n";
    BOOST_TEST(boost::apply_visitor([](auto&& v, auto&& t, auto&& p) { return lvalue_rvalue_detector()(FORWARD(v), FORWARD(t), FORWARD(p)); },
                variant_type(10), v1, v2) == "rvalue reference, lvalue reference, lvalue reference");

    std::cout << "Testing lvalue + rvalue + lvalue visitable\n";
    BOOST_TEST(boost::apply_visitor([](auto&& v, auto&& t, auto&& p) { return lvalue_rvalue_detector()(FORWARD(v), FORWARD(t), FORWARD(p)); },
                v1, variant_type(10), v2) == "lvalue reference, rvalue reference, lvalue reference");
#endif
#endif
}

void run_cpp14_tests()
{
#ifndef BOOST_NO_CXX14_DECLTYPE_AUTO
    variant_type const c1(10), c2(20), c3(30);
    variant_type v1(10), v2(20), v3(30);

    test_cpp14_visitor(c1);
    test_cpp14_mutable_visitor(c1);
    test_cpp14_visitor(c2, c3);
    test_cpp14_visitor(c1, c2, c3);

    test_cpp14_visitor(v1);
    test_cpp14_mutable_visitor(v1);
    test_cpp14_visitor(v2, v3);
    test_cpp14_visitor(v1, v2, v3);

    test_cpp14_visitor(boost::move(v1));
    test_cpp14_visitor(boost::move(v2), boost::move(v3));

    variant_type vv1(100), vv2(200), vv3(300);
    test_cpp14_visitor(boost::move(vv1), boost::move(vv2), boost::move(vv3));
#endif
}



int main()
{
    run_const_lvalue_ref_tests();
    run_rvalue_ref_tests();
    run_mixed_tests();
    run_cpp14_mixed_tests();
    run_cpp14_tests();

    return boost::report_errors();
}
