/*=============================================================================
    Copyright (c) 2016 Lee Clagett

    Use modification and distribution are subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
==============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/container/vector.hpp>

#define FUSION_SEQUENCE boost::fusion::vector
#include "conversion.hpp"

int main()
{
    test<test_detail::can_copy>();
    return boost::report_errors();
}
