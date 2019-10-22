// Copyright Daniel Wallin 2006.
// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DEDUCED_060920_HPP
#define BOOST_DEDUCED_060920_HPP

#include <boost/parameter/config.hpp>
#include "basics.hpp"

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/map.hpp>
#include <boost/mp11/algorithm.hpp>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>
#endif

namespace test {

    struct not_present_tag
    {
    };

    not_present_tag not_present;

    template <typename E, typename ArgPack>
    class assert_expected
    {
        E const& _expected;
        ArgPack const& _args;

     public:
        assert_expected(E const& e, ArgPack const& args_)
          : _expected(e), _args(args_)
        {
        }

        template <typename T>
        static bool check_not_present(T const&)
        {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            static_assert(
                std::is_same<T,test::not_present_tag>::value
              , "T == test::not_present_tag"
            );
#else
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<T,test::not_present_tag>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
#endif
            return true;
        }

        template <typename K>
        bool check1(K const& k, test::not_present_tag const& t, long) const
        {
            return assert_expected<E,ArgPack>::check_not_present(
                this->_args[k | t]
            );
        }

        template <typename K, typename Expected>
        bool check1(K const& k, Expected const& e, int) const
        {
            return test::equal(this->_args[k], e);
        }

        template <typename K>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        void operator()(K&&) const
#else
        void operator()(K) const
#endif
        {
            boost::parameter::keyword<K> const&
                k = boost::parameter::keyword<K>::instance;
            BOOST_TEST(this->check1(k, this->_expected[k], 0L));
        }
    };

    template <typename E, typename A>
    void check0(E const& e, A const& args)
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        boost::mp11::mp_for_each<boost::mp11::mp_map_keys<E> >(
            test::assert_expected<E,A>(e, args)
        );
#else
        boost::mpl::for_each<E>(test::assert_expected<E,A>(e, args));
#endif
    }

#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
    template <typename P, typename E, typename ...Args>
    void check(E const& e, Args const&... args)
    {
        test::check0(e, P()(args...));
    }
#else
    template <typename P, typename E, typename A0>
    void check(E const& e, A0 const& a0)
    {
        test::check0(e, P()(a0));
    }

    template <typename P, typename E, typename A0, typename A1>
    void check(E const& e, A0 const& a0, A1 const& a1)
    {
        test::check0(e, P()(a0, a1));
    }

    template <typename P, typename E, typename A0, typename A1, typename A2>
    void check(E const& e, A0 const& a0, A1 const& a1, A2 const& a2)
    {
        test::check0(e, P()(a0, a1, a2));
    }
#endif  // BOOST_PARAMETER_HAS_PERFECT_FORWARDING
} // namespace test

#endif  // include guard

