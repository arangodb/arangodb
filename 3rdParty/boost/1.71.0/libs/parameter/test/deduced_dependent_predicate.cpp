// Copyright Daniel Wallin 2006.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>
#include <boost/parameter/parameters.hpp>
#include <boost/parameter/name.hpp>
#include <boost/parameter/binding.hpp>
#include "deduced.hpp"

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/bind.hpp>
#include <boost/mp11/utility.hpp>
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_convertible.hpp>
#if BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x564))
#include <boost/type_traits/remove_reference.hpp>
#else
#include <boost/type_traits/add_lvalue_reference.hpp>
#endif  // Borland workarounds needed
#endif  // BOOST_PARAMETER_CAN_USE_MP11

namespace test {

    BOOST_PARAMETER_NAME(x)
    BOOST_PARAMETER_NAME(y)
    BOOST_PARAMETER_NAME(z)
} // namespace test

#include <boost/core/lightweight_test.hpp>

int main()
{
    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::y>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
              , boost::mp11::mp_bind<
                    std::is_same
                  , boost::mp11::_1
                  , boost::mp11::mp_bind<
                        test::tag::x::binding_fn
                      , boost::mp11::_2
                    >
                >
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
              , boost::mpl::if_<
                    boost::is_same<
#if BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x564))
                        boost::mpl::_1
                      , boost::remove_reference<
                            boost::parameter::binding<
                                boost::mpl::_2
                              , test::tag::x
                            >
                        >
#else
                        boost::add_lvalue_reference<boost::mpl::_1>
                      , boost::parameter::binding<boost::mpl::_2,test::tag::x>
#endif  // Borland workarounds needed
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >
#endif  // BOOST_PARAMETER_CAN_USE_MP11
            >
        >
    >((test::_x = 0, test::_y = 1), 0, 1);

    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::y>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
              , boost::mp11::mp_bind<
                    std::is_same
                  , boost::mp11::_1
                  , boost::mp11::mp_bind<
                        test::tag::x::binding_fn
                      , boost::mp11::_2
                    >
                >
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
              , boost::mpl::if_<
                    boost::is_same<
#if BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x564))
                        boost::mpl::_1
                      , boost::remove_reference<
                            boost::parameter::binding<
                                boost::mpl::_2
                              , test::tag::x
                            >
                        >
#else
                        boost::add_lvalue_reference<boost::mpl::_1>
                      , boost::parameter::binding<boost::mpl::_2,test::tag::x>
#endif  // Borland workarounds needed
                    >
                  , boost::mpl::true_
                  , boost::mpl::false_
                >
#endif  // BOOST_PARAMETER_CAN_USE_MP11
            >
        >
    >((test::_x = 0U, test::_y = 1U), 0U, 1U);

    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::y>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
              , boost::mp11::mp_bind<
                    std::is_convertible
                  , boost::mp11::_1
                  , boost::mp11::mp_bind_q<test::tag::x,boost::mp11::_2>
                >
#else
              , boost::mpl::if_<
                    boost::is_convertible<boost::mpl::_1,test::tag::x::_>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >
#endif
            >
        >
    >((test::_x = 0, test::_y = 1), 0, 1);

    test::check<
        boost::parameter::parameters<
            test::tag::x
          , boost::parameter::optional<
                boost::parameter::deduced<test::tag::y>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
              , boost::mp11::mp_bind<
                    std::is_convertible
                  , boost::mp11::_1
                  , boost::mp11::mp_bind_q<test::tag::x,boost::mp11::_2>
                >
#else
              , boost::mpl::if_<
                    boost::is_convertible<boost::mpl::_1,test::tag::x::_1>
                  , boost::mpl::true_
                  , boost::mpl::false_
                >
#endif
            >
        >
    >((test::_x = 0U, test::_y = 1U), 0U, 1U);

    return boost::report_errors();
}

