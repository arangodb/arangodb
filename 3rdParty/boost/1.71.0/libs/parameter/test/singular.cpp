// Copyright Daniel Wallin 2005.
// Copyright Cromwell D. Enage 2019.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

namespace test {

    struct default_src
    {
        typedef int result_type;

        int operator()() const
        {
            return 0;
        }
    };
} // namespace test

#include <boost/parameter/name.hpp>

namespace test {

    BOOST_PARAMETER_NAME(x)
    BOOST_PARAMETER_NAME(y)
    BOOST_PARAMETER_NAME(z)
} // namespace test

#include <boost/parameter/is_argument_pack.hpp>
#include <boost/parameter/config.hpp>
#include <boost/mpl/has_key.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/core/lightweight_test.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/map.hpp>
#include <type_traits>
#endif

namespace test {

    template <typename ArgumentPack, typename K, typename T>
    void check0(ArgumentPack const& p, K const& kw, T const& value)
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        static_assert(
            !boost::mp11::mp_map_contains<ArgumentPack,test::tag::z>::value
          , "test::tag::z must not be in ArgumentPack"
        );
        static_assert(
            std::is_same<
                boost::mp11::mp_map_find<ArgumentPack,test::tag::z>
              , void
            >::value
          , "test::tag::z must not be found in ArgumentPack"
        );
#endif  // BOOST_PARAMETER_CAN_USE_MP11
        BOOST_MPL_ASSERT((boost::parameter::is_argument_pack<ArgumentPack>));
        BOOST_MPL_ASSERT_NOT((
            boost::mpl::has_key<ArgumentPack,test::tag::z>
        ));
        BOOST_TEST_EQ(p[kw], value);
    }
} // namespace test

#include <boost/mpl/void.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/key_type.hpp>
#include <boost/mpl/order.hpp>
#include <boost/mpl/count.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/type_traits/is_same.hpp>

namespace test {

    template <typename ArgumentPack, typename K, typename T>
    void check1(ArgumentPack const& p, K const& kw, T const& value)
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        static_assert(
            boost::mp11::mp_map_contains<ArgumentPack,typename K::tag>::value
          , "typename K::tag must be in ArgumentPack"
        );
        static_assert(
            !boost::mp11::mp_map_contains<ArgumentPack,test::tag::z>::value
          , "test::tag::z must not be in ArgumentPack"
        );
        static_assert(
            !std::is_same<
                boost::mp11::mp_map_find<ArgumentPack,typename K::tag>
              , void
            >::value
          , "typename K::tag must be found in ArgumentPack"
        );
        static_assert(
            std::is_same<
                boost::mp11::mp_map_find<ArgumentPack,test::tag::z>
              , void
            >::value
          , "test::tag::z must not be found in ArgumentPack"
        );
#endif  // BOOST_PARAMETER_CAN_USE_MP11
        BOOST_MPL_ASSERT((boost::parameter::is_argument_pack<ArgumentPack>));
        BOOST_MPL_ASSERT((boost::mpl::has_key<ArgumentPack,typename K::tag>));
        BOOST_MPL_ASSERT_NOT((
            boost::mpl::has_key<ArgumentPack,test::tag::z>
        ));
        BOOST_MPL_ASSERT((
            boost::mpl::equal_to<
                typename boost::mpl::count<ArgumentPack,typename K::tag>::type
              , boost::mpl::int_<1>
            >
        ));
        BOOST_MPL_ASSERT((
            typename boost::mpl::if_<
                boost::is_same<
                    typename boost::mpl
                    ::key_type<ArgumentPack,typename K::tag>::type
                  , typename K::tag
                >
              , boost::mpl::true_
              , boost::mpl::false_
            >::type
        ));
        BOOST_MPL_ASSERT((
            typename boost::mpl::if_<
                boost::is_same<
                    typename boost::mpl
                    ::order<ArgumentPack,typename K::tag>::type
                  , boost::mpl::void_
                >
              , boost::mpl::false_
              , boost::mpl::true_
            >::type
        ));
        BOOST_TEST_EQ(p[kw], value);
    }
} // namespace test

int main()
{
    test::check1(test::_x = 20, test::_x, 20);
    test::check1(test::_y = 20, test::_y, 20);

    test::check0(test::_x = 20, test::_x | 0, 20);
    test::check0(test::_y = 20, test::_y | 0, 20);

    test::check0(test::_x = 20, test::_x || test::default_src(), 20);
    test::check0(test::_y = 20, test::_y || test::default_src(), 20);

    test::check0(test::_y = 20, test::_x | 0, 0);
    test::check0(test::_y = 20, test::_x || test::default_src(), 0);

    return boost::report_errors();
}

