//
// Copyright (C) 2018 James E. King III
// 
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/dynamic_bitset.hpp>
#include <boost/core/lightweight_test.hpp>

int main(int, char*[])
{
    boost::dynamic_bitset<> x(5); // all 0's by default
    x.set(1, 2);
    x.set(3, 1, true);
    x.set(2, 1, false);
    BOOST_TEST(!x.test(0));
    BOOST_TEST( x.test(1));
    BOOST_TEST(!x.test(2));
    BOOST_TEST( x.test(3));
    BOOST_TEST(!x.test(4));

    return boost::report_errors();
}
