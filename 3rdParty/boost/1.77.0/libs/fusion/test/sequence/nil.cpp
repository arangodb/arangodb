/*=============================================================================
    Copyright (c) 2014 Louis Dionne

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/container/list/cons_iterator.hpp>
#include <boost/fusion/container/list/nil.hpp>
#include <boost/fusion/support/config.hpp>
#include <boost/mpl/bool.hpp>


int main() {
    using namespace boost::fusion;

    // nil should be constexpr constructible
    {
        BOOST_CONSTEXPR nil x1 = nil();
        BOOST_CONSTEXPR nil x2 = nil(nil_iterator(), boost::mpl::true_());
        (void)x1; (void)x2;
    }

    return boost::report_errors();
}
