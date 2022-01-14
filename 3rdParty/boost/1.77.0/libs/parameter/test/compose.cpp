// Copyright Rene Rivera 2006.
// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/nested_keyword.hpp>
#include <boost/parameter/name.hpp>
#include <boost/parameter/config.hpp>

namespace param {

    BOOST_PARAMETER_NESTED_KEYWORD(tag, a0, a_zero)
    BOOST_PARAMETER_NESTED_KEYWORD(tag, a1, a_one)
    BOOST_PARAMETER_NAME(a2)
    BOOST_PARAMETER_NAME(a3)
    BOOST_PARAMETER_NAME(in(lrc))
    BOOST_PARAMETER_NAME(out(lr))
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
    BOOST_PARAMETER_NAME(consume(rr))
#else
    BOOST_PARAMETER_NAME(rr)
#endif
}

#include <boost/parameter/is_argument_pack.hpp>
#include <boost/mpl/void.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/has_key.hpp>
#include <boost/mpl/key_type.hpp>
#include <boost/mpl/order.hpp>
#include <boost/mpl/count.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/list.hpp>
#include <boost/mp11/map.hpp>
#include <type_traits>
#endif

#if defined(BOOST_NO_CXX11_HDR_FUNCTIONAL)
#include <boost/function.hpp>
#else
#include <functional>
#endif

namespace test {

    struct A
    {
        int i;
        int j;

        template <typename ArgPack>
        A(ArgPack const& args) : i(args[param::a0]), j(args[param::a1])
        {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            static_assert(
                boost::mp11::mp_map_contains<ArgPack,param::tag::a0>::value
              , "param::tag::a0 must be in ArgPack"
            );
            static_assert(
                boost::mp11::mp_map_contains<ArgPack,param::tag::a1>::value
              , "param::tag::a1 must be in ArgPack"
            );
            static_assert(
                !boost::mp11::mp_map_contains<ArgPack,param::tag::lrc>::value
              , "param::tag::lrc must not be in ArgPack"
            );
            static_assert(
                !boost::mp11::mp_map_contains<ArgPack,param::tag::lr>::value
              , "param::tag::lr must not be in ArgPack"
            );
            static_assert(
                !boost::mp11::mp_map_contains<ArgPack,param::tag::rr>::value
              , "param::tag::rr must not be in ArgPack"
            );
            static_assert(
                !std::is_same<
                    ::boost::mp11::mp_map_find<ArgPack,param::tag::a0>
                  , void
                >::value
              , "param::tag::a0 must be found in ArgPack"
            );
            static_assert(
                !std::is_same<
                    ::boost::mp11::mp_map_find<ArgPack,param::tag::a1>
                  , void
                >::value
              , "param::tag::a1 must be found in ArgPack"
            );
            static_assert(
                std::is_same<
                    ::boost::mp11::mp_map_find<ArgPack,param::tag::lrc>
                  , void
                >::value
              , "param::tag::lrc must not be found in ArgPack"
            );
            static_assert(
                std::is_same<
                    ::boost::mp11::mp_map_find<ArgPack,param::tag::lr>
                  , void
                >::value
              , "param::tag::lr must not be found in ArgPack"
            );
            static_assert(
                std::is_same<
                    ::boost::mp11::mp_map_find<ArgPack,param::tag::rr>
                  , void
                >::value
              , "param::tag::rr must not be found in ArgPack"
            );
#endif  // BOOST_PARAMETER_CAN_USE_MP11
            BOOST_MPL_ASSERT((boost::parameter::is_argument_pack<ArgPack>));
            BOOST_MPL_ASSERT((boost::mpl::has_key<ArgPack,param::tag::a0>));
            BOOST_MPL_ASSERT((boost::mpl::has_key<ArgPack,param::tag::a1>));
            BOOST_MPL_ASSERT_NOT((
                boost::mpl::has_key<ArgPack,param::tag::lrc>
            ));
            BOOST_MPL_ASSERT_NOT((
                boost::mpl::has_key<ArgPack,param::tag::lr>
            ));
            BOOST_MPL_ASSERT_NOT((
                boost::mpl::has_key<ArgPack,param::tag::rr>
            ));
            BOOST_MPL_ASSERT((
                boost::mpl::equal_to<
                    typename boost::mpl::count<ArgPack,param::tag::a0>::type
                  , boost::mpl::int_<1>
                >
            ));
            BOOST_MPL_ASSERT((
                boost::mpl::equal_to<
                    typename boost::mpl::count<ArgPack,param::tag::a1>::type
                  , boost::mpl::int_<1>
                >
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::key_type<ArgPack,param::tag::a0>::type
                      , param::tag::a0
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::key_type<ArgPack,param::tag::a1>::type
                      , param::tag::a1
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::order<ArgPack,param::tag::a0>::type
                      , boost::mpl::void_
                    >
                  , boost::mpl::false_
                  , boost::mpl::true_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::order<ArgPack,param::tag::a1>::type
                      , boost::mpl::void_
                    >
                  , boost::mpl::false_
                  , boost::mpl::true_
                >::type
            ));
        }
    };

