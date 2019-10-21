/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    apply_eval.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/apply_eval.hpp>

int main() {
    (void)boost::hof::apply_eval(boost::hof::always(), 1, 2);
}
