// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_CONCEPTS_HPP
#define BOOST_TEXT_CONCEPTS_HPP

#include <boost/text/config.hpp>
#include <boost/text/utf.hpp>

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

#include <ranges>


namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    //[ concepts_concepts

    template<typename T, format F>
    concept code_unit = std::integral<T> && sizeof(T) == (int)F;

    template<typename T>
    concept utf8_code_unit = code_unit<T, format::utf8>;

    template<typename T>
    concept utf16_code_unit = code_unit<T, format::utf16>;

    template<typename T>
    concept utf32_code_unit = code_unit<T, format::utf32>;

    template<typename T, format F>
    concept code_unit_iterator =
        std::bidirectional_iterator<T> && code_unit<std::iter_value_t<T>, F>;

    template<typename T, format F>
    concept code_unit_pointer =
        std::is_pointer_v<T> && code_unit<std::iter_value_t<T>, F>;

    template<typename T, format F>
    concept code_unit_range = std::ranges::bidirectional_range<T> &&
        code_unit<std::ranges::range_value_t<T>, F>;

    template<typename T, format F>
    concept contiguous_code_unit_range = std::ranges::contiguous_range<T> &&
        code_unit<std::ranges::range_value_t<T>, F>;

    template<typename T>
    concept utf8_iter = code_unit_iterator<T, format::utf8>;
    template<typename T>
    concept utf8_pointer = code_unit_pointer<T, format::utf8>;
    template<typename T>
    concept utf8_range = code_unit_range<T, format::utf8>;
    template<typename T>
    concept contiguous_utf8_range = contiguous_code_unit_range<T, format::utf8>;

    template<typename T>
    concept utf16_iter = code_unit_iterator<T, format::utf16>;
    template<typename T>
    concept utf16_pointer = code_unit_pointer<T, format::utf16>;
    template<typename T>
    concept utf16_range = code_unit_range<T, format::utf16>;
    template<typename T>
    concept contiguous_utf16_range =
        contiguous_code_unit_range<T, format::utf16>;

    template<typename T>
    concept utf32_iter = code_unit_iterator<T, format::utf32>;
    template<typename T>
    concept utf32_pointer = code_unit_pointer<T, format::utf32>;
    template<typename T>
    concept utf32_range = code_unit_range<T, format::utf32>;
    template<typename T>
    concept contiguous_utf32_range =
        contiguous_code_unit_range<T, format::utf32>;

    template<typename T>
    concept code_point = utf32_code_unit<T>;
    template<typename T>
    concept code_point_iter = utf32_iter<T>;
    template<typename T>
    concept code_point_range = utf32_range<T>;


    template<typename T>
    concept grapheme_iter =
        // clang-format off
        std::bidirectional_iterator<T> &&
        code_point_range<std::iter_reference_t<T>> &&
        requires(T t) {
        { t.base() } -> code_point_iter;
        // clang-format on
    };

    template<typename T>
    concept grapheme_range = std::ranges::bidirectional_range<T> &&
        grapheme_iter<std::ranges::iterator_t<T>>;

    template<typename R>
    using code_point_iterator_t = decltype(std::declval<R>().begin().base());

    template<typename R>
    using code_point_sentinel_t = decltype(std::declval<R>().end().base());

    template<typename T, format F>
    concept grapheme_iter_code_unit =
        // clang-format off
        grapheme_iter<T> &&
        requires(T t) {
        { t.base().base() } -> code_unit_iterator<F>;
        // clang-format on
    };

    template<typename T, format F>
    concept grapheme_range_code_unit = grapheme_range<T> &&
        grapheme_iter_code_unit<std::ranges::iterator_t<T>, F>;


    namespace dtl {
        template<typename T>
        concept eraseable_sized_bidi_range =
            // clang-format off
            std::ranges::sized_range<T> &&
            std::ranges::bidirectional_range<T> && requires(T t) {
            { t.erase(t.begin(), t.end()) } ->
                std::same_as<std::ranges::iterator_t<T>>;
            // clang-format on
        };
    }

    template<typename T>
    concept utf8_string =
        // clang-format off
        dtl::eraseable_sized_bidi_range<T> &&
        utf8_code_unit<std::ranges::range_value_t<T>> &&
        requires(T t, char const * it) {
        { t.insert(t.end(), it, it) } ->
            std::same_as<std::ranges::iterator_t<T>>;
        // clang-format on
    };

    template<typename T>
    concept utf16_string =
        // clang-format off
        dtl::eraseable_sized_bidi_range<T> &&
        utf16_code_unit<std::ranges::range_value_t<T>> &&
        requires(T t, uint16_t const * it) {
        { t.insert(t.end(), it, it) } ->
            std::same_as<std::ranges::iterator_t<T>>;
        // clang-format on
    };

    template<typename T>
    concept utf_string = utf8_string<T> || utf16_string<T>;

    template<typename T>
    // clang-format off
    concept transcoding_error_handler = requires(T t) {
        { t("") } -> code_point;
        // clang-format on
    };

    template<typename T>
    concept utf_iter = utf8_iter<T> || utf16_iter<T> || utf32_iter<T>;

    template<typename T>
    // clang-format off
    concept utf_range_like =
        utf8_range<std::remove_reference_t<T>> ||
        utf16_range<std::remove_reference_t<T>> ||
        utf32_range<std::remove_reference_t<T>> ||
        utf8_pointer<std::remove_reference_t<T>> ||
        utf16_pointer<std::remove_reference_t<T>> ||
        utf32_pointer<std::remove_reference_t<T>>;
    // clang-format on

    //]

}}}

#endif

#endif
