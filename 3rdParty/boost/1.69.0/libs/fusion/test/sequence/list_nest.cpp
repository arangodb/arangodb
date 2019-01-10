/*=============================================================================
    Copyright (C) 2015 Kohei Takahshi

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/list/list.hpp>
#include <boost/core/lightweight_test.hpp>

#define FUSION_SEQUENCE boost::fusion::list
#include "nest.hpp"

/* list has a few issues:
    - sequence conversion constructor has bug when first element is a sequence
    - assignment sequence conversion has bug in base class */
template <typename T>
struct skip_issues
{
    template <typename Source, typename Expected>
    bool operator()(Source const& source, Expected const& expected) const
    {
        using namespace test_detail;
        return
            run< can_copy<T> >(source, expected) &&
            run< can_construct_from_elements<T> >(source, expected);
    }
};


int
main()
{
    test<skip_issues>();
    return boost::report_errors();
}

