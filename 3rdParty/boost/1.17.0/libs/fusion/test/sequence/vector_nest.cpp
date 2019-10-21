/*=============================================================================
    Copyright (C) 2015 Kohei Takahshi

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/core/lightweight_test.hpp>

#define FUSION_SEQUENCE boost::fusion::vector
#include "nest.hpp"

int
main()
{
    test<test_detail::can_nest>();
    return boost::report_errors();
}

