/*=============================================================================
    Copyright (c) 2017 Nikita Kniazev

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/object.hpp>
#include <boost/phoenix/scope.hpp>
#include <boost/phoenix/operator.hpp>

#include <string>
#include <iostream>

template <typename T, std::size_t N>
struct array_holder
{
    typedef T underlying_type[N];

    array_holder(underlying_type const& x)
    {
        for (std::size_t i = 0; i < N; ++i) elems_[i] = x[i];
    }

    T elems_[N];

    friend std::ostream& operator<<(std::ostream& os, array_holder const& x)
    {
        os << x.elems_[0];
        for (std::size_t i = 1; i < N; ++i) os << ", " << x.elems_[i];
        return os;
    }
};

int main()
{
    using boost::phoenix::construct;
    using boost::phoenix::let;
    using boost::phoenix::arg_names::_1;
    using boost::phoenix::local_names::_a;

    let(_a = construct<std::string>("str"))
    [
        std::cout << _a << std::endl
    ]();

    let(_a = construct<array_holder<char, 4> >("str"))
    [
        std::cout << _a << std::endl
    ]();

    int ints[] = { 1, 2, 3 };
    let(_a = construct<array_holder<int, sizeof(ints) / sizeof(ints[0])> >(ints) )
    [
        std::cout << _a << std::endl
    ]();

    return 0;
}
