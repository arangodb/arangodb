// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ tarray
#include <boost/yap/algorithm.hpp>
#include <boost/yap/print.hpp>

#include <array>
#include <iostream>


template <boost::yap::expr_kind Kind, typename Tuple>
struct tarray_expr;


struct take_nth
{
    boost::yap::terminal<tarray_expr, int>
    operator() (boost::yap::terminal<tarray_expr, std::array<int, 3>> const & expr);

    std::size_t n;
};

// Another custom expression template.  In this case, we static_assert() that
// it only gets instantiated with terminals with pre-approved value types.
template <boost::yap::expr_kind Kind, typename Tuple>
struct tarray_expr
{
    // Make sure that, if this expression is a terminal, its value is one we
    // want to support.  Note that the presence of expr_kind::expr_ref makes
    // life slightly more difficult; we have to account for int const & and
    // int & as well as int.
    static_assert(
        Kind != boost::yap::expr_kind::terminal ||
        std::is_same<Tuple, boost::hana::tuple<int const &>>{} ||
        std::is_same<Tuple, boost::hana::tuple<int &>>{} ||
        std::is_same<Tuple, boost::hana::tuple<int>>{} ||
        std::is_same<Tuple, boost::hana::tuple<std::array<int, 3>>>{},
        "tarray_expr instantiated with an unsupported terminal type."
    );

    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;

    int operator[] (std::size_t n) const
    { return boost::yap::evaluate(boost::yap::transform(*this, take_nth{n})); }
};

// Define operators +, -, *, and /.
BOOST_YAP_USER_BINARY_OPERATOR(plus, tarray_expr, tarray_expr)
BOOST_YAP_USER_BINARY_OPERATOR(minus, tarray_expr, tarray_expr)
BOOST_YAP_USER_BINARY_OPERATOR(multiplies, tarray_expr, tarray_expr)
BOOST_YAP_USER_BINARY_OPERATOR(divides, tarray_expr, tarray_expr)


boost::yap::terminal<tarray_expr, int>
take_nth::operator() (boost::yap::terminal<tarray_expr, std::array<int, 3>> const & expr)
{
    int x = boost::yap::value(expr)[n];
    // Again, this is the move hack to get x into the resulting terminal as a
    // value instead of a reference.
    return boost::yap::make_terminal<tarray_expr>(std::move(x));
}


// Stream-out operators for the two kinds of terminals we support.

std::ostream & operator<< (std::ostream & os, boost::yap::terminal<tarray_expr, int> expr)
{ return os << '{' << boost::yap::value(expr) << '}'; }

std::ostream & operator<< (std::ostream & os, boost::yap::terminal<tarray_expr, std::array<int, 3>> expr)
{
    std::array<int, 3> const & a = boost::yap::value(expr);
    return os << '{' << a[0] << ", " << a[1] << ", " << a[2] << '}';
}

// Stream-out operators for general expressions.  Note that we have to treat
// the reference case separately; this also could have been done using
// constexpr if in a single function template.

template <typename Tuple>
std::ostream & operator<< (std::ostream & os, tarray_expr<boost::yap::expr_kind::expr_ref, Tuple> const & expr)
{ return os << boost::yap::deref(expr); }

template <boost::yap::expr_kind Kind, typename Tuple>
std::ostream & operator<< (std::ostream & os, tarray_expr<Kind, Tuple> const & expr)
{
    if (Kind == boost::yap::expr_kind::plus || Kind == boost::yap::expr_kind::minus)
        os << '(';
    os << boost::yap::left(expr) << " " << op_string(Kind) << " " << boost::yap::right(expr);
    if (Kind == boost::yap::expr_kind::plus || Kind == boost::yap::expr_kind::minus)
        os << ')';
    return os;
}


// Since we want different behavior on terminals than on other kinds of
// expressions, we create a custom type that does so.
struct tarray :
    tarray_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::array<int, 3>>
    >
{
    explicit tarray (int i = 0, int j = 0, int k = 0)
    {
        (*this)[0] = i;
        (*this)[1] = j;
        (*this)[2] = k;
    }

    explicit tarray (std::array<int, 3> a)
    {
        (*this)[0] = a[0];
        (*this)[1] = a[1];
        (*this)[2] = a[2];
    }

    int & operator[] (std::ptrdiff_t i)
    { return boost::yap::value(*this)[i]; }

    int const & operator[] (std::ptrdiff_t i) const
    { return boost::yap::value(*this)[i]; }

    template <typename T>
    tarray & operator= (T const & t)
    {
        // We use as_expr() here to make sure that the value passed to
        // assign() is an expression.  as_expr() simply forwards expressions
        // through, and wraps non-expressions as terminals.
        return assign(boost::yap::as_expr< ::tarray_expr>(t));
    }

    template <typename Expr>
    tarray & printAssign (Expr const & expr)
    {
        *this = expr;
        std::cout << *this << " = " << expr << std::endl;
        return *this;
    }

private:
    template <typename Expr>
    tarray & assign (Expr const & expr)
    {
        (*this)[0] = expr[0];
        (*this)[1] = expr[1];
        (*this)[2] = expr[2];
        return *this;
    }
};


int main()
{
    tarray a(3,1,2);

    tarray b;

    std::cout << a << std::endl;
    std::cout << b << std::endl;

    b[0] = 7; b[1] = 33; b[2] = -99;

    tarray c(a);

    std::cout << c << std::endl;

    a = 0;

    std::cout << a << std::endl;
    std::cout << b << std::endl;
    std::cout << c << std::endl;

    a = b + c;

    std::cout << a << std::endl;

    a.printAssign(b+c*(b + 3*c));

    return 0;
}
//]