    float F()
    {
        return 4.0f;
    }

    float E()
    {
        return 4.625f;
    }

    double D()
    {
        return 198.9;
    }

    struct C
    {
        struct result_type
        {
            double operator()() const
            {
                return 2.5;
            }
        };

        result_type operator()() const
        {
            return result_type();
        }
    };

    struct B : A
    {
#if defined(BOOST_NO_CXX11_HDR_FUNCTIONAL)
        boost::function<float()> k;
        boost::function<double()> l;
#else
        std::function<float()> k;
        std::function<double()> l;
#endif

        template <typename ArgPack>
        B(ArgPack const& args)
          : A((args, param::tag::a0::a_zero = 1))
          , k(args[param::_a2 | E])
          , l(args[param::_a3 || C()])
        {
        }
    };

    struct G
    {
        int i;
        int& j;
        int const& k;

        template <typename ArgPack>
        G(ArgPack const& args)
          : i(args[param::_rr])
          , j(args[param::_lr])
          , k(args[param::_lrc])
        {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            static_assert(
                boost::mp11::mp_map_contains<ArgPack,param::tag::lrc>::value
              , "param::tag::lrc must be in ArgPack"
            );
            static_assert(
                boost::mp11::mp_map_contains<ArgPack,param::tag::lr>::value
              , "param::tag::lr must be in ArgPack"
            );
            static_assert(
                boost::mp11::mp_map_contains<ArgPack,param::tag::rr>::value
              , "param::tag::rr must be in ArgPack"
            );
            static_assert(
                !boost::mp11::mp_map_contains<ArgPack,param::tag::a0>::value
              , "param::tag::a0 must not be in ArgPack"
            );
            static_assert(
                !boost::mp11::mp_map_contains<ArgPack,param::tag::a1>::value
              , "param::tag::a1 must not be in ArgPack"
            );
            static_assert(
                !boost::mp11::mp_map_contains<ArgPack,param::tag::a2>::value
              , "param::tag::a2 must not be in ArgPack"
            );
            static_assert(
                !boost::mp11::mp_map_contains<ArgPack,param::tag::a3>::value
              , "param::tag::a3 must not be in ArgPack"
            );
            static_assert(
                !std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::lrc>
                  , void
                >::value
              , "param::tag::lrc must be found in ArgPack"
            );
            static_assert(
                !std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::lr>
                  , void
                >::value
              , "param::tag::lr must be found in ArgPack"
            );
            static_assert(
                !std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::rr>
                  , void
                >::value
              , "param::tag::rr must be found in ArgPack"
            );
            static_assert(
                std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::a0>
                  , void
                >::value
              , "param::tag::a0 must not be found in ArgPack"
            );
            static_assert(
                std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::a1>
                  , void
                >::value
              , "param::tag::a1 must not be found in ArgPack"
            );
            static_assert(
                std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::a2>
                  , void
                >::value
              , "param::tag::a2 must not be found in ArgPack"
            );
            static_assert(
                std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::a3>
                  , void
                >::value
              , "param::tag::a3 must not be found in ArgPack"
            );
#endif  // BOOST_PARAMETER_CAN_USE_MP11
            BOOST_MPL_ASSERT((boost::parameter::is_argument_pack<ArgPack>));
            BOOST_MPL_ASSERT((boost::mpl::has_key<ArgPack,param::tag::rr>));
            BOOST_MPL_ASSERT((boost::mpl::has_key<ArgPack,param::tag::lr>));
            BOOST_MPL_ASSERT((boost::mpl::has_key<ArgPack,param::tag::lrc>));
            BOOST_MPL_ASSERT_NOT((
                boost::mpl::has_key<ArgPack,param::tag::a0>
            ));
            BOOST_MPL_ASSERT_NOT((
                boost::mpl::has_key<ArgPack,param::tag::a1>
            ));
            BOOST_MPL_ASSERT_NOT((
                boost::mpl::has_key<ArgPack,param::tag::a2>
            ));
            BOOST_MPL_ASSERT_NOT((
                boost::mpl::has_key<ArgPack,param::tag::a3>
            ));
            BOOST_MPL_ASSERT((
                boost::mpl::equal_to<
                    typename boost::mpl::count<ArgPack,param::tag::rr>::type
                  , boost::mpl::int_<1>
                >
            ));
            BOOST_MPL_ASSERT((
                boost::mpl::equal_to<
                    typename boost::mpl::count<ArgPack,param::tag::lr>::type
                  , boost::mpl::int_<1>
                >
            ));
            BOOST_MPL_ASSERT((
                boost::mpl::equal_to<
                    typename boost::mpl::count<ArgPack,param::tag::lrc>::type
                  , boost::mpl::int_<1>
                >
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::key_type<ArgPack,param::tag::rr>::type
                      , param::tag::rr
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::key_type<ArgPack,param::tag::lr>::type
                      , param::tag::lr
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::key_type<ArgPack,param::tag::lrc>::type
                      , param::tag::lrc
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::order<ArgPack,param::tag::rr>::type
                      , boost::mpl::void_
                    >
                  , boost::mpl::false_
                  , boost::mpl::true_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::order<ArgPack,param::tag::lr>::type
                      , boost::mpl::void_
                    >
                  , boost::mpl::false_
                  , boost::mpl::true_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::order<ArgPack,param::tag::lrc>::type
                      , boost::mpl::void_
                    >
                  , boost::mpl::false_
                  , boost::mpl::true_
                >::type
            ));
        }
    };
} // namespace test

