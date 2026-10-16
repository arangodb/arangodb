// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_COLLATION_FWD_HPP
#define BOOST_TEXT_COLLATION_FWD_HPP

#include <boost/text/config.hpp>

#include <boost/assert.hpp>

#include <cstdint>


namespace boost { namespace text {

    /** Collation strength.

        \see https://www.unicode.org/reports/tr10/#Multi_Level_Comparison */
    enum class collation_strength {
        primary,
        secondary,
        tertiary,
        quaternary,
        identical
    };

    /** Collation variable weighting.

        \see https://www.unicode.org/reports/tr10/#Variable_Weighting */
    enum class variable_weighting { non_ignorable, shifted };

    /** The order of L2 collation weights.  Only Canandian French uses
        backward. */
    enum class l2_weight_order { forward, backward };

    /** Controls whether a notional case level used in a tailored collation
        table.

        \see
        https://www.unicode.org/reports/tr35/tr35-collation.html#Case_Parameters
    */
    enum class case_level { on, off };

    /** Controls whether a preference is given to upper- or lower-case code
        points in a tailored collation table.

        \see
        https://www.unicode.org/reports/tr35/tr35-collation.html#Case_Parameters
    */
    enum class case_first { upper, lower, off };

    template<typename CharIter>
    struct read_table_result;

    struct collation_table;

    /** The flags taken by most functions in the collation search API, plus
        other functions that compare collated text or created collation keys.
        These flags are used to indicate desired properties for
        collation-aware comparisons. */
    enum class collation_flags : unsigned int {
        /** Use the default collation settings. */
        none = 0,

        /** Use a collation strength (primary) that disregards presence of or
            differences in accents. */
        ignore_accents = 1 << 0,

        /** Use a collation strength (secondary or higher) that disregards
            differences in case. */
        ignore_case = 1 << 1,

        /** Use a variable-weighting setting (shifted) that shifts the primary
            weights of punctuation to be quaternary weights instead, and use a
            collation strength that disregards quaternary weights (tertiary or
            higher).  It is an error to use this flag along with
            punctuation_last. */
        ignore_punctuation = 1 << 2,

        /** Consider case before accents, by making a pseudo-level for case
            weights.  The default behavior is to consider accent differences
            before case differences.  This flag only affects collation-aware
            operations that care about ordering of text.  Operations like
            search that only care about equality of text are unaffected. */
        case_before_accents = 1 << 3,

        /** Use a collation strength (tertiary) that considers case, and sort
            an upper-case code point before the lower-case code point for the
            same letter.  It is an error to use this flag along with
            lower_case_first or ignore_case.  This flag only affects
            collation-aware operations that care about ordering of
            text. Operations like search that only care about equality of text
            are unaffected. */
        upper_case_first = 1 << 4,

        /** Use a collation strength (tertiary) that considers case, and sort
            a lower-case code point before the upper-case code point for the
            same letter.  It is an error to use this flag along with
            upper_case_first or ignore_case.  This flag only affects
            collation-aware operations that care about ordering of text.
            Operations like search that only care about equality of text are
            unaffected. */
        lower_case_first = 1 << 5,

        /** Use a variable-weighting setting (shifted) that shifts the primary
            weights of punctuation to be quaternary weights instead, and use a
            collation strength (quaternary) that includes those weights.  It
            is an error to use this flag along with ignore_punctuation.  This
            flag only affects collation-aware operations that care about
            ordering of text.  Operations like search that only care about
            equality of text are unaffected. */
        punctuation_last = 1 << 6,

        /** Reverse the order in which the accent (secondary) weights appear.
            At the time of this writing, this is only done for Canadian
            French. */
        accents_reversed = 1 << 7
    };

