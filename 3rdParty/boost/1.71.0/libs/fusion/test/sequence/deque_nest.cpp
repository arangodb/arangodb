/*=============================================================================
    Copyright (C) 2015 Kohei Takahshi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/deque/deque.hpp>
#include <boost/core/lightweight_test.hpp>

#define FUSION_SEQUENCE boost::fusion::deque
#include "nest.hpp"

/* deque has a few issues:
    - sequence conversion constructor is explicit
    - assignment sequence conversion has bug in base class
    - c++11 direct assignment from lvalue has bug */
template <typename T>
struct skip_issues
{
    template <typename Source, typename Expected>
    bool operator()(Source const& source, Expected const& expected) const
    {
        using namespace test_detail;
        return
#if defined(BOOST_FUSION_HAS_VARIADIC_DEQUE)
            run< can_construct<T> >(source, expected) &&
            run< can_implicit_construct<T> >(source, expected) &&
            run< can_rvalue_assign<T> >(source, expected) &&
            run< can_convert_using<can_construct>::to<T> >(source, expected) &&
#else
            run< can_copy<T> >(source, expected) &&
#endif
            run< can_construct_from_elements<T> >(source, expected);
    }
};

int
main()
{
    test<skip_issues>();
    return boost::report_errors();
}

