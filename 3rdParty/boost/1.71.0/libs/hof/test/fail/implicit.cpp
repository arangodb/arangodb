/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    implicit.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/implicit.hpp>

template<class T>
struct auto_caster
{
    template<class U>
    T operator()(U x)
    {
        return T(x);
    }
};


int main()
{
    boost::hof::implicit<auto_caster> auto_cast = {};
    auto x = auto_cast(1.5);
    (void)x;
// This is not possible in c++17 due to guaranteed copy elison
#if BOOST_HOF_HAS_STD_17 || (defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ < 7)
    static_assert(false, "Always fail");
#endif
}
