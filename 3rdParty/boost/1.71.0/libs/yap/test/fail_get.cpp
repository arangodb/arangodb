// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>


int main()
{
    boost::yap::get(boost::yap::make_terminal(1), boost::hana::llong_c<1>);
}
