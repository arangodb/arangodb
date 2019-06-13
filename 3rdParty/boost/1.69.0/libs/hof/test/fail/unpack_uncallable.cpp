/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    unpack_uncallable.cpp
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
    constexpr static auto apply(F&&, S&& s) BOOST_HOF_RETURNS(s.bar);
};
}

int main() {
    boost::hof::unpack(boost::hof::always(1))(foo{});
}
