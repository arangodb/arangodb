/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include "compiler.hpp"
#include "vm.hpp"

namespace client
{
    void compiler::operator()(ast::nil) const
    {
        BOOST_ASSERT(0);
    }

    void compiler::operator()(unsigned int n) const
    {
        code.push_back(op_int);
        code.push_back(n);
    }

    void compiler::operator()(ast::operation const& x) const
    {
        boost::apply_visitor(*this, x.operand_);
        switch (x.operator_)
        {
            case '+': code.push_back(op_add); break;
            case '-': code.push_back(op_sub); break;
            case '*': code.push_back(op_mul); break;
            case '/': code.push_back(op_div); break;
            default: BOOST_ASSERT(0); break;
        }
    }

    void compiler::operator()(ast::signed_ const& x) const
    {
        boost::apply_visitor(*this, x.operand_);
        switch (x.sign)
        {
            case '-': code.push_back(op_neg); break;
            case '+': break;
            default: BOOST_ASSERT(0); break;
        }
    }

    void compiler::operator()(ast::expression const& x) const
    {
        boost::apply_visitor(*this, x.first);
        for (ast::operation const& oper : x.rest)
        {
            (*this)(oper);
        }
    }
}
