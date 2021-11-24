// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//
// For more information, see http://www.boost.org/libs/range/
//
// As reported in https://groups.google.com/forum/#!msg/boost-developers-archive/6JVNg7ZPb4k/RAlvPUec4MAJ

#include <boost/range/iterator_range_core.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/algorithm/string.hpp>

namespace boost
{
    enum {unnamed};
    struct S { 
        bool operator<(int) const {return false;}
        bool operator==(int) const {return false;}
    };
    template<typename T>
    bool foo(T i)
    {
        return i < unnamed || i == unnamed;
    }
}

int main()
{
    using boost::lambda::_1;
    (void)(_1 == 42);
    (void)(42 == _1);

    boost::foo(42);
    boost::foo(boost::S());
}
