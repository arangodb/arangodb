// Copyright 2013-2019 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

//[type_index_derived_example
/*`
    The following example shows that `type_info` is able to store the real type, successfully getting through
    all the inheritances.

    Example works with and without RTTI."
*/

#include <boost/type_index.hpp>
#include <boost/type_index/runtime_cast/register_runtime_class.hpp>
#include <iostream>

struct A {
    BOOST_TYPE_INDEX_REGISTER_CLASS
    virtual ~A(){}
};
struct B: public A { BOOST_TYPE_INDEX_REGISTER_CLASS };
struct C: public B { BOOST_TYPE_INDEX_REGISTER_CLASS };
struct D: public C { BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS(BOOST_TYPE_INDEX_NO_BASE_CLASS) };

void print_real_type(const A& a) {
    std::cout << boost::typeindex::type_id_runtime(a).pretty_name() << '\n';
}

int main() {
    C c;
    const A& c_as_a = c;
    print_real_type(c_as_a);    // Outputs `struct C`
    print_real_type(B());       // Outputs `struct B`

/*`
    It's also possible to use type_id_runtime with the BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS, which adds additional
    information for runtime_cast to work.
*/
    D d;
    const A& d_as_a = d;
    print_real_type(d_as_a);    // Outputs `struct D`

}

//] [/type_index_derived_example]
