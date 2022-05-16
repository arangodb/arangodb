/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman
    Copyright (c) 2013-2014 Agustin Berge

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  A Calculator example demonstrating generation of AST. The AST,
//  once created, is traversed, 1) To print its contents and
//  2) To evaluate the result.
//
//  [ JDG April 28, 2008 ]      For BoostCon 2008
//  [ JDG February 18, 2011 ]   Pure attributes. No semantic actions.
//  [ JDG January 9, 2013 ]     Spirit X3
//
///////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
# pragma warning(disable: 4345)
#endif

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

#include <list>
#include <numeric>

namespace x3 = boost::spirit::x3;

namespace client { namespace ast
{
    ///////////////////////////////////////////////////////////////////////////
    //  The AST
    ///////////////////////////////////////////////////////////////////////////
    struct nil {};
    struct signed_;
    struct program;

    typedef x3::variant<
            nil
          , unsigned int
          , x3::forward_ast<signed_>
          , x3::forward_ast<program>
        >
    operand;

    struct signed_
    {
        char sign;
        operand operand_;
    };

    struct operation
    {
        char operator_;
        operand operand_;
    };

    struct program
    {
        operand first;
        std::list<operation> rest;
    };
}}

BOOST_FUSION_ADAPT_STRUCT(client::ast::signed_,
    sign, operand_
)

BOOST_FUSION_ADAPT_STRUCT(client::ast::operation,
    operator_, operand_
)

BOOST_FUSION_ADAPT_STRUCT(client::ast::program,
    first, rest
)

namespace client { namespace ast
{
    ///////////////////////////////////////////////////////////////////////////
    //  The AST Printer
    ///////////////////////////////////////////////////////////////////////////
    struct printer
    {
        typedef void result_type;

        void operator()(nil) const {}
        void operator()(unsigned int n) const { std::cout << n; }

        void operator()(operation const& x) const
        {
            boost::apply_visitor(*this, x.operand_);
            switch (x.operator_)
            {
                case '+': std::cout << " add"; break;
                case '-': std::cout << " subt"; break;
                case '*': std::cout << " mult"; break;
                case '/': std::cout << " div"; break;
            }
        }

        void operator()(signed_ const& x) const
        {
            boost::apply_visitor(*this, x.operand_);
            switch (x.sign)
            {
                case '-': std::cout << " neg"; break;
                case '+': std::cout << " pos"; break;
            }
        }

        void operator()(program const& x) const
        {
            boost::apply_visitor(*this, x.first);
            for (operation const& oper: x.rest)
            {
                std::cout << ' ';
                (*this)(oper);
            }
        }
    };

    ///////////////////////////////////////////////////////////////////////////
    //  The AST evaluator
    ///////////////////////////////////////////////////////////////////////////
    struct eval
    {
        typedef int result_type;

        int operator()(nil) const { BOOST_ASSERT(0); return 0; }
        int operator()(unsigned int n) const { return n; }

        int operator()(int lhs, operation const& x) const
        {
            int rhs = boost::apply_visitor(*this, x.operand_);
            switch (x.operator_)
            {
                case '+': return lhs + rhs;
                case '-': return lhs - rhs;
                case '*': return lhs * rhs;
                case '/': return lhs / rhs;
            }
            BOOST_ASSERT(0);
            return 0;
        }

        int operator()(signed_ const& x) const
        {
            int rhs = boost::apply_visitor(*this, x.operand_);
            switch (x.sign)
            {
                case '-': return -rhs;
                case '+': return +rhs;
            }
            BOOST_ASSERT(0);
            return 0;
        }

        int operator()(program const& x) const
        {
            return std::accumulate( x.rest.begin(), x.rest.end()
                                    , boost::apply_visitor(*this, x.first)
                                    , *this);
        }
    };
}}

namespace client
{
    ///////////////////////////////////////////////////////////////////////////////
    //  The calculator grammar
    ///////////////////////////////////////////////////////////////////////////////
    namespace calculator_grammar
    {
        using parser_type =
            x3::any_parser<
                std::string::const_iterator
              , ast::program
              , decltype(x3::make_context<x3::skipper_tag>(x3::ascii::space))
            >;

        parser_type calculator();
    }
    
    auto const calculator = calculator_grammar::calculator();
}
