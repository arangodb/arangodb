/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    always.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/always.hpp>
#include <memory>

int main() {
    auto f = boost::hof::always(std::unique_ptr<int>{new int(1)});
    auto i = f(1, 2, 3);
    (void)i;
}
