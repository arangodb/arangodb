/*=============================================================================
    Copyright (c) 2014 Christoph Weiss

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/list/list.hpp>

#define FUSION_SEQUENCE list
#include "hash.hpp"

int main()
{
    hash_test();
    return boost::report_errors();
}