#include <utility>

namespace test {

    std::pair<int,int> const& lvalue_const_pair()
    {
        static std::pair<int,int> const clp = std::pair<int,int>(7, 10);
        return clp;
    }

    struct H
    {
        std::pair<int,int> i;
        std::pair<int,int>& j;
        std::pair<int,int> const& k;

        template <typename ArgPack>
        H(ArgPack const& args)
          : i(args[param::_rr | test::lvalue_const_pair()])
          , j(args[param::_lr])
          , k(args[param::_lrc])
        {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            static_assert(
                boost::mp11::mp_map_contains<ArgPack,param::tag::lrc>::value
              , "param::tag::lrc must be in ArgPack"
            );
            static_assert(
                boost::mp11::mp_map_contains<ArgPack,param::tag::lr>::value
              , "param::tag::lr must be in ArgPack"
            );
            static_assert(
                !std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::lrc>
                  , void
                >::value
              , "param::tag::lrc must be found in ArgPack"
            );
            static_assert(
                !std::is_same<
                    boost::mp11::mp_map_find<ArgPack,param::tag::lr>
                  , void
                >::value
              , "param::tag::lr must be found in ArgPack"
            );
#endif  // BOOST_PARAMETER_CAN_USE_MP11
            BOOST_MPL_ASSERT((boost::parameter::is_argument_pack<ArgPack>));
            BOOST_MPL_ASSERT((boost::mpl::has_key<ArgPack,param::tag::lr>));
            BOOST_MPL_ASSERT((boost::mpl::has_key<ArgPack,param::tag::lrc>));
            BOOST_MPL_ASSERT((
                boost::mpl::equal_to<
                    typename boost::mpl::count<ArgPack,param::tag::lr>::type
                  , boost::mpl::int_<1>
                >
            ));
            BOOST_MPL_ASSERT((
                boost::mpl::equal_to<
                    typename boost::mpl::count<ArgPack,param::tag::lrc>::type
                  , boost::mpl::int_<1>
                >
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::key_type<ArgPack,param::tag::lr>::type
                      , param::tag::lr
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::key_type<ArgPack,param::tag::lrc>::type
                      , param::tag::lrc
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::order<ArgPack,param::tag::lr>::type
                      , boost::mpl::void_
                    >
                  , boost::mpl::false_
                  , boost::mpl::true_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<
                        typename boost::mpl
                        ::order<ArgPack,param::tag::lrc>::type
                      , boost::mpl::void_
                    >
                  , boost::mpl::false_
                  , boost::mpl::true_
                >::type
            ));
        }
    };
} // namespace test

