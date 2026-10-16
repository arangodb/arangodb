// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_UNENCODED_ROPE_FWD_HPP
#define BOOST_TEXT_UNENCODED_ROPE_FWD_HPP

#include <string>


namespace boost { namespace text {

    /** A reference to a substring of a `basic_unencoded_rope`,
        `std::string<Char>`, or `std::string_view<Char>`.  The `String`
        template parameter refers to the type used in a `basic_unencoded_rope`
        to which this view may refer.  It is otherwise unused. */
    template<typename Char, typename String = std::basic_string<Char>>
#if BOOST_TEXT_USE_CONCEPTS
    // clang-format off
        requires std::is_same_v<Char, std::ranges::range_value_t<String>>
#endif
    struct basic_unencoded_rope_view;
    // clang-format on

    /** A mutable sequence of `Char` with copy-on-write semantics, and very
        cheap mutations at any point in the sequence (beginning, middle, or
        end).  A `basic_unencoded_rope` is non-contiguous and is not
        null-terminated. */
    template<typename Char, typename String = std::basic_string<Char>>
#if BOOST_TEXT_USE_CONCEPTS
    // clang-format off
        requires std::is_same_v<Char, std::ranges::range_value_t<String>>
#endif
    struct basic_unencoded_rope;
    // clang-format on

    /** The specialization of `basic_unencoded_rope` that should be used in most
        situations. */
    using unencoded_rope = basic_unencoded_rope<char>;

    /** The specialization of `basic_unencoded_rope_view` that should be used
        in most situations. */
    using unencoded_rope_view = basic_unencoded_rope_view<char>;

}}

#endif
