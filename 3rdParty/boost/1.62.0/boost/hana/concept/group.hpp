/*!
@file
Defines `boost::hana::Group`.

@copyright Louis Dionne 2013-2016
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HANA_CONCEPT_GROUP_HPP
#define BOOST_HANA_CONCEPT_GROUP_HPP

#include <boost/hana/fwd/concept/group.hpp>

#include <boost/hana/config.hpp>
#include <boost/hana/core/default.hpp>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/negate.hpp>


BOOST_HANA_NAMESPACE_BEGIN
    template <typename G>
    struct Group {
        using Tag = typename tag_of<G>::type;
        static constexpr bool value = !is_default<negate_impl<Tag>>::value ||
                                      !is_default<minus_impl<Tag, Tag>>::value;
    };
BOOST_HANA_NAMESPACE_END

#endif // !BOOST_HANA_CONCEPT_GROUP_HPP
