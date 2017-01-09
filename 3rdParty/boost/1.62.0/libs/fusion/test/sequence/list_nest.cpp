/*=============================================================================
    Copyright (C) 2015 Kohei Takahshi

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/list/list.hpp>
#include <boost/core/lightweight_test.hpp>

#define FUSION_SEQUENCE list
#include "nest.hpp"

int
main()
{
    test();
    return boost::report_errors();
}

