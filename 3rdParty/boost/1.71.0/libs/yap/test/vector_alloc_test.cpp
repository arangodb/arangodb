// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/yap.hpp>

#include <vector>
#include <iostream>

#include <boost/test/minimal.hpp>


int allocations = 0;

void * operator new(std::size_t size)
{
    ++allocations;
    void * retval = malloc(size);
    if (!retval)
        throw std::bad_alloc();
    return retval;
}

void operator delete(void * ptr) noexcept { free(ptr); }


struct take_nth
{
    template<typename T>
    auto operator()(
        boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
        std::vector<T> const & vec)
    {
        return boost::yap::make_terminal(vec[n]);
    }

    std::size_t n;
};

struct equal_sizes_impl
{
    template<typename T>
    auto operator()(
        boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
        std::vector<T> const & vec)
    {
        auto const expr_size = vec.size();
        if (expr_size != size)
            value = false;
        return 0;
    }

    std::size_t const size;
    bool value;
};

template<typename Expr>
bool equal_sizes(std::size_t size, Expr const & expr)
{
    equal_sizes_impl impl{size, true};
    boost::yap::transform(boost::yap::as_expr(expr), impl);
    return impl.value;
}


template<typename T, typename Expr>
std::vector<T> & assign(std::vector<T> & vec, Expr const & e)
{
    decltype(auto) expr = boost::yap::as_expr(e);
    assert(equal_sizes(vec.size(), expr));
    for (std::size_t i = 0, size = vec.size(); i < size; ++i) {
        vec[i] = boost::yap::evaluate(
            boost::yap::transform(boost::yap::as_expr(expr), take_nth{i}));
    }
    return vec;
}

template<typename T, typename Expr>
std::vector<T> & operator+=(std::vector<T> & vec, Expr const & e)
{
    decltype(auto) expr = boost::yap::as_expr(e);
    assert(equal_sizes(vec.size(), expr));
    for (std::size_t i = 0, size = vec.size(); i < size; ++i) {
        vec[i] += boost::yap::evaluate(
            boost::yap::transform(boost::yap::as_expr(expr), take_nth{i}));
    }
    return vec;
}

template<typename T>
struct is_vector : std::false_type
{
};

template<typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type
{
};

BOOST_YAP_USER_UDT_UNARY_OPERATOR(
    negate, boost::yap::expression, is_vector); // -
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    multiplies, boost::yap::expression, is_vector); // *
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    divides, boost::yap::expression, is_vector); // /
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    modulus, boost::yap::expression, is_vector); // %
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    plus, boost::yap::expression, is_vector); // +
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    minus, boost::yap::expression, is_vector); // -
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    less, boost::yap::expression, is_vector); // <
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    greater, boost::yap::expression, is_vector); // >
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    less_equal, boost::yap::expression, is_vector); // <=
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    greater_equal, boost::yap::expression, is_vector); // >=
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    equal_to, boost::yap::expression, is_vector); // ==
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    not_equal_to, boost::yap::expression, is_vector); // !=
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    logical_or, boost::yap::expression, is_vector); // ||
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    logical_and, boost::yap::expression, is_vector); // &&
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    bitwise_and, boost::yap::expression, is_vector); // &
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    bitwise_or, boost::yap::expression, is_vector); // |
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(
    bitwise_xor, boost::yap::expression, is_vector); // ^

int test_main(int, char * [])
{
    int i;
    int const n = 10;
    std::vector<int> a(n), b(n), c(n), d(n);
    std::vector<double> e(n);

    // Reset allocation count.  There should be none from this point on.
    allocations = 0;

    for (i = 0; i < n; ++i) {
        a[i] = i;
        b[i] = 2 * i;
        c[i] = 3 * i;
        d[i] = i;
    }

    assign(b, 2);
    assign(d, a + b * c);

    if_else(d < 30, b, c);
    a += if_else(d < 30, b, c);

    assign(e, c);
    e += e - 4 / (c + 1);

    for (i = 0; i < n; ++i) {
        std::cout << " a(" << i << ") = " << a[i] << " b(" << i
                  << ") = " << b[i] << " c(" << i << ") = " << c[i] << " d("
                  << i << ") = " << d[i] << " e(" << i << ") = " << e[i]
                  << std::endl;
    }

    BOOST_CHECK(allocations == 0);

    return 0;
}
