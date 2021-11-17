
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/mpl.hpp>
#include <boost/mp11/list.hpp>

#include <boost/mpl/at.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/begin.hpp>
#include <boost/mpl/end.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/clear.hpp>
#include <boost/mpl/empty.hpp>
#include <boost/mpl/erase.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/insert.hpp>
#include <boost/mpl/insert_range.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/push_back.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/reverse.hpp>
#include <boost/mpl/remove.hpp>
#include <boost/mpl/copy.hpp>
#include <boost/mpl/back_inserter.hpp>

#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

template<class T> using add_pointer_t = typename std::add_pointer<T>::type;

template<class L1> void test()
{
    namespace mpl = boost::mpl;
    using namespace boost::mp11;

    // intrinsics

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at<L1, mpl::int_<0>>::type, mp_at_c<L1, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at<L1, mpl::int_<1>>::type, mp_at_c<L1, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at<L1, mpl::int_<2>>::type, mp_at_c<L1, 2>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at_c<L1, 0>::type, mp_at_c<L1, 0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at_c<L1, 1>::type, mp_at_c<L1, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::at_c<L1, 2>::type, mp_at_c<L1, 2>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::back<L1>::type, float>));

    BOOST_TEST_EQ((mpl::distance<typename mpl::begin<L1>::type, typename mpl::end<L1>::type>::type::value), mp_size<L1>::value);

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::clear<L1>::type, mp_clear<L1>>));

    BOOST_TEST_TRAIT_FALSE((typename mpl::empty<L1>::type));
    BOOST_TEST_TRAIT_TRUE((typename mpl::empty<mp_clear<L1>>::type));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::erase<L1, typename mpl::begin<L1>::type>::type, mp_pop_front<L1>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::front<L1>::type, mp_front<L1>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::insert<L1, typename mpl::begin<L1>::type, void>::type, mp_push_front<L1, void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::insert<L1, typename mpl::end<L1>::type, void>::type, mp_push_back<L1, void>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::insert_range<L1, typename mpl::end<L1>::type, L1>::type, mp_append<L1, L1>>));

    BOOST_TEST_TRAIT_TRUE((typename mpl::is_sequence<L1>::type));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::pop_front<L1>::type, mp_pop_front<L1>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::push_back<L1, char>::type, mp_push_back<L1, char>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::push_front<L1, char>::type, mp_push_front<L1, char>>));

    BOOST_TEST_EQ((mpl::size<L1>::type::value), mp_size<L1>::value);

    // algorithms

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::transform<L1, std::add_pointer<mpl::_1>>::type, mp_transform<add_pointer_t, L1>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::reverse<L1>::type, mp_reverse<L1>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<typename mpl::remove<L1, int>::type, mp_remove<L1, int>>));

    using L2 = typename mpl::copy<L1, mpl::back_inserter<mpl::vector<>>>::type;
    using L3 = typename mpl::copy<L2, mpl::back_inserter<mp_clear<L1>>>::type;

    BOOST_TEST_TRAIT_TRUE((std::is_same<L1, L3>));
}

int main()
{
    using boost::mp11::mp_list;

    test<mp_list<int, void, float>>();
    test<std::tuple<int, long, float>>(); // MPL instantiates the tuple, so no 'void'

    return boost::report_errors();
}
