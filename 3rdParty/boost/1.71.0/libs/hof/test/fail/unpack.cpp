/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    unpack.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/unpack.hpp>

struct foo
{};

namespace boost { namespace hof {

template<>
struct unpack_sequence<foo>
{
    template<class F, class S>
    constexpr static int apply(F&&, S&&)
    {
        return 0;
    }
};
}

int main() {
    boost::hof::unpack(boost::hof::always(1))(foo{});
}
