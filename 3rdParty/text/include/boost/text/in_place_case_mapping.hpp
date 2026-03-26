// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_IN_PLACE_CASE_MAPPING_HPP
#define BOOST_TEXT_IN_PLACE_CASE_MAPPING_HPP

#include <boost/text/text.hpp>
#include <boost/text/rope.hpp>
#include <boost/text/case_mapping.hpp>


namespace boost { namespace text {

    /** Changes the case of `t` to lower-case, using language-specific
        handling as indicated by `lang`. */
    inline void in_place_to_lower(
        text & t, case_language lang = case_language::other) noexcept
    {
        std::string s;
        boost::text::to_lower(
            t.begin().base(),
            t.begin().base(),
            t.end().base(),
            boost::text::from_utf32_inserter(s, s.end()),
            lang);
        if (s.size() < t.storage_code_units()) {
            t = s;
        } else {
            boost::text::normalize<text::normalization>(s);
            t.replace(std::move(s));
        }
    }

    /** Changes the case of `r` to lower-case, using language-specific
        handling as indicated by `lang`. */
    inline void in_place_to_lower(
        rope & r, case_language lang = case_language::other) noexcept
    {
        std::string s;
        boost::text::to_lower(
            r.begin().base(),
            r.begin().base(),
            r.end().base(),
            boost::text::from_utf32_inserter(s, s.end()),
            lang);
        r = s;
    }

    /** Changes the case of `t` to title-case, using language-specific
        handling as indicated by `lang`.

        NextWordBreakFunc must meet the same type requirements as the
        `NextWordBreakFunc` template parameter to `to_title()`. */
    template<typename NextWordBreakFunc = next_word_break_callable>
    void in_place_to_title(
        text & t,
        case_language lang = case_language::other,
        NextWordBreakFunc next_word_break = NextWordBreakFunc{}) noexcept
    {
        std::string s;
        boost::text::to_title(
            t.begin().base(),
            t.begin().base(),
            t.end().base(),
            boost::text::from_utf32_inserter(s, s.end()),
            lang,
            next_word_break);
        if (s.size() < t.storage_code_units()) {
            t = s;
        } else {
            boost::text::normalize<text::normalization>(s);
            t.replace(std::move(s));
        }
    }

    /** Changes the case of `r` to title-case, using language-specific
        handling as indicated by `lang`.

        NextWordBreakFunc must meet the same type requirements as the
        `NextWordBreakFunc` template parameter to `to_title()`. */
    template<typename NextWordBreakFunc = next_word_break_callable>
    void in_place_to_title(
        rope & r,
        case_language lang = case_language::other,
        NextWordBreakFunc next_word_break = NextWordBreakFunc{}) noexcept
    {
        std::string s;
        boost::text::to_title(
            r.begin().base(),
            r.begin().base(),
            r.end().base(),
            boost::text::from_utf32_inserter(s, s.end()),
            lang,
            next_word_break);
        r = s;
    }

    /** Changes the case of `t` to upper-case, using language-specific
        handling as indicated by `lang`. */
    inline void in_place_to_upper(
        text & t, case_language lang = case_language::other) noexcept
    {
        std::string s;
        boost::text::to_upper(
            t.begin().base(),
            t.begin().base(),
            t.end().base(),
            boost::text::from_utf32_inserter(s, s.end()),
            lang);
        if (s.size() < t.storage_code_units()) {
            t = s;
        } else {
            boost::text::normalize<text::normalization>(s);
            t.replace(std::move(s));
        }
    }

    /** Changes the case of `r` to upper-case, using language-specific
        handling as indicated by `lang`. */
    inline void in_place_to_upper(
        rope & r, case_language lang = case_language::other) noexcept
    {
        std::string s;
        boost::text::to_upper(
            r.begin().base(),
            r.begin().base(),
            r.end().base(),
            boost::text::from_utf32_inserter(s, s.end()),
            lang);
        r = s;
    }

}}

#endif
