/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_CONJURE_VM_HPP)
#define BOOST_SPIRIT_CONJURE_VM_HPP

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetSelect.h>

#include <boost/assert.hpp>

namespace client
{
    class vmachine;

    ///////////////////////////////////////////////////////////////////////////
    //  A light wrapper to a function pointer returning int and accepting
    //  from 0 to 3 arguments, where arity is determined at runtime.
    ///////////////////////////////////////////////////////////////////////////
    class function
    {
    public:

        typedef int result_type;

        int operator()() const
        {
            BOOST_ASSERT(fptr != 0);
            BOOST_ASSERT(arity() == 0);
            int (*fp)() = (int(*)())(intptr_t)fptr;
            return fp();
        }

        int operator()(int _1) const
        {
            BOOST_ASSERT(fptr != 0);
            BOOST_ASSERT(arity() == 1);
            int (*fp)(int) = (int(*)(int))(intptr_t)fptr;
            return fp(_1);
        }

        int operator()(int _1, int _2) const
        {
            BOOST_ASSERT(fptr != 0);
            BOOST_ASSERT(arity() == 2);
            int (*fp)(int, int) = (int(*)(int, int))(intptr_t)fptr;
            return fp(_1, _2);
        }

        int operator()(int _1, int _2, int _3) const
        {
            BOOST_ASSERT(fptr != 0);
            BOOST_ASSERT(arity() == 3);
            int (*fp)(int, int, int) = (int(*)(int, int, int))(intptr_t)fptr;
            return fp(_1, _2, _3);
        }

        unsigned arity() const { return arity_; }
        bool operator!() const { return fptr == 0; }

    private:

        friend class vmachine;
        function(void* fptr, unsigned arity_)
            : fptr(fptr), arity_(arity_) {}

        void* fptr;
        unsigned arity_;
    };

    ///////////////////////////////////////////////////////////////////////////
    //  The Virtual Machine (light wrapper over LLVM JIT)
    ///////////////////////////////////////////////////////////////////////////
    class vmachine
    {
    public:

        vmachine();

        llvm::Module* module() const
        {
            return module_;
        }

        llvm::ExecutionEngine* execution_engine() const
        {
            return execution_engine_;
        }

        void print_assembler() const
        {
            module_->dump();
        }

        function get_function(char const* name)
        {
            llvm::Function* callee = module_->getFunction(name);
            if (callee == 0)
                return function(0, 0);

            // JIT the function
            void *fptr = execution_engine_->getPointerToFunction(callee);
            return function(fptr, callee->arg_size());
        }

    private:

        llvm::Module* module_;
        llvm::ExecutionEngine* execution_engine_;
    };
}

#endif

