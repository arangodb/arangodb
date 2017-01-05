/*!
@file
Defines `boost::hana::Hashable`.

@copyright Louis Dionne 2016
@copyright Jason Rice 2016
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HANA_CONCEPT_HASHABLE_HPP
#define BOOST_HANA_CONCEPT_HASHABLE_HPP

#include <boost/hana/fwd/concept/hashable.hpp>

#include <boost/hana/config.hpp>
#include <boost/hana/core/default.hpp>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/hash.hpp>


BOOST_HANA_NAMESPACE_BEGIN
    template <typename T>
    struct Hashable {
        using Tag = typename tag_of<T>::type;
        static constexpr bool value = !is_default<hash_impl<Tag>>::value;
    };
BOOST_HANA_NAMESPACE_END

#endif // !BOOST_HANA_CONCEPT_HASHABLE_HPP
