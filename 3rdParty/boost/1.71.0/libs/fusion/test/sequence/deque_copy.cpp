/*=============================================================================
    Copyright (c) 1999-2003 Jaakko Jarvi
    Copyright (c) 2001-2011 Joel de Guzman
    Copyright (c) 2006 Dan Marsden

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/deque/deque.hpp>
#include <boost/fusion/container/generation/make_deque.hpp>
#include <boost/fusion/container/generation/deque_tie.hpp>

#define FUSION_SEQUENCE deque
#include "copy.hpp"

using namespace test_detail;

// c++11 deque has bug, cannot properly copy-assign from a const value
template <typename T>
struct skip_const_lvalue_assignment
{
    template <typename Source, typename Expected>
    bool operator()(Source const& source, Expected const& expected) const
    {
        return
            run< can_implicit_construct<T> >(source, expected) &&
            run< can_construct<T> >(source, expected) &&
            run< can_rvalue_assign<T> >(source, expected) &&
            run< can_lvalue_assign<T> >(source, expected);
    }
};

int
main()
{
#if defined(BOOST_FUSION_HAS_VARIADIC_DEQUE)
    test<skip_const_lvalue_assignment>();
#else
    test<can_copy>();
#endif
    return boost::report_errors();
}
