// Copyright David Abrahams, Daniel Wallin 2005.
// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BASICS_050424_HPP
#define BASICS_050424_HPP

#include <boost/parameter.hpp>

#if (BOOST_PARAMETER_MAX_ARITY < 4)
#error Define BOOST_PARAMETER_MAX_ARITY as 4 or greater.
#endif
#if !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) && \
    (BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY < 5)
#error Define BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY \
as 5 or greater.
#endif

#if !defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>
#endif

#include <boost/core/lightweight_test.hpp>

namespace test {

    BOOST_PARAMETER_NAME(name)
    BOOST_PARAMETER_NAME(value)
    BOOST_PARAMETER_NAME(index)
    BOOST_PARAMETER_NAME(tester)

    struct f_parameters // vc6 is happier with inheritance than with a typedef
      : boost::parameter::parameters<
            test::tag::tester
          , test::tag::name
          , test::tag::value
          , test::tag::index
        >
    {
    };

    inline double value_default()
    {
        return 666.222;
    }

    template <typename T>
    inline bool equal(T const& x, T const& y)
    {
        return x == y;
    }

    template <typename Name, typename Value, typename Index>
    struct values_t
    {
        values_t(Name const& n_, Value const& v_, Index const& i_)
          : n(n_), v(v_), i(i_)
        {
        }

        template <typename Name_, typename Value_, typename Index_>
        void
            operator()(
                Name_ const& n_
              , Value_ const& v_
              , Index_ const& i_
            ) const
        {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            static_assert(
                std::is_same<Index,Index_>::value
              , "Index == Index_"
            );
            static_assert(
                std::is_same<Value,Value_>::value
              , "Value == Value_"
            );
            static_assert(
                std::is_same<Name,Name_>::value
              , "Name == Name_"
            );
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<Index,Index_>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<Value,Value_>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
            BOOST_MPL_ASSERT((
                typename boost::mpl::if_<
                    boost::is_same<Name,Name_>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >::type
            ));
#endif  // BOOST_PARAMETER_CAN_USE_MP11
            BOOST_TEST(test::equal(n, n_));
            BOOST_TEST(test::equal(v, v_));
            BOOST_TEST(test::equal(i, i_));
        }

        Name const& n;
        Value const& v;
        Index const& i;
    };

    template <typename Name, typename Value, typename Index>
    inline test::values_t<Name,Value,Index>
        values(Name const& n, Value const& v, Index const& i)
    {
        return test::values_t<Name,Value,Index>(n, v, i);
    }
} // namespace test

#endif  // include guard