#include <boost/core/lightweight_test.hpp>

void test_compose0()
{
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1700) && \
    BOOST_WORKAROUND(BOOST_MSVC, < 1800)
    // MSVC 11.0 on AppVeyor fails without this workaround.
    test::A a((
        param::a0 = 1
      , param::a1 = 13
      , param::_a2 = std::function<double()>(test::D)
    ));
#else
    test::A a((param::a0 = 1, param::a1 = 13, param::_a2 = test::D));
#endif
    BOOST_TEST_EQ(1, a.i);
    BOOST_TEST_EQ(13, a.j);
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1700) && \
    BOOST_WORKAROUND(BOOST_MSVC, < 1800)
    // MSVC 11.0 on AppVeyor fails without this workaround.
    test::B b0((
        param::tag::a1::a_one = 13
      , param::_a2 = std::function<float()>(test::F)
    ));
#else
    test::B b0((param::tag::a1::a_one = 13, param::_a2 = test::F));
#endif
    BOOST_TEST_EQ(1, b0.i);
    BOOST_TEST_EQ(13, b0.j);
    BOOST_TEST_EQ(4.0f, b0.k());
    BOOST_TEST_EQ(2.5, b0.l());
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1700) && \
    BOOST_WORKAROUND(BOOST_MSVC, < 1800)
    // MSVC 11.0 on AppVeyor fails without this workaround.
    test::B b1((
        param::_a3 = std::function<double()>(test::D)
      , param::a1 = 13
    ));
#else
    test::B b1((param::_a3 = test::D, param::a1 = 13));
#endif
    BOOST_TEST_EQ(1, b1.i);
    BOOST_TEST_EQ(13, b1.j);
    BOOST_TEST_EQ(4.625f, b1.k());
    BOOST_TEST_EQ(198.9, b1.l());
    int x = 23;
    int const y = 42;
#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_0)
    test::G g((param::_lr = 15, param::_rr = 16, param::_lrc = y));
#else
    test::G g((param::_lr = x, param::_rr = 16, param::_lrc = y));
#endif
    BOOST_TEST_EQ(16, g.i);
    BOOST_TEST_EQ(23, g.j);
    BOOST_TEST_EQ(42, g.k);
    x = 1;
    BOOST_TEST_EQ(1, g.j);
    std::pair<int,int> p1(8, 9);
    std::pair<int,int> const p2(11, 12);
    test::H h0((param::_lr = p1, param::_lrc = p2));
#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_1)
    test::H h1((
        param::_lr = p2
      , param::_rr = std::make_pair(7, 10)
      , param::_lrc = p2
    ));
#else
    test::H h1((
        param::_lr = p1
      , param::_rr = std::make_pair(7, 10)
      , param::_lrc = p2
    ));
#endif
    BOOST_TEST_EQ(h0.i.first, h1.i.first);
    BOOST_TEST_EQ(h0.i.second, h1.i.second);
    BOOST_TEST_EQ(p1.first, h1.j.first);
    BOOST_TEST_EQ(p1.second, h1.j.second);
    BOOST_TEST_EQ(p2.first, h1.k.first);
    BOOST_TEST_EQ(p2.second, h1.k.second);
    p1.first = 1;
    BOOST_TEST_EQ(p1.first, h1.j.first);
}

