/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC7_VM_HPP)
#define BOOST_SPIRIT_X3_CALC7_VM_HPP

#include <vector>

namespace client
{
    ///////////////////////////////////////////////////////////////////////////
    //  The Virtual Machine
    ///////////////////////////////////////////////////////////////////////////
    enum byte_code
    {
        op_neg,     //  negate the top stack entry
        op_add,     //  add top two stack entries
        op_sub,     //  subtract top two stack entries
        op_mul,     //  multiply top two stack entries
        op_div,     //  divide top two stack entries
        op_int,     //  push constant integer into the stack
    };

    class vmachine
    {
    public:

        vmachine(unsigned stackSize = 4096)
          : stack(stackSize)
          , stack_ptr(stack.begin())
        {
        }

        int top() const { return stack_ptr[-1]; };
        void execute(std::vector<int> const& code);

    private:

        std::vector<int> stack;
        std::vector<int>::iterator stack_ptr;
    };

}

#endif
