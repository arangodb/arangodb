// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <string>
#include <vector>


#define user_expr user_expr_1

/// [USER_UNARY_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

// Operator overloads for operator!().
BOOST_YAP_USER_UNARY_OPERATOR(logical_not, user_expr, user_expr)
/// [USER_UNARY_OPERATOR]

struct lazy_vector_1 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef user_expr

#define user_expr user_expr_2

/// [USER_BINARY_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

// Operator overloads for operator&&()
BOOST_YAP_USER_BINARY_OPERATOR(logical_and, user_expr, user_expr)
/// [USER_BINARY_OPERATOR]

struct lazy_vector_2 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef user_expr

#define user_expr user_expr_3

/// [USER_CALL_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;

    // Member operator overloads for operator()().  These will match any
    // number of parameters.  Each one can be any type, even another
    // expression.
    BOOST_YAP_USER_CALL_OPERATOR(::user_expr)
};
/// [USER_CALL_OPERATOR]

struct lazy_vector_3 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef user_expr

#define user_expr user_expr_4

/// [USER_SUBSCRIPT_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;

    // Member operator overloads for operator[]().  These will match any value
    // on the right-hand side, even another expression.
    BOOST_YAP_USER_SUBSCRIPT_OPERATOR(::user_expr)
};

/// [USER_SUBSCRIPT_OPERATOR]

struct lazy_vector_4 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef user_expr

#define user_expr user_expr_5

/// [USER_EXPR_IF_ELSE]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

// Defines an overload of if_else() that returns expressions instantiated from
// user_expr.
BOOST_YAP_USER_EXPR_IF_ELSE(::user_expr)
/// [USER_EXPR_IF_ELSE]

struct lazy_vector_5 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef user_expr

#define user_expr user_expr_6
#define is_vector is_vector_1

/// [USER_UDT_ANY_IF_ELSE]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

// Defines an overload of if_else() that returns expressions instantiated from
// user_expr.
BOOST_YAP_USER_UDT_ANY_IF_ELSE(::user_expr, is_vector)
/// [USER_UDT_ANY_IF_ELSE]

struct lazy_vector_6 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef is_vector
#undef user_expr

#define user_expr user_expr_7
#define is_vector is_vector_2

/// [USER_UDT_UNARY_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

// Defines an overload of operator!() that applies to vectors and returns
// expressions instantiated from user_expr.
BOOST_YAP_USER_UDT_UNARY_OPERATOR(logical_not, ::user_expr, is_vector)
/// [USER_UDT_UNARY_OPERATOR]

struct lazy_vector_7 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef is_vector
#undef user_expr

#define user_expr user_expr_8
#define is_vector is_vector_3

/// [USER_UDT_UDT_BINARY_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

template <typename T>
struct is_string : std::false_type {};

template <>
struct is_string<std::string> : std::true_type {};

// Defines an overload of operator||() that applies to vectors on the left and
// strings on the right, and returns expressions instantiated from user_expr.
BOOST_YAP_USER_UDT_UDT_BINARY_OPERATOR(logical_or, ::user_expr, is_vector, is_string)

// Defines an overload of operator&&() that applies to strings and returns
// expressions instantiated from user_expr.
BOOST_YAP_USER_UDT_UDT_BINARY_OPERATOR(logical_and, ::user_expr, is_string, is_string)
/// [USER_UDT_UDT_BINARY_OPERATOR]

struct lazy_vector_8 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef is_vector
#undef user_expr

#define user_expr user_expr_9
#define is_vector is_vector_4

/// [USER_UDT_ANY_BINARY_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

// Defines an overload of operator&&() that matches a vector on either side,
// and any type (possibly a vector) on the other.
BOOST_YAP_USER_UDT_ANY_BINARY_OPERATOR(logical_and, ::user_expr, is_vector)
/// [USER_UDT_ANY_BINARY_OPERATOR]

struct lazy_vector_9 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef is_vector
#undef user_expr

#define user_expr user_expr_10

/// [USER_LITERAL_PLACEHOLDER_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;
};

namespace literals {

    // Defines a user literal operator that makes placeholders; 2_p will be a
    // 2-placeholder instantiated from the user_expr template.
    BOOST_YAP_USER_LITERAL_PLACEHOLDER_OPERATOR(user_expr)

}
/// [USER_LITERAL_PLACEHOLDER_OPERATOR]

struct lazy_vector_10 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef user_expr

#define user_expr user_expr_11

/// [USER_ASSIGN_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;

    // Member operator overloads for operator=().  These will match any value
    // on the right-hand side, even another expression, except that it will
    // not conflict with the asignment or move assignment operators.
    BOOST_YAP_USER_ASSIGN_OPERATOR(user_expr, ::user_expr)
};
/// [USER_ASSIGN_OPERATOR]

struct lazy_vector_11 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};

#undef user_expr

#undef user_expr

#define user_expr user_expr_12

/// [USER_CALL_OPERATOR]
template <boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static const boost::yap::expr_kind kind = Kind;

    Tuple elements;

    // Member operator overloads for operator()().  These will take exactly N
    // parameters.  Each one can be any type, even another expression.
    BOOST_YAP_USER_CALL_OPERATOR(::user_expr)
};
/// [USER_CALL_OPERATOR]

struct lazy_vector_12 :
    user_expr<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<std::vector<double>>
    >
{};


int main ()
{
    lazy_vector_1 v1;
    lazy_vector_2 v2;
    lazy_vector_3 v3;
    lazy_vector_4 v4;
    lazy_vector_5 v5;
    lazy_vector_6 v6;
    lazy_vector_7 v7;
    lazy_vector_8 v8;
    lazy_vector_9 v9;
    lazy_vector_10 v10;
    lazy_vector_11 v11;
    lazy_vector_12 v12;

    return 0;
}
