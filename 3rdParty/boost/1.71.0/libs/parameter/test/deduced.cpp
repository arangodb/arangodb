// Copyright Daniel Wallin 2006.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/parameters.hpp>
#include <boost/parameter/name.hpp>
#include <boost/parameter/binding.hpp>
#include <boost/parameter/config.hpp>
#include "deduced.hpp"

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_convertible.hpp>
#endif

#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
#include <boost/parameter/aux_/preprocessor/nullptr.hpp>
#endif

namespace test {

    BOOST_PARAMETER_NAME(x)
    BOOST_PARAMETER_NAME(y)
    BOOST_PARAMETER_NAME(z)

    template <typename To>
    struct predicate
    {
        template <typename From, typename Args>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        using fn = std::is_convertible<From,To>;
#else
        struct apply
          : boost::mpl::if_<
                boost::is_convertible<From,To>
              , boost::mpl::true_
              , boost::mpl::false_
            >
        {
        };
#endif
    };
} // namespace test

#include <boost/core/lightweight_test.hpp>
#include <string>

int main()
{
    test::check<
        boost::parameter::parameters<test::tag::x,test::tag::y>
    >((test::_x = 0, test::_y = 1), 0, 1);

    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::required<
                boost::parameter::deduced<test::tag::y>
              , test::predicate<int>
            >
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::z>
              , test::predicate<std::string>
            >
        >
    >(
        (
            test::_x = 0
          , test::_y = test::not_present
          , test::_z = std::string("foo")
        )
      , test::_x = 0
      , std::string("foo")
    );

    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::required<
                boost::parameter::deduced<test::tag::y>
              , test::predicate<int>
            >
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::z>
              , test::predicate<std::string>
            >
        >
    >(
        (test::_x = 0, test::_y = 1, test::_z = std::string("foo"))
      , 0
      , std::string("foo")
      , 1
    );

    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::required<
                boost::parameter::deduced<test::tag::y>
              , test::predicate<int>
            >
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::z>
              , test::predicate<std::string>
            >
        >
    >(
        (test::_x = 0, test::_y = 1, test::_z = std::string("foo"))
      , 0
      , 1
      , std::string("foo")
    );

    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::required<
                boost::parameter::deduced<test::tag::y>
              , test::predicate<int>
            >
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::z>
              , test::predicate<std::string>
            >
        >
    >(
        (test::_x = 0, test::_y = 1, test::_z = std::string("foo"))
      , 0
      , test::_y = 1
      , std::string("foo")
    );

    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::required<
                boost::parameter::deduced<test::tag::y>
              , test::predicate<int>
            >
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::z>
              , test::predicate<std::string>
            >
        >
    >(
        (test::_x = 0, test::_y = 1, test::_z = std::string("foo"))
      , test::_z = std::string("foo")
      , test::_x = 0
      , 1
    );

#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
    // Fails because boost::parameter::aux::make_arg_list<> evaluates
    // boost::parameter::aux::is_named_argument<> to boost::mpl::false_
    // for static_cast<long*>(BOOST_PARAMETER_AUX_PP_NULLPTR).
    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::required<
                boost::parameter::deduced<test::tag::y>
              , test::predicate<int>
            >
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::z>
              , test::predicate<std::string>
            >
        >
    >(
        (test::_x = 0, test::_y = 1, test::_z = std::string("foo"))
      , test::_x = 0
      , static_cast<long*>(BOOST_PARAMETER_AUX_PP_NULLPTR)
      , 1
    );
#endif

    return boost::report_errors();
}

