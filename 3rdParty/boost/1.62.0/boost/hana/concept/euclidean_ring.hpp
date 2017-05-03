/*!
@file
Defines `boost::hana::EuclideanRing`.

@copyright Louis Dionne 2013-2016
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HANA_CONCEPT_EUCLIDEAN_RING_HPP
#define BOOST_HANA_CONCEPT_EUCLIDEAN_RING_HPP

#include <boost/hana/fwd/concept/euclidean_ring.hpp>

#include <boost/hana/config.hpp>
#include <boost/hana/core/default.hpp>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/div.hpp>
#include <boost/hana/mod.hpp>


BOOST_HANA_NAMESPACE_BEGIN
    template <typename R>
    struct EuclideanRing {
        using Tag = typename tag_of<R>::type;
        static constexpr bool value = !is_default<mod_impl<Tag, Tag>>::value &&
                                      !is_default<div_impl<Tag, Tag>>::value;
    };
BOOST_HANA_NAMESPACE_END

#endif // !BOOST_HANA_CONCEPT_EUCLIDEAN_RING_HPP
