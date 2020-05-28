/*=============================================================================
    Copyright (c) 2014 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/fusion/adapted/boost_tuple.hpp>

#define FUSION_SEQUENCE boost::tuple
#define FUSION_TRAVERSAL_TAG forward_traversal_tag
#define FUSION_NO_PRIOR
#include "./iterator.hpp"

int
main()
{
    test();
    return boost::report_errors();
}

