//
// Copyright (c) Chris Glover, 2016.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[type_index_runtime_cast_example
/*`
    The following example shows that `runtime_cast` is able to find a valid pointer
    in various class hierarchies regardless of inheritance or type relation.

    Example works with and without RTTI."
*/

#include <boost/type_index/runtime_cast.hpp>
#include <iostream>

struct A {
    BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS(BOOST_TYPE_INDEX_NO_BASE_CLASS)
    virtual ~A()
    {}
};

struct B {
    BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS(BOOST_TYPE_INDEX_NO_BASE_CLASS)
    virtual ~B()
    {}
};

struct C : A {
    BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS((A))
};

struct D : B {
    BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS((B))
};

struct E : C, D {
    BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS((C)(D))
};

int main() {
    C c;
    A* a = &c;

    if(C* cp = boost::typeindex::runtime_cast<C*>(a)) {
        std::cout << "Yes, a points to a C: "
                  << a << "->" << cp << '\n';
    }
    else {
        std::cout << "Error: Expected a to point to a C" << '\n';
    }

    if(E* ce = boost::typeindex::runtime_cast<E*>(a)) {
        std::cout << "Error: Expected a to not points to an E: "
                  << a << "->" << ce << '\n';
    }
    else {
        std::cout << "But, a does not point to an E" << '\n';
    }

    E e;
    C* cp2 = &e;
    if(D* dp = boost::typeindex::runtime_cast<D*>(cp2)) {
        std::cout << "Yes, we can cross-cast from a C* to a D* when we actually have an E: "
                  << cp2 << "->" << dp << '\n';
    }
    else {
        std::cout << "Error: Expected cp to point to a D" << '\n';
    }

/*`
    Alternatively, we can use runtime_pointer_cast so we don't need to specity the target as a pointer.
    This works for smart_ptr types too.
*/
    A* ap = &e;
    if(B* bp = boost::typeindex::runtime_pointer_cast<B>(ap)) {
        std::cout << "Yes, we can cross-cast and up-cast at the same time."
                  << ap << "->" << bp << '\n';
    }
    else {
        std::cout << "Error: Expected ap to point to a B" << '\n';
    }

    return 0;
}

//] [/type_index_runtime_cast_example]
