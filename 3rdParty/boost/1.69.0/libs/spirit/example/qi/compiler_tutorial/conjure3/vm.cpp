/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include "config.hpp"
#include "vm.hpp"
#include <iostream>

#if defined(_MSC_VER)
# pragma warning(disable: 4800) // forcing value to bool 'true' or 'false'
                                // (performance warning)
#endif

namespace client
{
    vmachine::vmachine()
    {
        llvm::InitializeNativeTarget();
        llvm::LLVMContext& context = llvm::getGlobalContext();

        // Make the module, which holds all the code.
        module_ = new llvm::Module("Conjure JIT", context);

        // Create the JIT.  This takes ownership of the module.
        std::string error;
        execution_engine_ =
            llvm::EngineBuilder(module_).setErrorStr(&error).create();

        BOOST_ASSERT(execution_engine_ != 0);
        if (!execution_engine_)
        {
            std::cerr <<
                "Could not create ExecutionEngine: " <<
                error << std::endl;

            exit(1);
        }
    }
}


