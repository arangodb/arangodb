// Copyright David Abrahams 2006.
// Copyright Cromwell D. Enage 2019.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/add_pointer.hpp>
#include "basics.hpp"

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/list.hpp>
#include <boost/mp11/map.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/mpl.hpp>
#endif

namespace test {

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    template <typename Map>
    struct assert_in_map
    {
        template <typename T>
        void operator()(T&&)
        {
            static_assert(
                boost::mp11::mp_map_contains<Map,T>::value
              , "T must be in Map"
            );
        }
    };

    template <typename Set>
    struct assert_in_set_0
    {
        template <typename T>
        void operator()(T&&)
        {
            static_assert(
                boost::mp11::mp_contains<Set,T>::value
              , "T must be in Set"
            );
        }
    };
#endif

    template <typename Set>
    struct assert_in_set_1
    {
        template <typename T>
        void operator()(T*)
        {
            BOOST_MPL_ASSERT((boost::mpl::contains<Set,T>));
        }
    };

    template <typename Expected, typename Args>
    void f_impl(Args const& p BOOST_APPEND_EXPLICIT_TEMPLATE_TYPE(Expected))
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        static_assert(
            boost::mp11::mp_size<Expected>::value == boost::mp11::mp_size<
                Args
            >::value
          , "mp_size<Expected>::value == mp_size<Args>::value"
        );

        boost::mp11::mp_for_each<boost::mp11::mp_map_keys<Args> >(
            test::assert_in_set_0<Expected>()
        );
        boost::mp11::mp_for_each<Expected>(test::assert_in_map<Args>());
#endif

        BOOST_MPL_ASSERT_RELATION(
            boost::mpl::size<Expected>::value
          , ==
          , boost::mpl::size<Args>::value
        );

        boost::mpl::for_each<Args,boost::add_pointer<boost::mpl::_1> >(
            test::assert_in_set_1<Expected>()
        );
        boost::mpl::for_each<Expected,boost::add_pointer<boost::mpl::_1> >(
            test::assert_in_set_1<Args>()
        );
    }

    template <
        typename Expected
      , typename Tester
      , typename Name
      , typename Value
      , typename Index
    >
    void f(
        Tester const& t
      , Name const& name_
      , Value const& value_
      , Index const& index_
        BOOST_APPEND_EXPLICIT_TEMPLATE_TYPE(Expected)
    )
    {
        test::f_impl<Expected>(
            test::f_parameters()(t, name_, value_, index_)
        );
    }

    template <
        typename Expected
      , typename Tester
      , typename Name
      , typename Value
    >
    void f(
        Tester const& t
      , Name const& name_
      , Value const& value_
        BOOST_APPEND_EXPLICIT_TEMPLATE_TYPE(Expected)
    )
    {
        test::f_impl<Expected>(test::f_parameters()(t, name_, value_));
    }

    template <typename Expected, typename Tester, typename Name>
    void f(
        Tester const& t
      , Name const& name_
        BOOST_APPEND_EXPLICIT_TEMPLATE_TYPE(Expected)
    )
    {
        test::f_impl<Expected>(test::f_parameters()(t, name_));
    }

    void run()
    {
        typedef test::tag::tester tester_;
        typedef test::tag::name name_;
        typedef test::tag::value value_;
        typedef test::tag::index index_;

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        test::f<
            boost::mp11::mp_list<tester_,name_,value_,index_>
        >(1, 2, 3, 4);
        test::f<
            boost::mp11::mp_list<tester_,name_,index_>
        >(1, 2, test::_index = 3);
        test::f<
            boost::mp11::mp_list<tester_,name_,index_>
        >(1, test::_index = 2, test::_name = 3);
        test::f<
            boost::mp11::mp_list<name_,value_>
        >(test::_name = 3, test::_value = 4);
        test::f_impl<boost::mp11::mp_list<value_> >(test::_value = 4);
#endif

        test::f<boost::mpl::list4<tester_,name_,value_,index_> >(1, 2, 3, 4);
        test::f<
            boost::mpl::list3<tester_,name_,index_>
        >(1, 2, test::_index = 3);
        test::f<
            boost::mpl::list3<tester_,name_,index_>
        >(1, test::_index = 2, test::_name = 3);
        test::f<
            boost::mpl::list2<name_,value_>
        >(test::_name = 3, test::_value = 4);
        test::f_impl<boost::mpl::list1<value_> >(test::_value = 4);
    }
}

#include <boost/core/lightweight_test.hpp>

int main()
{
    test::run();
    return boost::report_errors();
}

