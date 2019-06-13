/*=============================================================================
    Copyright (C) 2016 Lee Clagett

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/container/vector.hpp>

#define FUSION_SEQUENCE boost::fusion::vector
#define FUSION_ALT_SEQUENCE boost::fusion::list
#include "traits.hpp"

int main() {
    test_convertible(true /* has conversion construction */ );

    // C++11 models overly aggressive (bug) implicit conversion from C++03
    BOOST_TEST((
        is_convertible<
            boost::fusion::list<int>
          , boost::fusion::vector< boost::fusion::list<int> >
        >(true)
    ));

#if defined(FUSION_TEST_HAS_CONSTRUCTIBLE)
    test_constructible();
#endif

    return boost::report_errors();
}
