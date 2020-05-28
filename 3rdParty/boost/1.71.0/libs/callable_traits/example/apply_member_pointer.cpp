/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

//[ apply_member_pointer
#include <type_traits>
#include <boost/callable_traits/apply_member_pointer.hpp>

namespace ct = boost::callable_traits;

struct foo;
struct bar;

int main() {

    {
        // function type -> member function pointer type
        using f = int(int);
        using g = ct::apply_member_pointer_t<f, foo>;
        using expect = int(foo::*)(int);
        static_assert(std::is_same<g, expect>::value, "");
    }

    {
        // function pointer type (unqualified) -> member function pointer type
        using f = int(*)();
        using g = ct::apply_member_pointer_t<f, foo>;
        using expect = int(foo::*)();
        static_assert(std::is_same<g, expect>::value, "");
    }


    {
        // function pointer type (qualified) -> member data pointer type

        // Look out for cases like these two. If the input type to apply_member_pointer
        // is a ``[*qualified]`` function pointer type, then the  aliased type is a member data
        // pointer to a ``[*qualified function pointer]``.

        {
            using f = int(*&)();
            using g = ct::apply_member_pointer_t<f, foo>;
            using expect = int (* foo::*)();
            static_assert(std::is_same<g, expect>::value, "");
        }

        {
            using f = int(* const)();
            using g = ct::apply_member_pointer_t<f, foo>;
            using expect = int (* const foo::*)();
            static_assert(std::is_same<g, expect>::value, "");
        }
    }

    {
        // function reference type -> member function pointer type
        using f = void(&)();
        using g = ct::apply_member_pointer_t<f, foo>;
        using expect = void(foo::*)();
        static_assert(std::is_same<g, expect>::value, "");
    }

    {
        // member function pointer type -> member function pointer type
        // (note the different parent class)
        using f = int(bar::*)() const;
        using g = ct::apply_member_pointer_t<f, foo>;
        using expect = int(foo::*)() const;
        static_assert(std::is_same<g, expect>::value, "");
    }

    {
        // non-callable type -> member data pointer type
        using g = ct::apply_member_pointer_t<int, foo>;
        using expect = int foo::*;
        static_assert(std::is_same<g, expect>::value, "");
    }


    {
        // function object type -> member data pointer type
        // the same is true for lambdas and generic lambdas
        auto lambda = [](){};
        using f = decltype(lambda);
        using g = ct::apply_member_pointer_t<f, foo>;
        using expect = f foo::*;
        static_assert(std::is_same<g, expect>::value, "");
    }
}
//]