#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) || \
    (2 < BOOST_PARAMETER_COMPOSE_MAX_ARITY)
#include <boost/parameter/compose.hpp>

void test_compose1()
{
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1700) && \
    BOOST_WORKAROUND(BOOST_MSVC, < 1800)
    // MSVC 11.0 on AppVeyor fails without this workaround.
    test::A a(boost::parameter::compose(
        param::a0 = 1
      , param::a1 = 13
      , param::_a2 = std::function<double()>(test::D)
    ));
#else
    test::A a(boost::parameter::compose(
        param::a0 = 1
      , param::a1 = 13
      , param::_a2 = test::D
    ));
#endif
    BOOST_TEST_EQ(1, a.i);
    BOOST_TEST_EQ(13, a.j);
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1700) && \
    BOOST_WORKAROUND(BOOST_MSVC, < 1800)
    // MSVC 11.0 on AppVeyor fails without this workaround.
    test::B b0(boost::parameter::compose(
        param::tag::a1::a_one = 13
      , param::_a2 = std::function<float()>(test::F)
    ));
#else
    test::B b0(boost::parameter::compose(
        param::tag::a1::a_one = 13
      , param::_a2 = test::F
    ));
#endif
    BOOST_TEST_EQ(1, b0.i);
    BOOST_TEST_EQ(13, b0.j);
    BOOST_TEST_EQ(4.0f, b0.k());
    BOOST_TEST_EQ(2.5, b0.l());
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) && \
    BOOST_WORKAROUND(BOOST_MSVC, >= 1700) && \
    BOOST_WORKAROUND(BOOST_MSVC, < 1800)
    // MSVC 11.0 on AppVeyor fails without this workaround.
    test::B b1(boost::parameter::compose(
        param::_a3 = std::function<double()>(test::D)
      , param::a1 = 13
    ));
#else
    test::B b1(boost::parameter::compose(
        param::_a3 = test::D
      , param::a1 = 13
    ));
#endif
    BOOST_TEST_EQ(1, b1.i);
    BOOST_TEST_EQ(13, b1.j);
    BOOST_TEST_EQ(4.625f, b1.k());
    BOOST_TEST_EQ(198.9, b1.l());
    int x = 23;
    int const y = 42;
#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_0)
    test::G g(boost::parameter::compose(
        param::_lr = 15
      , param::_rr = 16
      , param::_lrc = y
    ));
#else
    test::G g(boost::parameter::compose(
        param::_lr = x
      , param::_rr = 16
      , param::_lrc = y
    ));
#endif
    BOOST_TEST_EQ(16, g.i);
    BOOST_TEST_EQ(23, g.j);
    BOOST_TEST_EQ(42, g.k);
    x = 1;
    BOOST_TEST_EQ(1, g.j);
    std::pair<int,int> p1(8, 9);
    std::pair<int,int> const p2(11, 12);
    test::H h0(boost::parameter::compose(param::_lr = p1, param::_lrc = p2));
#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_1)
    test::H h1(boost::parameter::compose(
        param::_lr = p2
      , param::_rr = std::make_pair(7, 10)
      , param::_lrc = p2
    ));
#else
    test::H h1(boost::parameter::compose(
        param::_lr = p1
      , param::_rr = std::make_pair(7, 10)
      , param::_lrc = p2
    ));
#endif
    BOOST_TEST_EQ(h0.i.first, h1.i.first);
    BOOST_TEST_EQ(h0.i.second, h1.i.second);
    BOOST_TEST_EQ(p1.first, h1.j.first);
    BOOST_TEST_EQ(p1.second, h1.j.second);
    BOOST_TEST_EQ(p2.first, h1.k.first);
    BOOST_TEST_EQ(p2.second, h1.k.second);
    p1.first = 1;
    BOOST_TEST_EQ(p1.first, h1.j.first);
}

#endif  // can invoke boost::parameter::compose

int main()
{
    test_compose0();
#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) || \
    (2 < BOOST_PARAMETER_COMPOSE_MAX_ARITY)
    test_compose1();
#endif
    return boost::report_errors();
}