    inline constexpr collation_flags
    operator|(collation_flags lhs, collation_flags rhs)
    {
#if !defined(BOOST_TEXT_NO_CXX14_CONSTEXPR) && defined(DEBUG)
        auto const lhs_ = static_cast<unsigned int>(lhs);
        auto const rhs_ = static_cast<unsigned int>(rhs);

        auto const ignore_case_uint =
            static_cast<unsigned int>(collation_flags::ignore_case);
        auto const ignore_punct_uint =
            static_cast<unsigned int>(collation_flags::ignore_punctuation);
        auto const punct_last_uint =
            static_cast<unsigned int>(collation_flags::punctuation_last);
        auto const upper_first_uint =
            static_cast<unsigned int>(collation_flags::upper_case_first);
        auto const lower_first_uint =
            static_cast<unsigned int>(collation_flags::lower_case_first);

        bool const ignore_case =
            (lhs_ & ignore_case_uint) || (rhs_ & ignore_case_uint);
        bool const ignore_punct =
            (lhs_ & ignore_punct_uint) || (rhs_ & ignore_punct_uint);
        bool const punct_last =
            (lhs_ & punct_last_uint) || (rhs_ & punct_last_uint);
        bool const upper_first =
            (lhs_ & upper_first_uint) || (rhs_ & upper_first_uint);
        bool const lower_first =
            (lhs_ & lower_first_uint) || (rhs_ & lower_first_uint);

        BOOST_ASSERT(
            (!ignore_punct || !punct_last) && "Incompatible collation_flags");
        BOOST_ASSERT(
            (!upper_first || !lower_first) && "Incompatible collation_flags");
        BOOST_ASSERT(
            (!upper_first || !ignore_case) && "Incompatible collation_flags");
        BOOST_ASSERT(
            (!lower_first || !ignore_case) && "Incompatible collation_flags");
#endif

        return collation_flags(
            static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
    }

    namespace detail {

        enum class retain_case_bits_t { no, yes };

        enum case_bits : uint16_t {
            lower_case_bits = 0x0000,
            mixed_case_bits = 0x4000,
            upper_case_bits = 0x8000
        };

        inline uint32_t
        replace_lead_byte(uint32_t l1_weight, uint32_t new_lead_byte)
        {
            l1_weight &= 0x00ffffff;
            l1_weight |= new_lead_byte;
            return l1_weight;
        }

        inline unsigned int to_uint(collation_flags flag)
        {
            return static_cast<unsigned int>(flag);
        }

        inline collation_strength to_strength(collation_flags flags_)
        {
            unsigned int const flags = static_cast<unsigned int>(flags_);

            BOOST_ASSERT(
                (!(flags &
                   detail::to_uint(collation_flags::ignore_punctuation)) ||
                 !(flags &
                   detail::to_uint(collation_flags::punctuation_last))) &&
                "These flags are mutually incompatible.");

            BOOST_ASSERT(
                (!(flags &
                   detail::to_uint(collation_flags::lower_case_first)) ||
                 !(flags & detail::to_uint(collation_flags::ignore_case))) &&
                "These flags are mutually incompatible.");

            BOOST_ASSERT(
                (!(flags &
                   detail::to_uint(collation_flags::upper_case_first)) ||
                 !(flags & detail::to_uint(collation_flags::ignore_case))) &&
                "These flags are mutually incompatible.");

            if (flags & detail::to_uint(collation_flags::ignore_accents))
                return collation_strength::primary;
            else if (flags & detail::to_uint(collation_flags::ignore_case))
                return collation_strength::secondary;
            else if (flags & detail::to_uint(collation_flags::punctuation_last))
                return collation_strength::quaternary;
            else
                return collation_strength::tertiary;
        }

        inline case_level to_case_level(collation_flags flags_)
        {
            unsigned int const flags = static_cast<unsigned int>(flags_);

            if (flags & detail::to_uint(collation_flags::case_before_accents) ||
                (flags & detail::to_uint(collation_flags::ignore_accents) &&
                 !(flags & detail::to_uint(collation_flags::ignore_case)))) {
                return case_level::on;
            } else {
                return case_level::off;
            }
        }

        inline case_first to_case_first(collation_flags flags_)
        {
            unsigned int const flags = static_cast<unsigned int>(flags_);

            BOOST_ASSERT(
                (!(flags &
                   detail::to_uint(collation_flags::lower_case_first)) ||
                 !(flags &
                   detail::to_uint(collation_flags::upper_case_first))) &&
                "These flags are mutually incompatible.");

            if (flags & detail::to_uint(collation_flags::lower_case_first))
                return case_first::lower;
            else if (flags & detail::to_uint(collation_flags::upper_case_first))
                return case_first::upper;
            else
                return case_first::off;
        }

        inline variable_weighting to_weighting(collation_flags flags_)
        {
            unsigned int const flags = static_cast<unsigned int>(flags_);

            if (flags & detail::to_uint(collation_flags::ignore_punctuation))
                return variable_weighting::shifted;
            else
                return variable_weighting::non_ignorable;
        }

        inline l2_weight_order to_l2_order(collation_flags flags_)
        {
            unsigned int const flags = static_cast<unsigned int>(flags_);

            if (flags & detail::to_uint(collation_flags::accents_reversed))
                return l2_weight_order::backward;
            else
                return l2_weight_order::forward;
        }

        template<
            typename CPIter1,
            typename Sentinel1,
            typename CPIter2,
            typename Sentinel2>
        inline int collate(
            CPIter1 lhs_first,
            Sentinel1 lhs_last,
            CPIter2 rhs_first,
            Sentinel2 rhs_last,
            collation_strength strength,
            case_first case_1st,
            case_level case_lvl,
            variable_weighting weighting,
            l2_weight_order l2_order,
            collation_table const & table);
    }

}}

#endif
