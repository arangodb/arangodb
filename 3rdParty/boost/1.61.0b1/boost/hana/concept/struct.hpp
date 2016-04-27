/*!
@file
Defines `boost::hana::Struct`.

@copyright Louis Dionne 2013-2016
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HANA_CONCEPT_STRUCT_HPP
#define BOOST_HANA_CONCEPT_STRUCT_HPP

#include <boost/hana/fwd/concept/struct.hpp>

#include <boost/hana/accessors.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/core/default.hpp>
#include <boost/hana/core/tag_of.hpp>


BOOST_HANA_NAMESPACE_BEGIN
    template <typename S>
    struct Struct {
        using Tag = typename tag_of<S>::type;
        static constexpr bool value = !is_default<accessors_impl<Tag>>::value;
    };
BOOST_HANA_NAMESPACE_END

#endif // !BOOST_HANA_CONCEPT_STRUCT_HPP
