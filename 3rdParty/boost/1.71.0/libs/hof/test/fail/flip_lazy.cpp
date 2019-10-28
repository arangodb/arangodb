/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    flip_lazy.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/lazy.hpp>
#include <boost/hof/placeholders.hpp>
#include <boost/hof/flip.hpp>

int main() {
    auto i = (boost::hof::flip(boost::hof::_1 - boost::hof::_2) * boost::hof::_1)(3, 6);
    (void)i;
}
