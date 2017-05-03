// Copyright 2013-2015 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

//[type_index_exact_type_match_example
/*`
    The following example shows that `type_index` (and `type_info`) is able to store the exact type, 
    without stripping const, volatile and references. Example works with and without RTTI.

    In this example we'll create a class that stores a pointer to function and remembers the exact type of the 
    parameter the function accepts. When the call to the bound function is made, he actual input parameter 
    type is checked against the stored parameter type and an exception is thrown in case of mismatch.
*/

#include <boost/type_index.hpp>
#include <iostream>
#include <stdexcept>
//<-
// Making `#include <cassert>` visible in docs, while actually using hand-made check
// instead of `assert`. This is required to verify correct behavior even if NDEBUG
// is defined and to avoid `unused local variable` warnings with defined NDEBUG.
#ifdef assert
#   undef assert
#endif
#define assert(X) if (!(X)) std::exit(__LINE__)
    /* !Comment block is not closed intentionaly!
//->
#include <cassert>
//<-
    !Closing comment block! */
//->

class type_erased_unary_function {
    void*                           function_ptr_;
    boost::typeindex::type_index    exact_param_t_;

public:
    template <class ParamT>
    type_erased_unary_function(void(*ptr)(ParamT)) 
        : function_ptr_(reinterpret_cast<void*>(ptr)) // ptr - is a pointer to function returning `void` and accepting parameter of type `ParamT`
        , exact_param_t_(boost::typeindex::type_id_with_cvr<ParamT>())
    {}

    template <class ParamT>
    void call(ParamT v) {
        if (exact_param_t_ != boost::typeindex::type_id_with_cvr<ParamT>()) {
            throw std::runtime_error("Incorrect `ParamT`");
        }

        return (reinterpret_cast<void(*)(ParamT)>(function_ptr_))(v);
    }
};

void foo(int){}

int main() {
    type_erased_unary_function func(&foo);
    func.call(100); // OK, `100` has type `int`

    try {
        int i = 100; 
        
        // An attempt to convert stored function to a function accepting reference
        func.call<int&>(i); // Will throw, because types `int&` and `int` mismatch

        assert(false);
    } catch (const std::runtime_error& /*e*/) {}
}

//] [/type_index_exact_type_match_example]
