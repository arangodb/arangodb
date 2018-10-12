// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

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


template<boost::yap::expr_kind Kind, typename Tuple>
struct lazy_vector_expr;


struct take_nth
{
    boost::yap::terminal<lazy_vector_expr, double> operator()(
        boost::yap::terminal<lazy_vector_expr, std::vector<double>> const &
            expr);

    std::size_t n;
};

template<boost::yap::expr_kind Kind, typename Tuple>
struct lazy_vector_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;

    auto operator[](std::size_t n) const
    {
        return boost::yap::evaluate(boost::yap::transform(*this, take_nth{n}));
    }
};

BOOST_YAP_USER_BINARY_OPERATOR(plus, lazy_vector_expr, lazy_vector_expr)
BOOST_YAP_USER_BINARY_OPERATOR(minus, lazy_vector_expr, lazy_vector_expr)

boost::yap::terminal<lazy_vector_expr, double> take_nth::operator()(
    boost::yap::terminal<lazy_vector_expr, std::vector<double>> const & expr)
{
    double x = boost::yap::value(expr)[n];
    return boost::yap::make_terminal<lazy_vector_expr, double>(std::move(x));
}

struct lazy_vector : lazy_vector_expr<
                         boost::yap::expr_kind::terminal,
                         boost::hana::tuple<std::vector<double>>>
{
    lazy_vector() {}

    explicit lazy_vector(std::vector<double> && vec)
    {
        elements = boost::hana::tuple<std::vector<double>>(std::move(vec));
    }

    template<boost::yap::expr_kind Kind, typename Tuple>
    lazy_vector & operator+=(lazy_vector_expr<Kind, Tuple> const & rhs)
    {
        std::vector<double> & this_vec = boost::yap::value(*this);
        for (int i = 0, size = (int)this_vec.size(); i < size; ++i) {
            this_vec[i] += rhs[i];
        }
        return *this;
    }
};


int test_main(int, char * [])
{
    lazy_vector v1{std::vector<double>(4, 1.0)};
    lazy_vector v2{std::vector<double>(4, 2.0)};
    lazy_vector v3{std::vector<double>(4, 3.0)};

    // Reset allocation count.  There should be none from this point on.
    allocations = 0;

    double d1 = (v2 + v3)[2];
    std::cout << d1 << "\n";

    v1 += v2 - v3;
    std::cout << '{' << v1[0] << ',' << v1[1] << ',' << v1[2] << ',' << v1[3]
              << '}' << "\n";

    BOOST_CHECK(allocations == 0);

    return 0;
}
