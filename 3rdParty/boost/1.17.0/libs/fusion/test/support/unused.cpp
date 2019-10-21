/*=============================================================================
    Copyright (c) 2018 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/config.hpp>
#include <boost/fusion/support/unused.hpp>
#include <boost/type_traits/detail/yes_no_type.hpp>
#include <boost/static_assert.hpp>
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
#include <utility>
#endif

struct T { };

void unused_construction()
{
    boost::fusion::unused_type dephault;

    boost::fusion::unused_type BOOST_ATTRIBUTE_UNUSED parenthesis = boost::fusion::unused_type();
#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
    boost::fusion::unused_type BOOST_ATTRIBUTE_UNUSED brace{};
    boost::fusion::unused_type BOOST_ATTRIBUTE_UNUSED list_copy = {};
#endif

    boost::fusion::unused_type copy_copy BOOST_ATTRIBUTE_UNUSED = dephault;
    boost::fusion::unused_type copy_direct BOOST_ATTRIBUTE_UNUSED (dephault);
#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
    boost::fusion::unused_type copy_copy_brace_direct BOOST_ATTRIBUTE_UNUSED = {dephault};
    boost::fusion::unused_type copy_direct_brace BOOST_ATTRIBUTE_UNUSED {dephault};
#endif

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    boost::fusion::unused_type move_copy BOOST_ATTRIBUTE_UNUSED = std::move(dephault);
    boost::fusion::unused_type move_direct BOOST_ATTRIBUTE_UNUSED (std::move(dephault));
#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
    boost::fusion::unused_type move_copy_brace_direct BOOST_ATTRIBUTE_UNUSED = {std::move(dephault)};
    boost::fusion::unused_type move_direct_brace BOOST_ATTRIBUTE_UNUSED {std::move(dephault)};
#endif
#endif


    T value;

    boost::fusion::unused_type T_copy_copy BOOST_ATTRIBUTE_UNUSED = value;
    boost::fusion::unused_type T_copy_direct BOOST_ATTRIBUTE_UNUSED (value);
#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
    boost::fusion::unused_type T_copy_copy_brace_direct BOOST_ATTRIBUTE_UNUSED = {value};
    boost::fusion::unused_type T_copy_direct_brace BOOST_ATTRIBUTE_UNUSED {value};
#endif

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    boost::fusion::unused_type T_move_copy BOOST_ATTRIBUTE_UNUSED = std::move(value);
    boost::fusion::unused_type T_move_direct BOOST_ATTRIBUTE_UNUSED (std::move(value));
#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
    boost::fusion::unused_type T_move_copy_brace_direct BOOST_ATTRIBUTE_UNUSED = {std::move(value)};
    boost::fusion::unused_type T_move_direct_brace BOOST_ATTRIBUTE_UNUSED {std::move(value)};
#endif
#endif
}

void unused_assignment()
{
    boost::fusion::unused_type val1, val2;

    val1 = val2;
#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
    val1 = {};
#endif
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    val1 = std::move(val2);
#endif


    T value;

    val1 = value;
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    val1 = std::move(value);
#endif
}

boost::type_traits::yes_type test_unused(boost::fusion::detail::unused_only const&);
boost::type_traits::no_type  test_unused(...);

void only_unused()
{
    BOOST_STATIC_ASSERT((sizeof(test_unused(boost::fusion::unused)) == sizeof(boost::type_traits::yes_type)));
    BOOST_STATIC_ASSERT((sizeof(test_unused(0)) == sizeof(boost::type_traits::no_type)));

    boost::fusion::unused_type my_unused;
    (void)my_unused;
    BOOST_STATIC_ASSERT((sizeof(test_unused(my_unused)) == sizeof(boost::type_traits::yes_type)));
}
