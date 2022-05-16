// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ self_evaluation
#include <boost/yap/expression.hpp>

#include <boost/optional.hpp>
#include <boost/hana/fold.hpp>
#include <boost/hana/maximum.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>


// A super-basic matrix type, and a few associated operations.
struct matrix
{
    matrix() : values_(), rows_(0), cols_(0) {}

    matrix(int rows, int cols) : values_(rows * cols), rows_(rows), cols_(cols)
    {
        assert(0 < rows);
        assert(0 < cols);
    }

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    double operator()(int r, int c) const
    { return values_[r * cols_ + c]; }
    double & operator()(int r, int c)
    { return values_[r * cols_ + c]; }

private:
    std::vector<double> values_;
    int rows_;
    int cols_;
};

matrix operator*(matrix const & lhs, double x)
{
    matrix retval = lhs;
    for (int i = 0; i < retval.rows(); ++i) {
        for (int j = 0; j < retval.cols(); ++j) {
            retval(i, j) *= x;
        }
    }
    return retval;
}
matrix operator*(double x, matrix const & lhs) { return lhs * x; }

matrix operator+(matrix const & lhs, matrix const & rhs)
{
    assert(lhs.rows() == rhs.rows());
    assert(lhs.cols() == rhs.cols());
    matrix retval = lhs;
    for (int i = 0; i < retval.rows(); ++i) {
        for (int j = 0; j < retval.cols(); ++j) {
            retval(i, j) += rhs(i, j);
        }
    }
    return retval;
}

// daxpy() means Double-precision AX Plus Y.  This crazy name comes from BLAS.
// It is more efficient than a naive implementation, because it does not
// create temporaries.  The covnention of using Y as an out-parameter comes
// from FORTRAN BLAS.
matrix & daxpy(double a, matrix const & x, matrix & y)
{
    assert(x.rows() == y.rows());
    assert(x.cols() == y.cols());
    for (int i = 0; i < y.rows(); ++i) {
        for (int j = 0; j < y.cols(); ++j) {
            y(i, j) += a * x(i, j);
        }
    }
    return y;
}

template <boost::yap::expr_kind Kind, typename Tuple>
struct self_evaluating_expr;

template <boost::yap::expr_kind Kind, typename Tuple>
auto evaluate_matrix_expr(self_evaluating_expr<Kind, Tuple> const & expr);

// This is the primary template for our expression template.  If you assign a
// self_evaluating_expr to a matrix, its conversion operator transforms and
// evaluates the expression with a call to evaluate_matrix_expr().
template <boost::yap::expr_kind Kind, typename Tuple>
struct self_evaluating_expr
{
    operator auto() const;

    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

// This is a specialization of our expression template for assignment
// expressions.  The destructor transforms and evaluates via a call to
// evaluate_matrix_expr(), and then assigns the result to the variable on the
// left side of the assignment.
//
// In a production implementation, you'd need to have specializations for
// plus_assign, minus_assign, etc.
template <typename Tuple>
struct self_evaluating_expr<boost::yap::expr_kind::assign, Tuple>
{
    ~self_evaluating_expr();

    static const boost::yap::expr_kind kind = boost::yap::expr_kind::assign;

    Tuple elements;
};

struct use_daxpy
{
    // A plus-expression, which may be of the form double * matrix + matrix,
    // or may be something else.  Since our daxpy() above requires a mutable
    // "y", we only need to match a mutable lvalue matrix reference here.
    template <typename Tuple>
    auto operator()(
        boost::yap::expr_tag<boost::yap::expr_kind::plus>,
        self_evaluating_expr<boost::yap::expr_kind::multiplies, Tuple> const & expr,
        matrix & m)
    {
        // Here, we transform the left-hand side into a pair if it's the
        // double * matrix operation we're looking for.  Otherwise, we just
        // get a copy of the left side expression.
        //
        // Note that this is a bit of a cheat, done for clarity.  If we pass a
        // larger expression that happens to contain a double * matrix
        // subexpression, that subexpression will be transformed into a tuple!
        // In production code, this transform should probably only be
        // performed on an expression with all terminal members.
        auto lhs = boost::yap::transform(
            expr,
            [](boost::yap::expr_tag<boost::yap::expr_kind::multiplies>,
               double d,
               matrix const & m) {
                return std::pair<double, matrix const &>(d, m);
            });

        // If we got back a copy of expr above, just re-construct the
        // expression this function mathes; in other words, do not effectively
        // transform anything.  Otherwise, replace the expression matched by
        // this function with a call to daxpy().
        if constexpr (boost::yap::is_expr<decltype(lhs)>::value) {
            return expr + m;
        } else {
            return boost::yap::make_terminal(daxpy)(lhs.first, lhs.second, m);
        }
    }
};


// This is the heart of what self_evaluating_expr does.  If we had other
// optimizations/transformations we wanted to do, we'd put them in this
// function, either before or after the use_daxpy transformation.
template <boost::yap::expr_kind Kind, typename Tuple>
auto evaluate_matrix_expr(self_evaluating_expr<Kind, Tuple> const & expr)
{
    auto daxpy_form = boost::yap::transform(expr, use_daxpy{});
    return boost::yap::evaluate(daxpy_form);
}

template<boost::yap::expr_kind Kind, typename Tuple>
self_evaluating_expr<Kind, Tuple>::operator auto() const
{
    return evaluate_matrix_expr(*this);
}

template<typename Tuple>
self_evaluating_expr<boost::yap::expr_kind::assign, Tuple>::
    ~self_evaluating_expr()
{
    using namespace boost::hana::literals;
    boost::yap::evaluate(elements[0_c]) = evaluate_matrix_expr(elements[1_c]);
}

// In order to define the = operator with the semantics we want, it's
// convenient to derive a terminal type from a terminal instantiation of
// self_evaluating_expr.  Note that we could have written a template
// specialization here instead -- either one would work.  That would of course
// have required more typing.
struct self_evaluating :
    self_evaluating_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<matrix>
    >
{
    self_evaluating() {}

    explicit self_evaluating(matrix m)
    { elements = boost::hana::tuple<matrix>(std::move(m)); }

    BOOST_YAP_USER_ASSIGN_OPERATOR(self_evaluating_expr, ::self_evaluating_expr);
};

BOOST_YAP_USER_BINARY_OPERATOR(plus, self_evaluating_expr, self_evaluating_expr)
BOOST_YAP_USER_BINARY_OPERATOR(minus, self_evaluating_expr, self_evaluating_expr)
BOOST_YAP_USER_BINARY_OPERATOR(multiplies, self_evaluating_expr, self_evaluating_expr)


int main()
{
    matrix identity(2, 2);
    identity(0, 0) = 1.0;
    identity(1, 1) = 1.0;

    // These are YAP-ified terminal expressions.
    self_evaluating m1(identity);
    self_evaluating m2(identity);
    self_evaluating m3(identity);

    // This transforms the YAP expression to use daxpy(), so it creates no
    // temporaries.  The transform happens in the destructor of the
    // assignment-expression specialization of self_evaluating_expr.
    m1 = 3.0 * m2 + m3;

    // Same as above, except that it uses the matrix conversion operator on
    // the self_evaluating_expr primary template, because here we're assigning
    // a YAP expression to a non-YAP-ified matrix.
    matrix m_result_1 = 3.0 * m2 + m3;

    // Creates temporaries and does not use daxpy(), because the A * X + Y
    // pattern does not occur within the expression.
    matrix m_result_2 = 3.0 * m2;

    return 0;
}
//]
