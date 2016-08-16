/*=============================================================================
    Copyright (c) 2015 Kohei Takahashi

    Use modification and distribution are subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
==============================================================================*/

#ifndef FUSION_TEST_SFINAE_FRIENDLY_HPP
#define FUSION_TEST_SFINAE_FRIENDLY_HPP

#include <boost/config.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/utility/result_of.hpp>
#include <boost/core/enable_if.hpp>

#if !defined(BOOST_NO_SFINAE) && defined(BOOST_RESULT_OF_USE_DECLTYPE)

#include <boost/fusion/container/vector.hpp>

namespace sfinae_friendly
{
    template <typename> struct arg_;
    template <typename R, typename T> struct arg_<R(T)> { typedef T type; };

    template <typename Traits, typename = void>
    struct check
        : boost::mpl::true_ { };

    template <typename Traits>
    struct check<Traits, typename boost::enable_if_has_type<typename Traits::type>::type>
        : boost::mpl::false_ { };

    struct unspecified {};
    typedef boost::fusion::vector<> v0;
    typedef boost::fusion::vector<unspecified> v1;
    typedef boost::fusion::vector<unspecified, unspecified> v2;
    typedef boost::fusion::vector<unspecified, unspecified, unspecified> v3;
}

#define SFINAE_FRIENDLY_ASSERT(Traits) \
    BOOST_MPL_ASSERT((::sfinae_friendly::check<typename ::sfinae_friendly::arg_<void Traits>::type>))

#else

#define SFINAE_FRIENDLY_ASSERT(Traits) \
    BOOST_MPL_ASSERT((boost::mpl::true_))

#endif

#endif // FUSION_TEST_SFINAE_FRIENDLY_HPP

