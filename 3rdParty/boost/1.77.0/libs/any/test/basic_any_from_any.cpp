// Copyright (c) 2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/any.hpp>
#include <boost/any/basic_any.hpp>

int main()
{
    boost::any value = 42;
    boost::anys::basic_any<256, 16> bany = value;
}

