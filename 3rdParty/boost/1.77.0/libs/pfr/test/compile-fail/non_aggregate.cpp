// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/core.hpp>
#include <boost/core/lightweight_test.hpp>

#include <tuple>

int main() {
    (void)boost::pfr::tuple_size<std::pair<int, short>>::value; // Must be a compile time error
    return boost::report_errors();
}


