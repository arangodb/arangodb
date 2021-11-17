// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ mixed
#include <boost/yap/yap.hpp>

#include <complex>
#include <list>
#include <vector>
#include <iostream>


// This wrapper makes the pattern matching in transforms below (like deref and
// incr) a lot easier to write.
template <typename Iter>
struct iter_wrapper
{
    Iter it;
};

template <typename Iter>
auto make_iter_wrapper (Iter it)
{ return iter_wrapper<Iter>{it}; }


// A container -> wrapped-begin transform.
struct begin
{
    template <typename Cont>
    auto operator() (boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
                     Cont const & cont)
        -> decltype(boost::yap::make_terminal(make_iter_wrapper(cont.begin())))
    { return boost::yap::make_terminal(make_iter_wrapper(cont.begin())); }
};

// A wrapped-iterator -> dereferenced value transform.
struct deref
{
    template <typename Iter>
    auto operator() (boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
                     iter_wrapper<Iter> wrapper)
        -> decltype(boost::yap::make_terminal(*wrapper.it))
    { return boost::yap::make_terminal(*wrapper.it); }
};

// A wrapped-iterator increment transform, using side effects.
struct incr
{
    template <typename Iter>
    auto operator() (boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
                     iter_wrapper<Iter> & wrapper)
        -> decltype(boost::yap::make_terminal(wrapper.it))
    {
        ++wrapper.it;
        // Since this transform is valuable for its side effects, and thus the
        // result of the transform is ignored, we could return anything here.
        return boost::yap::make_terminal(wrapper.it);
    }
};


// The implementation of elementwise evaluation of expressions of sequences;
// all the later operations use this one.
template <
    template <class, class> class Cont,
    typename T,
    typename A,
    typename Expr,
    typename Op
>
Cont<T, A> & op_assign (Cont<T, A> & cont, Expr const & e, Op && op)
{
    decltype(auto) expr = boost::yap::as_expr(e);
    // Transform the expression of sequences into an expression of
    // begin-iterators.
    auto expr2 = boost::yap::transform(boost::yap::as_expr(expr), begin{});
    for (auto && x : cont) {
        // Transform the expression of iterators into an expression of
        // pointed-to-values, evaluate the resulting expression, and call op()
        // with the result of the evaluation.
        op(x, boost::yap::evaluate(boost::yap::transform(expr2, deref{})));
        // Transform the expression of iterators into an ignored value; as a
        // side effect, increment the iterators in the expression.
        boost::yap::transform(expr2, incr{});
    }
    return cont;
}

template <
    template <class, class> class Cont,
    typename T,
    typename A,
    typename Expr
>
Cont<T, A> & assign (Cont<T, A> & cont, Expr const & expr)
{
    return op_assign(cont, expr, [](auto & cont_value, auto && expr_value) {
        cont_value = std::forward<decltype(expr_value)>(expr_value);
    });
}

template <
    template <class, class> class Cont,
    typename T,
    typename A,
    typename Expr
>
Cont<T, A> & operator+= (Cont<T, A> & cont, Expr const & expr)
{
    return op_assign(cont, expr, [](auto & cont_value, auto && expr_value) {
        cont_value += std::forward<decltype(expr_value)>(expr_value);
    });
}

template <
    template <class, class> class Cont,
    typename T,
    typename A,
    typename Expr
>
Cont<T, A> & operator-= (Cont<T, A> & cont, Expr const & expr)
{
    return op_assign(cont, expr, [](auto & cont_value, auto && expr_value) {
        cont_value -= std::forward<decltype(expr_value)>(expr_value);
    });
}

// A type trait that identifies std::vectors and std::lists.
template <typename T>
struct is_mixed : std::false_type {};

template <typename T, typename A>
struct is_mixed<std::vector<T, A>> : std::true_type {};

template <typename T, typename A>
struct is_mixed<std::list<T, A>> : std::true_type {};

// Define expression-producing operators over std::vectors and std::lists.
BOOST_YAP_USER_UDT_UNARY_OPERATOR(negate, boost::yap::expression, is_mixed); // -
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(multiplies, boost::yap::expression, is_mixed); // *
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(divides, boost::yap::expression, is_mixed); // /
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(modulus, boost::yap::expression, is_mixed); // %
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(plus, boost::yap::expression, is_mixed); // +
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(minus, boost::yap::expression, is_mixed); // -
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(less, boost::yap::expression, is_mixed); // <
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(greater, boost::yap::expression, is_mixed); // >
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(less_equal, boost::yap::expression, is_mixed); // <=
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(greater_equal, boost::yap::expression, is_mixed); // >=
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(equal_to, boost::yap::expression, is_mixed); // ==
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(not_equal_to, boost::yap::expression, is_mixed); // !=
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(logical_or, boost::yap::expression, is_mixed); // ||
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(logical_and, boost::yap::expression, is_mixed); // &&
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(bitwise_and, boost::yap::expression, is_mixed); // &
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(bitwise_or, boost::yap::expression, is_mixed); // |
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(bitwise_xor, boost::yap::expression, is_mixed); // ^

// Define a type that can resolve to any overload of std::sin().
struct sin_t
{
    template<typename T>
    T operator()(T x)
    {
        return std::sin(x);
    }
};

int main()
{
    int n = 10;
    std::vector<int> a,b,c,d;
    std::list<double> e;
    std::list<std::complex<double>> f;

    int i;
    for(i = 0;i < n; ++i)
    {
        a.push_back(i);
        b.push_back(2*i);
        c.push_back(3*i);
        d.push_back(i);
        e.push_back(0.0);
        f.push_back(std::complex<double>(1.0, 1.0));
    }

    assign(b, 2);
    assign(d, a + b * c);
    a += if_else(d < 30, b, c);

    assign(e, c);
    e += e - 4 / (c + 1);

    auto sin = boost::yap::make_terminal(sin_t{});
    f -= sin(0.1 * e * std::complex<double>(0.2, 1.2));

    std::list<double>::const_iterator ei = e.begin();
    std::list<std::complex<double>>::const_iterator fi = f.begin();
    for (i = 0; i < n; ++i)
    {
        std::cout
            << "a(" << i << ") = " << a[i]
            << " b(" << i << ") = " << b[i]
            << " c(" << i << ") = " << c[i]
            << " d(" << i << ") = " << d[i]
            << " e(" << i << ") = " << *ei++
            << " f(" << i << ") = " << *fi++
            << std::endl;
    }

    return 0;
}
//]
