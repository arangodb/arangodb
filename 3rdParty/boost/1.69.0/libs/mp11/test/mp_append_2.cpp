
//  Copyright 2015-2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/lightweight_test_trait.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

template<class T> struct W;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_append;
    using boost::mp11::mp_iota_c;
    using boost::mp11::mp_transform;
    using boost::mp11::mp_rename;
    using boost::mp11::mp_push_front;

    using L1 = mp_iota_c<125>;
    using L2 = mp_transform<W, L1>;
    using L3 = mp_push_front<L2, mp_list<>>;
    using L4 = mp_rename<L3, mp_append>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<L4, L1>));

    //

    return boost::report_errors();
}
