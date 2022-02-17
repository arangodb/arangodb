// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_ROPE_FWD_HPP
#define BOOST_TEXT_ROPE_FWD_HPP

#include <boost/text/normalize_fwd.hpp>
#include <boost/text/concepts.hpp>
#include <boost/text/transcode_iterator.hpp>


namespace boost { namespace text {

    /** A reference to a substring of a `basic_rope`, `basic_text`, or
        `basic_text_view`.  The `String` template parameter refers to the type
        used in a `basic_rope` to which this view may refer.  It is otherwise
        unused. */
    template<
        nf Normalization,
        typename Char = char,
        typename String = std::basic_string<Char>>
#if BOOST_TEXT_USE_CONCEPTS
        // clang-format off
        requires utf8_code_unit<Char> || utf16_code_unit<Char>
#endif
    struct basic_rope_view;
    // clang-format on

    /** A mutable sequence of graphemes with copy-on-write semantics.  A
        `basic_rope` is non-contiguous and is not null-terminated.  The
        underlying storage is an unencoded_rope that is UTF-8-encoded and kept
        in normalization form `Normalization`. */
    template<
        nf Normalization,
        typename Char = char,
        typename String = std::basic_string<Char>>
#if BOOST_TEXT_USE_CONCEPTS
        // clang-format off
        requires (utf8_code_unit<Char> || utf16_code_unit<Char>) &&
            std::is_same_v<Char, std::ranges::range_value_t<String>>
#endif
    struct basic_rope;
    // clang-format on

    /** The specialization of `basic_rope` that should be used in most
        situations. */
    using rope = basic_rope<nf::c, char>;

    /** The specialization of `basic_rope_view` that should be used in most
        situations. */
    using rope_view = basic_rope_view<nf::c, char>;

    namespace detail {
        template<typename T, typename Iter, int Size = sizeof(T)>
        struct rope_transcode_iterator;
        template<typename T, typename Iter>
        struct rope_transcode_iterator<T, Iter, 1>
        {
            using type = utf_8_to_32_iterator<Iter>;
        };
        template<typename T, typename Iter>
        struct rope_transcode_iterator<T, Iter, 2>
        {
            using type = utf_16_to_32_iterator<Iter>;
        };
        template<typename T, typename Iter>
        using rope_transcode_iterator_t =
            typename rope_transcode_iterator<T, Iter>::type;
    }

}}

#endif
