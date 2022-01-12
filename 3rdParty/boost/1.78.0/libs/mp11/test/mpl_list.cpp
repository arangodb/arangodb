
// Copyright 2017, 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/mpl_list.hpp>
#include <boost/mp11/list.hpp>

#include <boost/mpl/at.hpp>
#include <boost/mpl/size.hpp>

#include <boost/core/lightweight_test_trait.hpp>

int main()
{
    namespace mpl = boost::mpl;
    using namespace boost::mp11;

    using L1 = mp_list<void, int, float>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at<L1, mpl::int_<0>>::type, mp_at_c<L1, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at<L1, mpl::int_<1>>::type, mp_at_c<L1, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at<L1, mpl::int_<2>>::type, mp_at_c<L1, 2>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at_c<L1, 0>::type, mp_at_c<L1, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at_c<L1, 1>::type, mp_at_c<L1, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at_c<L1, 2>::type, mp_at_c<L1, 2>>));

    BOOST_TEST_EQ((mpl::size<L1>::type::value), mp_size<L1>::value);

    return boost::report_errors();
}
