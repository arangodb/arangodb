// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_COLLATE_HPP
#define BOOST_TEXT_COLLATE_HPP

#include <boost/text/algorithm.hpp>
#include <boost/text/collation_fwd.hpp>
#include <boost/text/normalize.hpp>
#include <boost/text/normalize_string.hpp>
#include <boost/text/detail/collation_data.hpp>

#include <boost/algorithm/cxx14/equal.hpp>
#include <boost/algorithm/cxx14/mismatch.hpp>
#include <boost/container/small_vector.hpp>

#include <vector>

#ifndef BOOST_TEXT_DOXYGEN

#ifndef BOOST_TEXT_COLLATE_INSTRUMENTATION
#define BOOST_TEXT_COLLATE_INSTRUMENTATION 0
#endif

#ifndef BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
#define BOOST_TEXT_INSTRUMENT_COLLATE_IMPL 0
#endif

#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
#include <iostream>
#endif

#endif


namespace boost { namespace text {

    /** A collation sort key.  Consists of a sequence of 32-bit values. */
    struct text_sort_key
    {
        using const_iterator = std::vector<uint32_t>::const_iterator;
        using iterator = const_iterator;
        using value_type = uint32_t;

        text_sort_key() {}
        explicit text_sort_key(std::vector<uint32_t> bytes) :
            storage_(std::move(bytes))
        {}

        std::size_t size() const noexcept { return storage_.size(); }

        const_iterator begin() const noexcept { return storage_.begin(); }
        const_iterator end() const noexcept { return storage_.end(); }

        friend bool
        operator==(text_sort_key const & lhs, text_sort_key const & rhs)
        {
            return algorithm::equal(
                lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
        }

        friend bool
        operator!=(text_sort_key const & lhs, text_sort_key const & rhs)
        {
            return !(lhs == rhs);
        }

        friend bool
        operator<(text_sort_key const & lhs, text_sort_key const & rhs)
        {
            return std::lexicographical_compare(
                lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
        }

        friend bool
        operator<=(text_sort_key const & lhs, text_sort_key const & rhs)
        {
            return !(rhs < lhs);
        }

        friend bool
        operator>(text_sort_key const & lhs, text_sort_key const & rhs)
        {
            return rhs < lhs;
        }

        friend bool
        operator>=(text_sort_key const & lhs, text_sort_key const & rhs)
        {
            return !(lhs < rhs);
        }

    private:
        std::vector<uint32_t> storage_;
    };

#if BOOST_TEXT_COLLATE_INSTRUMENTATION
    inline std::ostream & operator<<(std::ostream & os, text_sort_key const & k)
    {
        os << std::hex << "[";
        for (auto x : k) {
            os << " " << x;
        }
        os << " ]" << std::dec;
        return os;
    }
#endif

    /** Returns 0 if the given sort keys are equal, a value < 0 if `lhs` is
        less than `rhs`, and a value > 0 otherwise. */
    inline int
    compare(text_sort_key const & lhs, text_sort_key const & rhs) noexcept
    {
        return boost::text::lexicographical_compare_three_way(
            lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    // The code in this file implements the UCA as described in
    // http://www.unicode.org/reports/tr10/#Main_Algorithm .  The numbering
    // and some variable naming comes from there.
    namespace detail {

        // http://www.unicode.org/reports/tr10/#Derived_Collation_Elements
        template<typename OutIter, typename LeadByteFunc>
        inline OutIter add_derived_elements(
            uint32_t cp,
            OutIter out,
            detail::collation_trie_t const & trie,
            collation_element const * collation_elements_first,
            LeadByteFunc const & lead_byte)
        {
            // Core Han Unified Ideographs
            std::array<uint32_t, 12> const CJK_Compatibility_Ideographs = {
                {0xFA0E,
                 0xFA0F,
                 0xFA11,
                 0xFA13,
                 0xFA14,
                 0xFA1F,
                 0xFA21,
                 0xFA23,
                 0xFA24,
                 0xFA27,
                 0xFA28,
                 0xFA29}};

            std::array<uint32_t, 222> const CJK_Unified_Ideographs_Extension_D =
                {{0x2B740, 0x2B741, 0x2B742, 0x2B743, 0x2B744, 0x2B745, 0x2B746,
                  0x2B747, 0x2B748, 0x2B749, 0x2B74A, 0x2B74B, 0x2B74C, 0x2B74D,
                  0x2B74E, 0x2B74F, 0x2B750, 0x2B751, 0x2B752, 0x2B753, 0x2B754,
                  0x2B755, 0x2B756, 0x2B757, 0x2B758, 0x2B759, 0x2B75A, 0x2B75B,
                  0x2B75C, 0x2B75D, 0x2B75E, 0x2B75F, 0x2B760, 0x2B761, 0x2B762,
                  0x2B763, 0x2B764, 0x2B765, 0x2B766, 0x2B767, 0x2B768, 0x2B769,
                  0x2B76A, 0x2B76B, 0x2B76C, 0x2B76D, 0x2B76E, 0x2B76F, 0x2B770,
                  0x2B771, 0x2B772, 0x2B773, 0x2B774, 0x2B775, 0x2B776, 0x2B777,
                  0x2B778, 0x2B779, 0x2B77A, 0x2B77B, 0x2B77C, 0x2B77D, 0x2B77E,
                  0x2B77F, 0x2B780, 0x2B781, 0x2B782, 0x2B783, 0x2B784, 0x2B785,
                  0x2B786, 0x2B787, 0x2B788, 0x2B789, 0x2B78A, 0x2B78B, 0x2B78C,
                  0x2B78D, 0x2B78E, 0x2B78F, 0x2B790, 0x2B791, 0x2B792, 0x2B793,
                  0x2B794, 0x2B795, 0x2B796, 0x2B797, 0x2B798, 0x2B799, 0x2B79A,
                  0x2B79B, 0x2B79C, 0x2B79D, 0x2B79E, 0x2B79F, 0x2B7A0, 0x2B7A1,
                  0x2B7A2, 0x2B7A3, 0x2B7A4, 0x2B7A5, 0x2B7A6, 0x2B7A7, 0x2B7A8,
                  0x2B7A9, 0x2B7AA, 0x2B7AB, 0x2B7AC, 0x2B7AD, 0x2B7AE, 0x2B7AF,
                  0x2B7B0, 0x2B7B1, 0x2B7B2, 0x2B7B3, 0x2B7B4, 0x2B7B5, 0x2B7B6,
                  0x2B7B7, 0x2B7B8, 0x2B7B9, 0x2B7BA, 0x2B7BB, 0x2B7BC, 0x2B7BD,
                  0x2B7BE, 0x2B7BF, 0x2B7C0, 0x2B7C1, 0x2B7C2, 0x2B7C3, 0x2B7C4,
                  0x2B7C5, 0x2B7C6, 0x2B7C7, 0x2B7C8, 0x2B7C9, 0x2B7CA, 0x2B7CB,
                  0x2B7CC, 0x2B7CD, 0x2B7CE, 0x2B7CF, 0x2B7D0, 0x2B7D1, 0x2B7D2,
                  0x2B7D3, 0x2B7D4, 0x2B7D5, 0x2B7D6, 0x2B7D7, 0x2B7D8, 0x2B7D9,
                  0x2B7DA, 0x2B7DB, 0x2B7DC, 0x2B7DD, 0x2B7DE, 0x2B7DF, 0x2B7E0,
                  0x2B7E1, 0x2B7E2, 0x2B7E3, 0x2B7E4, 0x2B7E5, 0x2B7E6, 0x2B7E7,
                  0x2B7E8, 0x2B7E9, 0x2B7EA, 0x2B7EB, 0x2B7EC, 0x2B7ED, 0x2B7EE,
                  0x2B7EF, 0x2B7F0, 0x2B7F1, 0x2B7F2, 0x2B7F3, 0x2B7F4, 0x2B7F5,
                  0x2B7F6, 0x2B7F7, 0x2B7F8, 0x2B7F9, 0x2B7FA, 0x2B7FB, 0x2B7FC,
                  0x2B7FD, 0x2B7FE, 0x2B7FF, 0x2B800, 0x2B801, 0x2B802, 0x2B803,
                  0x2B804, 0x2B805, 0x2B806, 0x2B807, 0x2B808, 0x2B809, 0x2B80A,
                  0x2B80B, 0x2B80C, 0x2B80D, 0x2B80E, 0x2B80F, 0x2B810, 0x2B811,
                  0x2B812, 0x2B813, 0x2B814, 0x2B815, 0x2B816, 0x2B817, 0x2B818,
                  0x2B819, 0x2B81A, 0x2B81B, 0x2B81C, 0x2B81D}};

            double const spacing = implicit_weights_spacing_times_ten / 10.0;

            for (auto seg : make_implicit_weights_segments()) {
                if (seg.first_ <= cp && cp < seg.last_) {
                    if (seg.first_ == CJK_Compatibility_Ideographs[0] &&
                        ((cp & ~OR_CJK_Compatibility_Ideographs) ||
                         !std::count(
                             CJK_Compatibility_Ideographs.begin(),
                             CJK_Compatibility_Ideographs.end(),
                             cp))) {
                        continue;
                    }

                    if (seg.first_ == CJK_Unified_Ideographs_Extension_D[0] &&
                        ((cp & ~OR_CJK_Unified_Ideographs_Extension_D) ||
                         !std::binary_search(
                             CJK_Unified_Ideographs_Extension_D.begin(),
                             CJK_Unified_Ideographs_Extension_D.end(),
                             cp))) {
                        continue;
                    }

                    uint32_t const primary_weight_low_bits =
                        seg.primary_offset_ + (cp - seg.first_) * spacing;
                    BOOST_ASSERT(
                        (primary_weight_low_bits & 0xfffff) ==
                        primary_weight_low_bits);
                    uint32_t const bytes[4] = {
                        implicit_weights_first_lead_byte,
                        ((primary_weight_low_bits >> 12) & 0xfe) | 0x1,
                        ((primary_weight_low_bits >> 5) & 0xfe) | 0x1,
                        (primary_weight_low_bits >> 0) & 0x3f};
                    uint32_t const primary = bytes[0] << 24 | bytes[1] << 16 |
                                             bytes[2] << 8 | bytes[3] << 0;
                    collation_element ce{primary, 0x0500, 0x0500, 0x0};

                    ce.l1_ = detail::replace_lead_byte(ce.l1_, lead_byte(ce));

                    *out++ = ce;
                    return out;
                }
            }

            // This is not tailorable, so we won't use lead_byte here.
            *out++ = collation_element{
                (implicit_weights_final_lead_byte << 24) | (cp & 0xffffff),
                0x0500,
                0x0500,
                0x0};
            return out;
        }

        inline bool ignorable(collation_element ce) noexcept
        {
            return ce.l1_ == 0;
        }

        template<typename CEIter>
        inline void s2_3_case_bits(CEIter first, CEIter last)
        {
            for (auto it = first; it != last; ++it) {
                auto & ce = *it;
                // The top two bits in each byte in FractionalUCA.txt's L3
                // weights are for the case level.
                // http://www.unicode.org/reports/tr35/tr35-collation.html#File_Format_FractionalUCA_txt
                uint16_t const l3 = ce.l3_ & disable_case_level_mask;

                ce.l3_ = l3;
            }
        }

        // http://www.unicode.org/reports/tr10/#Variable_Weighting
        template<typename CEIter>
        inline bool s2_3(
            CEIter first,
            CEIter last,
            collation_strength strength,
            variable_weighting weighting,
            bool after_variable,
            retain_case_bits_t retain_case_bits)
        {
            // http://www.unicode.org/reports/tr10/#Implicit_Weights says: "If
            // a fourth or higher weights are used, then the same pattern is
            // followed for those weights. They are set to a non-zero value in
            // the first collation element and zero in the second."
            //
            // Even though this appears in the section on implicit weights
            // that "do not have explicit mappings in the DUCET", this
            // apparently applies to any pair of collation elements that
            // matches the pattern produced by the derived weight algorithm,
            // since that's what CollationTest_SHIFTED.txt expects.
            bool second_of_implicit_weight_pair = false;

            for (auto it = first; it != last; ++it) {
                auto & ce = *it;
                if (after_variable && detail::ignorable(ce)) {
                    ce.l1_ = 0;
                    ce.l2_ = 0;
                    ce.l3_ = 0;
                    ce.l4_ = 0;
                } else if (!ce.l1_) {
                    if (!ce.l2_ && !ce.l3_) {
                        ce.l4_ = 0x0000;
                    } else if (ce.l3_) {
                        if (after_variable)
                            ce.l4_ = 0x0000;
                        else
                            ce.l4_ = 0xffffffff;
                    }
                    after_variable = false;
                } else if (detail::variable(ce)) {
                    ce.l4_ = ce.l1_;
                    ce.l1_ = 0;
                    ce.l2_ = 0;
                    ce.l3_ = 0;
                    after_variable = true;
                } else {
                    if (ce.l1_)
                        ce.l4_ = 0xffffffff;
                    after_variable = false;
                }
                if (second_of_implicit_weight_pair) {
                    ce.l4_ = 0;
                    second_of_implicit_weight_pair = false;
                }
#if 0
                 // Not necessary with FractionalUCA.txt-derived data, in
                 // which each implicit weight before the unassigned code
                 // points is only one CE.
                second_of_implicit_weight_pair =
                    implicit_weights_first_lead_byte <= l1 &&
                    l1 <= implicit_weights_final_lead_byte;
#endif
            }

            return after_variable;
        }

        inline std::array<bool, 272> const &
        get_derived_element_high_two_bytes()
        {
            static std::array<bool, 272> retval = {{}};
            for (auto seg : make_implicit_weights_segments()) {
                for (uint32_t i = (seg.first_ >> 12),
                              end = (seg.last_ >> 12) + 1;
                     i != end;
                     ++i) {
                    BOOST_ASSERT(i < 256u);
                    retval[i] = true;
                }
            }
            return retval;
        }

        template<typename CPIter>
        CPIter s2_1_1(CPIter first, CPIter last, trie_match_t collation)
        {
            // S2.1.1 Process any nonstarters following S.
            auto retval = first;
            if (!collation.leaf) {
                retval = std::find_if(first, last, [](uint32_t cp) {
                    return detail::ccc(cp) == 0;
                });
            }
            return retval;
        }

        template<typename CPIter>
        trie_match_t s2_1_2(
            CPIter & first,
            CPIter nonstarter_last,
            trie_match_t collation,
            detail::collation_trie_t const & trie,
            bool primaries_only = false)
        {
            // S2.1.2
            auto nonstarter_first = first;
            while (!collation.leaf && nonstarter_first != nonstarter_last &&
                   detail::ccc(*(nonstarter_first - 1)) <
                       detail::ccc(*nonstarter_first)) {
                auto const cp = *nonstarter_first;
                auto coll = trie.extend_subsequence(collation, cp);
                // S2.1.3
                if (coll.match && collation.size < coll.size) {
                    std::copy_backward(
                        first, nonstarter_first, nonstarter_first + 1);
                    *first++ = cp;
                    collation = coll;
                } else if (primaries_only) {
                    ++first;
                }
                ++nonstarter_first;
            }

            return collation;
        }

        template<typename CPIter>
        trie_match_t s2_1(
            CPIter & first,
            CPIter last,
            trie_match_t collation,
            detail::collation_trie_t const & trie)
        {
            auto nonstarter_last = detail::s2_1_1(first, last, collation);
            return detail::s2_1_2(first, nonstarter_last, collation, trie);
        }

        template<
            typename CPIter,
            typename CPOutIter,
            typename LeadByteFunc,
            typename SizeOutIter = std::ptrdiff_t *>
        auto
        s2(CPIter first,
           CPIter last,
           CPOutIter out,
           detail::collation_trie_t const & trie,
           collation_element const * collation_elements_first,
           LeadByteFunc const & lead_byte,
           collation_strength strength,
           variable_weighting weighting,
           retain_case_bits_t retain_case_bits,
           SizeOutIter * size_out = nullptr)
            -> detail::cp_iter_ret_t<CPOutIter, CPIter>
        {
            std::array<bool, 272> const & derived_element_high_two_bytes =
                get_derived_element_high_two_bytes();

            bool after_variable = false;
            while (first != last) {
                // S2.1 Find longest prefix that results in a collation trie
                // match.
                trie_match_t collation_;
                collation_ = trie.longest_match(first, last);
                if (!collation_.match) {
                    // S2.2
                    uint32_t cp = *first++;
                    if (detail::hangul_syllable(cp)) {
                        auto cps = detail::decompose_hangul_syllable<3>(cp);
                        out = detail::s2(
                            cps.begin(),
                            cps.end(),
                            out,
                            trie,
                            collation_elements_first,
                            lead_byte,
                            strength,
                            weighting,
                            retain_case_bits);
                        if (size_out)
                            *(*size_out)++ = 1;
                        continue;
                    }

                    if (!derived_element_high_two_bytes[cp >> 12]) {
                        // This is not tailorable, so we won't use lead_byte
                        // here.
                        *out++ = collation_element{
                            (implicit_weights_final_lead_byte << 24) |
                                (cp & 0xffffff),
                            0x0500,
                            0x0500,
                            0x0};
                        if (size_out)
                            *(*size_out)++ = 1;
                        continue;
                    }

                    collation_element derived_ces[32];
                    auto const derived_ces_end = detail::add_derived_elements(
                        cp,
                        derived_ces,
                        trie,
                        collation_elements_first,
                        lead_byte);
                    if (weighting == variable_weighting::shifted &&
                        strength != collation_strength::primary) {
                        after_variable = detail::s2_3(
                            derived_ces,
                            derived_ces_end,
                            strength,
                            weighting,
                            after_variable,
                            retain_case_bits);
                    }
                    out = std::copy(derived_ces, derived_ces_end, out);
                    if (size_out)
                        *(*size_out)++ = 1;
                    continue;
                }

                {
                    auto it_16 = boost::text::as_utf16(first, last).begin();
                    auto s = collation_.size;
                    int cps = 0;
                    while (s) {
                        if (boost::text::high_surrogate(*it_16)) {
                            s -= 2;
                            std::advance(it_16, 2);
                        } else {
                            s -= 1;
                            ++it_16;
                        }
                        ++cps;
                    }
                    first += cps;
                }

                // S2.1
                collation_ = detail::s2_1(first, last, collation_, trie);

                auto const & collation_value = *trie[collation_];

                // S2.4
                auto const initial_out = out;
                out = std::copy(
                    collation_value.begin(collation_elements_first),
                    collation_value.end(collation_elements_first),
                    out);

                // S2.3
                if (retain_case_bits == retain_case_bits_t::no &&
                    collation_strength::tertiary <= strength) {
                    detail::s2_3_case_bits(initial_out, out);
                }
                if (weighting != variable_weighting::non_ignorable &&
                    strength != collation_strength::primary) {
                    after_variable = detail::s2_3(
                        initial_out,
                        out,
                        strength,
                        weighting,
                        after_variable,
                        retain_case_bits);
                }

                if (size_out) {
                    *(*size_out)++ = collation_value.size();
                    for (int i = 1; i < collation_.size; ++i) {
                        *(*size_out)++ = 0;
                    }
                }
            }

            return out;
        }

        using level_sort_key_values_t = container::small_vector<uint32_t, 256>;
        using level_sort_key_bytes_t = container::small_vector<uint8_t, 1024>;

        // In-place compression of values such that 8-bit byte values are
        // packed into a 32-bit dwords (e.g. 0x0000XX00, 0x0000YYZZ ->
        // 0x00XXYYZZ), based on
        // https://www.unicode.org/reports/tr10/#Reducing_Sort_Key_Lengths
        // 9.1.3.
        inline level_sort_key_bytes_t
        pack_words(level_sort_key_values_t const & values)
        {
            level_sort_key_bytes_t retval;

            // We cannot treat the inputs naively as a sequence of bytes,
            // because we don't know the endianness.
            for (auto x : values) {
                uint8_t const bytes[4] = {
                    uint8_t(x >> 24),
                    uint8_t((x >> 16) & 0xff),
                    uint8_t((x >> 8) & 0xff),
                    uint8_t(x & 0xff),
                };
                if (bytes[0])
                    retval.push_back(bytes[0]);
                if (bytes[1])
                    retval.push_back(bytes[1]);
                if (bytes[2])
                    retval.push_back(bytes[2]);
                if (bytes[3])
                    retval.push_back(bytes[3]);
            }

            return retval;
        }

        // In-place run-length encoding, based on
        // https://www.unicode.org/reports/tr10/#Reducing_Sort_Key_Lengths
        // 9.1.4.
        inline void
        rle(level_sort_key_bytes_t & bytes,
            uint8_t min_,
            uint8_t common,
            uint8_t max_)
        {
            uint8_t const min_top = (common - 1) - (min_ - 1);
            uint8_t const max_bottom = (common + 1) + (0xff - max_);
            int const bound = (min_top + max_bottom) / 2;

            auto it = bytes.begin();
            auto const end = bytes.end();
            auto out = bytes.begin();
            while (it != end) {
                if (*it == common) {
                    auto const last_common =
                        boost::text::find_not(it, end, common);
                    auto const size = last_common - it;
                    if (last_common == end || *last_common < common) {
                        int const synthetic_low = min_top + size;
                        if (bound <= synthetic_low) {
                            auto const max_compressible_copies =
                                (bound - 1) - min_top;
                            auto const repetitions =
                                size / max_compressible_copies;
                            out = std::fill_n(out, repetitions, bound - 1);
                            auto const remainder =
                                size % max_compressible_copies;
                            if (remainder)
                                *out++ = min_top + remainder;
                        } else {
                            *out++ = synthetic_low;
                        }
                    } else {
                        int const synthetic_high = max_bottom - size;
                        if (synthetic_high < bound) {
                            auto const max_compressible_copies =
                                max_bottom - bound;
                            auto const repetitions =
                                size / max_compressible_copies;
                            out = std::fill_n(out, repetitions, bound);
                            auto const remainder =
                                size % max_compressible_copies;
                            if (remainder)
                                *out++ = max_bottom - remainder;
                        } else {
                            *out++ = synthetic_high;
                        }
                    }
                    it = last_common;
                } else {
                    if (min_ <= *it && *it < common)
                        *out = *it - (min_ - 1);
                    else if (common < *it && *it <= max_)
                        *out = *it + 0xff - max_;
                    else
                        *out = *it;
                    ++it;
                    ++out;
                }
            }

            bytes.erase(out, end);
        }

        inline void pad_words(level_sort_key_bytes_t & bytes)
        {
            int remainder = bytes.size() % 4;
            if (remainder) {
                remainder = 4 - remainder;
                bytes.resize(bytes.size() + remainder, 0);
            }
        }

        inline void level_bytes_to_values(
            level_sort_key_bytes_t const & bytes,
            level_sort_key_values_t & values)
        {
            BOOST_ASSERT(bytes.size() % 4 == 0);

            values.resize(bytes.size() / 4);

            auto out = values.begin();
            for (auto it = bytes.begin(), end = bytes.end(); it != end;
                 it += 4) {
                uint32_t const x = *(it + 0) << 24 | *(it + 1) << 16 |
                                   *(it + 2) << 8 | *(it + 3) << 0;
                *out++ = x;
            }
        }

        // https://www.unicode.org/reports/tr35/tr35-collation.html#Case_Weights
        inline collation_element modify_for_case(
            collation_element ce,
            collation_strength strength,
            case_first case_1st,
            case_level case_lvl) noexcept
        {
            if (case_1st == case_first::off && case_lvl == case_level::off) {
                ce.l3_ &= disable_case_level_mask;
                return ce;
            }

            uint16_t c = 0; // Set 1, 2, or 3 below.
            auto const case_bits = ce.l3_ & case_level_bits_mask;

            if (case_1st == case_first::upper) {
                c = (case_bits == upper_case_bits)
                        ? 1
                        : ((case_bits == mixed_case_bits) ? 2 : 3);
            } else {
                c = (case_bits == upper_case_bits)
                        ? 3
                        : ((case_bits == mixed_case_bits) ? 2 : 1);
            }

            if (case_lvl == case_level::on) {
                if (strength == collation_strength::primary) {
                    // Ensure we use values >= min_secondary_byte.
                    c += min_secondary_byte - 1;
                    if (!ce.l1_)
                        ce.l2_ = 0;
                    else
                        ce.l2_ = c << 8; // Shift bits into lead L2 byte.
                    ce.l3_ = 0;
                } else {
                    ce.l4_ = ce.l3_ & disable_case_level_mask;
                    // Ensure we use values >= min_tertiary_byte.
                    c += min_tertiary_byte - 1;
                    if (!ce.l1_ && !ce.l2_)
                        ce.l3_ = 0;
                    else
                        ce.l3_ = c << 8; // Shift into L3 lead byte.
                }
            } else {
                ce.l3_ &= disable_case_level_mask;
                if (ce.l2_)
                    ce.l3_ |= c << 14; // Shift into high 2 bits of L3.
                else if (ce.l3_)
                    ce.l3_ |= 3 << 14; // Shift into high 2 bits of L3.
            }

            return ce;
        }

        template<
            typename CEIter,
            typename CPIter,
            typename Sentinel,
            typename Container>
        auto
        s3(CEIter ces_first,
           CEIter ces_last,
           int ces_size,
           collation_strength strength,
           case_first case_1st,
           case_level case_lvl,
           l2_weight_order l2_order,
           CPIter cps_first,
           Sentinel cps_last,
           int cps_size,
           Container & bytes) -> detail::cp_iter_ret_t<void, CPIter>
        {
            level_sort_key_values_t l1;
            level_sort_key_values_t l2;
            level_sort_key_values_t l3;
            level_sort_key_values_t l4;
            // For when case level bumps L4.
            level_sort_key_values_t l4_overflow;

            auto const strength_for_copies =
                case_lvl == case_level::on
                    ? collation_strength(static_cast<int>(strength) + 1)
                    : strength;
            for (; ces_first != ces_last; ++ces_first) {
                auto ce = *ces_first;
                ce = detail::modify_for_case(ce, strength, case_1st, case_lvl);
                if (ce.l1_)
                    l1.push_back(ce.l1_);
                if (collation_strength::secondary <= strength_for_copies) {
                    if (ce.l2_)
                        l2.push_back(ce.l2_);
                    if (collation_strength::tertiary <= strength_for_copies) {
                        if (ce.l3_)
                            l3.push_back(ce.l3_);
                        if (collation_strength::quaternary <=
                            strength_for_copies) {
                            if (ce.l4_) {
                                l4.push_back(ce.l4_);
                                if (ces_first->l4_)
                                    l4_overflow.push_back(ces_first->l4_);
                            }
                        }
                    }
                }
            }

            if (l1.empty() && l2.empty() && l3.empty() && l4.empty() &&
                l4_overflow.empty()) {
                return;
            }

            if (!l2.empty()) {
                if (l2_order == l2_weight_order::backward)
                    std::reverse(l2.begin(), l2.end());
                auto packed_l2 = pack_words(l2);
                detail::rle(packed_l2,
                    min_secondary_byte,
                    common_secondary_byte,
                    max_secondary_byte);
                detail::pad_words(packed_l2);
                detail::level_bytes_to_values(packed_l2, l2);
                if (!l3.empty()) {
                    auto packed_l3 = detail::pack_words(l3);
                    detail::rle(packed_l3,
                        min_tertiary_byte,
                        common_tertiary_byte,
                        max_tertiary_byte);
                    detail::pad_words(packed_l3);
                    detail::level_bytes_to_values(packed_l3, l3);
                }
            }

            int const separators = static_cast<int>(strength_for_copies);

            level_sort_key_values_t nfd;
            if (collation_strength::quaternary < strength) {
                boost::text::normalize<nf::d>(
                    cps_first, cps_last, std::back_inserter(nfd));
            }

            int size = l1.size();
            if (collation_strength::primary < strength_for_copies) {
                size += l2.size();
                if (collation_strength::secondary < strength_for_copies) {
                    size += l3.size();
                    if (collation_strength::tertiary < strength_for_copies) {
                        size += l4.size();
                        if (!l4_overflow.empty()) {
                            ++size;
                            size += l4_overflow.size();
                        }
                        if (collation_strength::quaternary <
                            strength_for_copies)
                            size += nfd.size();
                    }
                }
            }
            size += separators;

            bytes.resize(bytes.size() + size);

            auto it = bytes.end() - size;
            it = std::copy(l1.begin(), l1.end(), it);
            if (collation_strength::primary < strength_for_copies) {
                *it++ = 0x0000;
                it = std::copy(l2.begin(), l2.end(), it);
                if (collation_strength::secondary < strength_for_copies) {
                    *it++ = 0x0000;
                    it = std::copy(l3.begin(), l3.end(), it);
                    if (collation_strength::tertiary < strength_for_copies) {
                        *it++ = 0x0000;
                        it = std::copy(l4.begin(), l4.end(), it);
                        if (!l4_overflow.empty()) {
                            *it++ = 0x0000;
                            it = std::copy(
                                l4_overflow.begin(), l4_overflow.end(), it);
                        }
                        if (collation_strength::quaternary <
                            strength_for_copies) {
                            *it++ = 0x0000;
                            it = std::copy(nfd.begin(), nfd.end(), it);
                        }
                    }
                }
            }
            BOOST_ASSERT(it == bytes.end());
        }

        template<typename CPIter, typename Sentinel>
        auto collation_sort_key(
            CPIter first,
            Sentinel last,
            collation_strength strength,
            case_first case_1st,
            case_level case_lvl,
            variable_weighting weighting,
            l2_weight_order l2_order,
            collation_table const & table)
            -> detail::cp_iter_ret_t<text_sort_key, CPIter>;

        template<typename Result, typename Iter>
        auto make_iterator(Result first, Iter it, null_sentinel s)
            -> decltype(Result(first.base(), it, s))
        {
            return Result(first.base(), it, s);
        }

        template<typename Result, typename Iter>
        auto make_iterator(Result first, Iter it, Result last)
            -> decltype(Result(first.base(), it, last.base()))
        {
            return Result(first.base(), it, last.base());
        }

        template<typename Iter>
        Iter make_iterator(Iter first, Iter it, Iter last)
        {
            return it;
        }

        template<typename Iter, typename Sentinel, typename LeadByteFunc>
        auto collate_impl(
            utf8_tag,
            Iter first1,
            Sentinel last1,
            utf8_tag,
            Iter first2,
            Sentinel last2,
            collation_strength strength,
            case_first case_1st,
            case_level case_lvl,
            variable_weighting weighting,
            l2_weight_order l2_order,
            collation_table const & table,
            collation_trie_t const & trie,
            collation_element const * ces_first,
            LeadByteFunc const & lead_byte);

        template<
            typename Tag1,
            typename Iter1,
            typename Sentinel1,
            typename Tag2,
            typename Iter2,
            typename Sentinel2,
            typename LeadByteFunc>
        auto collate_impl(
            Tag1,
            Iter1 first1,
            Sentinel1 last1,
            Tag2,
            Iter2 first2,
            Sentinel2 last2,
            collation_strength strength,
            case_first case_1st,
            case_level case_lvl,
            variable_weighting weighting,
            l2_weight_order l2_order,
            collation_table const & table,
            collation_trie_t const & trie,
            collation_element const * ces_first,
            LeadByteFunc const & lead_byte)
        {
            auto const lhs = boost::text::as_utf32(first1, last1);
            auto const rhs = boost::text::as_utf32(first2, last2);
            text_sort_key const lhs_sk = detail::collation_sort_key(
                lhs.begin(),
                lhs.end(),
                strength,
                case_1st,
                case_lvl,
                weighting,
                l2_order,
                table);
            text_sort_key const rhs_sk = detail::collation_sort_key(
                rhs.begin(),
                rhs.end(),
                strength,
                case_1st,
                case_lvl,
                weighting,
                l2_order,
                table);
            return boost::text::compare(lhs_sk, rhs_sk);
        }

        template<
            typename CPIter1,
            typename Sentinel1,
            typename CPIter2,
            typename Sentinel2>
        int collate(
            CPIter1 first1,
            Sentinel1 last1,
            CPIter2 first2,
            Sentinel2 last2,
            collation_strength strength,
            case_first case_1st,
            case_level case_lvl,
            variable_weighting weighting,
            l2_weight_order l2_order,
            collation_table const & table);

        template<format F>
        using collation_norm_buffer_for = std::conditional_t<
            F == format::utf8,
            container::small_vector<char, 1024>,
            container::small_vector<uint16_t, 512>>;
    }

}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

#ifdef BOOST_TEXT_DOXYGEN

    /** Returns a collation sort key for the code points in `[first, last)`,
        using the given collation table.  Any optional settings such as
        `case_1st` will be honored, so long as they do not conflict with the
        settings on the given table.

        Consider using one of the overloads that takes collation_flags
        instead.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        \pre `[first, last)` is normalized NFD or FCC. */
    template<typename CPIter, typename Sentinel>
    text_sort_key collation_sort_key(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_strength strength = collation_strength::tertiary,
        case_first case_1st = case_first::off,
        case_level case_lvl = case_level::off,
        variable_weighting weighting = variable_weighting::non_ignorable,
        l2_weight_order l2_order = l2_weight_order::forward);

    /** Returns a collation sort key for the code points in `[first, last)`,
        using the given collation table.  Any optional settings flags will be
        honored, so long as they do not conflict with the settings on the
        given table.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        \pre `[first, last)` is normalized NFD or FCC. */
    template<typename CPIter, typename Sentinel>
    text_sort_key collation_sort_key(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a collation sort key for the code points in `r`, using the
        given collation table.  Any optional settings flags will be honored,
        so long as they do not conflict with the settings on the given table.

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept.

        \pre r is normalized NFD or FCC. */
    template<typename CPRange>
    text_sort_key collation_sort_key(
        CPRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a collation sort key for the graphemes in `r`, using the given
        collation table.  Any optional settings flags will be honored, so long
        as they do not conflict with the settings on the given table.

        This function only participates in overload resolution if
        `GraphemeRange` models the GraphemeRange concept.

        \pre r is normalized NFD or FCC. */
    template<typename GraphemeRange>
    text_sort_key collation_sort_key(
        GraphemeRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Creates sort keys for `[first1, last1)` and `[first2, last2)`, then
        returns the result of calling compare() on the keys. Any optional
        settings such as `case_1st` will be honored, so long as they do not
        conflict with the settings on the given table.

        Consider using one of the overloads that takes collation_flags
        instead.

        This function only participates in overload resolution if `CPIter1`
        and `CPIter2` model the CPIter concept.

        \pre `[first1, last1)` is normalized NFD or FCC.
        \pre `[first2, last2)` is normalized NFD or FCC. */
    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    int collate(
        CPIter1 first1,
        Sentinel1 last1,
        CPIter2 first2,
        Sentinel2 last2,
        collation_table const & table,
        collation_strength strength = collation_strength::tertiary,
        case_first case_1st = case_first::off,
        case_level case_lvl = case_level::off,
        variable_weighting weighting = variable_weighting::non_ignorable,
        l2_weight_order l2_order = l2_weight_order::forward);

    /** Creates sort keys for `[first1, last1)` and `[first2, last2)`, then
        returns the result of calling compare() on the keys.  Any optional
        settings flags will be honored, so long as they do not conflict with
        the settings on the given table.

        This function only participates in overload resolution if `CPIter1`
        and `CPIter2` model the CPIter concept.

        \pre `[first1, last1)` is normalized NFD or FCC.
        \pre `[first2, last2)` is normalized NFD or FCC. */
    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    int collate(
        CPIter1 first1,
        Sentinel1 last1,
        CPIter2 first2,
        Sentinel2 last2,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Creates sort keys for `r1` and `r2`, then returns the result of
        calling compare() on the keys.  Any optional settings flags will be
        honored, so long as they do not conflict with the settings on the
        given table.

        This function only participates in overload resolution if `CPRange1`
        and `CPRange2` model the CPRange concept.

        \pre `r1` is normalized NFD or FCC.
        \pre `r2` is normalized NFD or FCC. */
    template<typename CPRange1, typename CPRange2>
    int collate(
        CPRange1 const & r1,
        CPRange2 const & r2,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Creates sort keys for `r1` and `r2`, then returns the result of
        calling compare() on the keys.  Any optional settings flags will be
        honored, so long as they do not conflict with the settings on the
        given table.

        This function only participates in overload resolution if
        `GraphemeRange1` and `GraphemeRange2` model the GraphemeRange concept.

        \pre `r1` is normalized NFD or FCC.
        \pre `r2` is normalized NFD or FCC. */
    template<typename GraphemeRange1, typename GraphemeRange2>
    int collate(
        GraphemeRange1 const & r1,
        GraphemeRange2 const & r2,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

#else
    
    template<typename CPIter, typename Sentinel>
    auto collation_sort_key(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_strength strength = collation_strength::tertiary,
        case_first case_1st = case_first::off,
        case_level case_lvl = case_level::off,
        variable_weighting weighting = variable_weighting::non_ignorable,
        l2_weight_order l2_order = l2_weight_order::forward)
        -> detail::cp_iter_ret_t<text_sort_key, CPIter>
    {
        return detail::collation_sort_key(
            first,
            last,
            strength,
            case_1st,
            case_lvl,
            weighting,
            l2_order,
            table);
    }

    template<typename CPIter, typename Sentinel>
    auto collation_sort_key(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_iter_ret_t<text_sort_key, CPIter>
    {
        return detail::collation_sort_key(
            first,
            last,
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags),
            table);
    }

    template<typename CPRange>
    auto collation_sort_key(
        CPRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<text_sort_key, CPRange>
    {
        return detail::collation_sort_key(
            std::begin(r),
            std::end(r),
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags),
            table);
    }

    template<typename GraphemeRange>
    auto collation_sort_key(
        GraphemeRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<text_sort_key, GraphemeRange>
    {
        return detail::collation_sort_key(
            std::begin(r).base(),
            std::end(r).base(),
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags),
            table);
    }

    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    auto collate(
        CPIter1 first1,
        Sentinel1 last1,
        CPIter2 first2,
        Sentinel2 last2,
        collation_table const & table,
        collation_strength strength = collation_strength::tertiary,
        case_first case_1st = case_first::off,
        case_level case_lvl = case_level::off,
        variable_weighting weighting = variable_weighting::non_ignorable,
        l2_weight_order l2_order = l2_weight_order::forward)
        -> detail::cp_iter_ret_t<int, CPIter1>
    {
        return detail::collate(
            first1,
            last1,
            first2,
            last2,
            strength,
            case_1st,
            case_lvl,
            weighting,
            l2_order,
            table);
    }

    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    auto collate(
        CPIter1 first1,
        Sentinel1 last1,
        CPIter2 first2,
        Sentinel2 last2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_iter_ret_t<int, CPIter1>
    {
        return detail::collate(
            first1,
            last1,
            first2,
            last2,
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags),
            table);
    }

    template<typename CPRange1, typename CPRange2>
    auto collate(
        CPRange1 const & r1,
        CPRange2 const & r2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<int, CPRange1>
    {
        return v1::collate(
            std::begin(r1),
            std::end(r1),
            std::begin(r2),
            std::end(r2),
            table,
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto collate(
        GraphemeRange1 const & r1,
        GraphemeRange2 const & r2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<int, GraphemeRange1>
    {
        return v1::collate(
            std::begin(r1).base(),
            std::end(r1).base(),
            std::begin(r2).base(),
            std::end(r2).base(),
            table,
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags));
    }

#endif

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    template<code_point_iter I, std::sentinel_for<I> S>
    text_sort_key collation_sort_key(
        I first,
        S last,
        collation_table const & table,
        collation_strength strength = collation_strength::tertiary,
        case_first case_1st = case_first::off,
        case_level case_lvl = case_level::off,
        variable_weighting weighting = variable_weighting::non_ignorable,
        l2_weight_order l2_order = l2_weight_order::forward)
    {
        return detail::collation_sort_key(
            first,
            last,
            strength,
            case_1st,
            case_lvl,
            weighting,
            l2_order,
            table);
    }

    template<code_point_iter I, std::sentinel_for<I> S>
    text_sort_key collation_sort_key(
        I first,
        S last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return detail::collation_sort_key(
            first,
            last,
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags),
            table);
    }

    template<code_point_range R>
    text_sort_key collation_sort_key(
        R && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return detail::collation_sort_key(
            std::begin(r),
            std::end(r),
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags),
            table);
    }

    template<grapheme_range R>
    text_sort_key collation_sort_key(
        R && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return detail::collation_sort_key(
            std::begin(r).base(),
            std::end(r).base(),
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags),
            table);
    }

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2>
    int collate(
        I1 first1,
        S1 last1,
        I2 first2,
        S2 last2,
        collation_table const & table,
        collation_strength strength = collation_strength::tertiary,
        case_first case_1st = case_first::off,
        case_level case_lvl = case_level::off,
        variable_weighting weighting = variable_weighting::non_ignorable,
        l2_weight_order l2_order = l2_weight_order::forward)
    {
        return detail::collate(
            first1,
            last1,
            first2,
            last2,
            strength,
            case_1st,
            case_lvl,
            weighting,
            l2_order,
            table);
    }

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2>
    int collate(
        I1 first1,
        S1 last1,
        I2 first2,
        S2 last2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return detail::collate(
            first1,
            last1,
            first2,
            last2,
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags),
            table);
    }

    template<code_point_range R1, code_point_range R2>
    int collate(
        R1 const & r1,
        R2 const & r2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return boost::text::collate(
            std::begin(r1),
            std::end(r1),
            std::begin(r2),
            std::end(r2),
            table,
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags));
    }

    template<grapheme_range R1, grapheme_range R2>
    int collate(
        R1 const & r1,
        R2 const & r2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return boost::text::collate(
            std::begin(r1).base(),
            std::end(r1).base(),
            std::begin(r2).base(),
            std::end(r2).base(),
            table,
            detail::to_strength(flags),
            detail::to_case_first(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags),
            detail::to_l2_order(flags));
    }

}}}

#endif

#include <boost/text/collation_table.hpp>

namespace boost { namespace text { namespace detail {

    template<typename CPIter, typename Sentinel, std::size_t N, std::size_t M>
    CPIter get_collation_elements(
        CPIter first,
        Sentinel last,
        collation_strength strength,
        case_first case_1st,
        case_level case_lvl,
        variable_weighting weighting,
        l2_weight_order l2_order,
        collation_table const & table,
        std::array<uint32_t, N> & buffer,
        typename std::array<uint32_t, N>::iterator & buf_it,
        int & cps,
        container::small_vector<collation_element, M> & ces)
    {
        auto it = first;
        {
            auto u = detail::unpack_iterator_and_sentinel(it, last);
            auto copy_result = detail::transcode_to_32<true>(
                u.tag_, u.f_, u.l_, buffer.end() - buf_it, buf_it);
            it = detail::make_iterator(first, copy_result.iter, last);
            buf_it = copy_result.out;
        }

        // The chunk we pass to S2 should end at the earliest contiguous
        // starter (ccc == 0) we find searching backward from the end.  This
        // is because 1) we don't want to cut off trailing combining
        // characters that may participate in longest-match determination in
        // S2.1, and 2) in S2.3 we need to know if earlier CPs are
        // variable-weighted or not.
        auto s2_it = buf_it;
        if (s2_it == buffer.end()) {
            while (s2_it != buffer.begin()) {
                if (detail::ccc(*--s2_it) == 0)
                    break;
            }
            // TODO: This will produce incorrect results if std::prev(s2_it)
            // points to a CP with variable-weighted CEs.
        }

        auto const end_of_raw_input = std::prev(it, s2_it - buf_it);
        auto const ces_size = ces.size();
        ces.resize(ces_size + M);
        auto ces_end = table.copy_collation_elements(
            buffer.begin(),
            s2_it,
            ces.begin() + ces_size,
            strength,
            case_1st,
            case_lvl,
            weighting);
        ces.resize(ces_end - ces.begin());
        buf_it = std::copy(s2_it, buf_it, buffer.begin());
        first = end_of_raw_input;

        return first;
    }

    template<typename CPIter, typename Sentinel>
    auto collation_sort_key(
        CPIter first,
        Sentinel last,
        collation_strength strength,
        case_first case_1st,
        case_level case_lvl,
        variable_weighting weighting,
        l2_weight_order l2_order,
        collation_table const & table)
        -> detail::cp_iter_ret_t<text_sort_key, CPIter>
    {
        auto const initial_first = first;

        if (table.l2_order())
            l2_order = *table.l2_order();
        if (table.weighting())
            weighting = *table.weighting();
        if (table.case_1st())
            case_1st = *table.case_1st();
        if (table.case_lvl())
            case_lvl = *table.case_lvl();

        std::array<uint32_t, 128> buffer;
        container::small_vector<collation_element, 128 * 10> ces;
        auto buf_it = buffer.begin();
        int cps = 0;
        while (first != last) {
            first = get_collation_elements(
                first,
                last,
                strength,
                case_1st,
                case_lvl,
                weighting,
                l2_order,
                table,
                buffer,
                buf_it,
                cps,
                ces);
        }

        std::vector<uint32_t> bytes;
        detail::s3(ces.begin(),
           ces.end(),
           ces.size(),
           strength,
           case_1st,
           case_lvl,
           l2_order,
           initial_first,
           last,
           cps,
           bytes);

        return text_sort_key(std::move(bytes));
    }

    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    int collate(
        CPIter1 first1,
        Sentinel1 last1,
        CPIter2 first2,
        Sentinel2 last2,
        collation_strength strength,
        case_first case_1st,
        case_level case_lvl,
        variable_weighting weighting,
        l2_weight_order l2_order,
        collation_table const & table)
    {
        if (table.l2_order())
            l2_order = *table.l2_order();
        if (table.weighting())
            weighting = *table.weighting();
        if (table.case_1st())
            case_1st = *table.case_1st();
        if (table.case_lvl())
            case_lvl = *table.case_lvl();

        auto lhs_u = detail::unpack_iterator_and_sentinel(first1, last1);
        auto rhs_u = detail::unpack_iterator_and_sentinel(first2, last2);
        return detail::collate_impl(
            lhs_u.tag_,
            lhs_u.f_,
            lhs_u.l_,
            rhs_u.tag_,
            rhs_u.f_,
            rhs_u.l_,
            strength,
            case_1st,
            case_lvl,
            weighting,
            l2_order,
            table,
            table.data_->trie_,
            table.collation_elements_begin(),
            [&table](detail::collation_element ce) {
                return detail::lead_byte(
                    ce,
                    table.data_->nonsimple_reorders_,
                    table.data_->simple_reorders_);
            });
    }

    template<typename Iter>
    struct next_primary_result
    {
        Iter it_;
        uint32_t cp_;
        unsigned char lead_primary_;
        uint32_t derived_primary_;
        trie_match_t coll_;
    };

    template<typename Iter, typename Sentinel, typename LeadByteFunc>
    auto collate_impl(
        utf8_tag,
        Iter first1,
        Sentinel last1,
        utf8_tag,
        Iter first2,
        Sentinel last2,
        collation_strength strength,
        case_first case_1st,
        case_level case_lvl,
        variable_weighting weighting,
        l2_weight_order l2_order,
        collation_table const & table,
        collation_trie_t const & trie,
        collation_element const * ces_first,
        LeadByteFunc const & lead_byte)
    {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
        std::cout << "\n\ncollate_impl():\n";
#endif

        auto lhs_it = first1;
        auto rhs_it = first2;

        if (l2_order == l2_weight_order::forward) {
            // This is std::ranges::mismatch(), but I can't use that yet.
            for (; lhs_it != last1 && rhs_it != last2;
                 ++lhs_it, ++rhs_it) {
                if (*lhs_it != *rhs_it) {
                    // Back up to the start of the current CP.
                    while (lhs_it != first1) {
                        --lhs_it;
                        --rhs_it;
                        if (!boost::text::continuation(*lhs_it))
                            break;
                    }
                    break;
                }
            }
            if (lhs_it == last1 && rhs_it == last2)
                return 0;
        }

        auto const lhs_identical_prefix = lhs_it;
        auto const rhs_identical_prefix = rhs_it;

        auto unshifted_primary_seq =
            [&](auto first, auto last, auto & primaries) {
                uint32_t retval = 0;
                for (auto it = first; it != last; ++it) {
                    if (it->l1_ &&
                        (weighting == variable_weighting::non_ignorable ||
                         strength == collation_strength::primary ||
                         !detail::variable(*it))) {
                        if (!retval)
                            retval = it->l1_;
                        primaries.push_back(it->l1_);
                    }
                }
                return retval;
            };

        auto unshifted_derived_primary = [&](uint32_t cp, auto & primaries) {
            collation_element ces[32];
            auto ces_end = detail::s2(
                &cp,
                &cp + 1,
                ces,
                trie,
                ces_first,
                lead_byte,
                collation_strength::primary,
                variable_weighting::non_ignorable,
                retain_case_bits_t::yes);
            return unshifted_primary_seq(ces, ces_end, primaries);
        };

        container::small_vector<uint32_t, 128> l_primaries;
        container::small_vector<uint32_t, 128> r_primaries;

        // Returns the CP that starts the primary-bearing sequence of CEs, and
        // the iterator just past the CP.
        auto next_primary = [&](Iter it, Sentinel last, auto & primaries) {
            using result_t = next_primary_result<Iter>;
            for (; it != last;) {
                unsigned char const c = *it;
                if (c < 0x80) {
                    int incr = 1;
#if BOOST_TEXT_USE_SIMD
                    if ((int)sizeof(__m128i) <= last - it) {
                        __m128i chunk = load_chars_for_sse(it);
                        int32_t const mask = _mm_movemask_epi8(chunk);
                        incr = mask == 0 ? 16 : trailing_zeros(mask);
                    }
#endif
                    for (auto const end = it + incr; it < end; ++it) {
                        uint32_t const cp = (unsigned char)*it;
                        trie_match_t coll =
                            trie.longest_subsequence((uint16_t)cp);
                        if (coll.match) {
                            auto lead_primary =
                                trie[coll]->lead_primary(weighting);
                            if (lead_primary)
                                return result_t{
                                    ++it, cp, lead_primary, 0, coll};
                        } else {
                            auto const p =
                                unshifted_derived_primary(cp, primaries);
                            if (p)
                                return result_t{++it, cp, 0, p, coll};
                        }
                    }
                } else {
                    auto next = it;
                    uint32_t cp = detail::advance(next, last);
                    trie_match_t coll = trie.longest_subsequence(cp);
                    it = next;
                    if (coll.match) {
                        auto lead_primary = trie[coll]->lead_primary(weighting);
                        if (lead_primary)
                            return result_t{it, cp, lead_primary, 0, coll};
                    } else {
                        auto const p = unshifted_derived_primary(cp, primaries);
                        if (p)
                            return result_t{it, cp, 0, p, coll};
                    }
                }
            }
            return result_t{it, 0, 0, 0, trie_match_t{}};
        };

        auto back_up_before_nonstarters = [&](Iter first,
                                              Iter & it,
                                              uint32_t & cp,
                                              unsigned char & lead_primary,
                                              auto & primaries,
                                              auto prev_primaries_size) {
            bool retval = false;
            auto prev = detail::decrement(first, it);
            while (it != first && table.nonstarter(cp)) {
                auto it2 = prev;
                auto prev2 = detail::decrement(first, prev);
                auto temp = prev2;
                auto cp2 = detail::advance(temp, it);

                // Check that moving backward one CP still includes the
                // current CP cp.
                uint32_t cps[] = {cp2, cp};
                auto const cus = boost::text::as_utf16(cps);
                auto coll = trie.longest_subsequence(cps, cps + 2);
                if (coll.size != std::distance(cus.begin(), cus.end()))
                    break;

                it = it2;
                prev = prev2;
                cp = cp2;

                lead_primary = 0;
                primaries.resize(prev_primaries_size);

                retval = true;
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                std::cout << "    backing up one CP.\n";
#endif
            }
            return retval;
        };

        container::small_vector<uint32_t, 64> cps;
        container::small_vector<Iter, 64> cp_end_iters;

        auto get_primary =
            [&](Iter & it,
                Sentinel last,
                uint32_t cp,
                trie_match_t coll,
                bool backed_up,
                auto & primaries) {
                if (backed_up)
                    coll = trie.longest_subsequence(cp);
                if (coll.match) {
                    // S2.1
                    if (!coll.leaf) {
                        // Find the longest match, one CP at a time.
                        auto next = it;
                        cps.clear();
                        cp_end_iters.clear();
                        cps.push_back(cp);
                        cp_end_iters.push_back(next);
                        auto last_match = coll;
                        while (next != last) {
                            auto temp = next;
                            cp = detail::advance(temp, last);
                            auto extended_coll =
                                trie.extend_subsequence(coll, cp);
                            if (extended_coll == coll)
                                break;
                            next = temp;
                            coll = extended_coll;
                            if (coll.match)
                                last_match = coll;
                            cps.push_back(cp);
                            cp_end_iters.push_back(next);
                        }

                        auto const last_match_cps_size = cps.size();

                        // S2.1.1
                        while (next != last) {
                            cp = detail::advance(next, last);
                            if (detail::ccc(cp) == 0)
                                break;
                            cps.push_back(cp);
                            cp_end_iters.push_back(next);
                        }

#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                        auto const & collation_value = *trie[coll];
                        std::cout << "coll.match, coll_value="
                                  << collation_value.first() << " "
                                  << collation_value.last()
                                  << " getting CEs for: ";
                        for (auto cp : cps) {
                            std::cout << "0x" << std::hex << cp << " ";
                        }
                        std::cout << std::dec << "\n";
#endif

                        // S2.1.2
                        auto cps_it = cps.begin() + last_match_cps_size;
                        coll =
                            detail::s2_1_2(cps_it, cps.end(), coll, trie, true);
                        it = cp_end_iters[cps_it - cps.begin() - 1];
                    }

                    auto const & collation_value = *trie[coll];
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                    std::cout << "final coll.match coll.size=" << coll.size
                              << " coll.value=" << collation_value.first()
                              << " " << collation_value.last() << "\n";
#endif

                    return unshifted_primary_seq(
                        collation_value.begin(ces_first),
                        collation_value.end(ces_first),
                        primaries);
                } else {
                    return unshifted_derived_primary(cp, primaries);
                }
            };

        // Look for a non-ignorable primary, or the end of each sequence.

        while (lhs_it != last1 || rhs_it != last2) {
            auto prev_l_primaries_size = l_primaries.size();
            auto prev_r_primaries_size = r_primaries.size();
            auto l_prim = next_primary(lhs_it, last1, l_primaries);
            auto r_prim = next_primary(rhs_it, last2, r_primaries);

#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
            std::cout << "l_prim.cp_=" << std::hex << "0x" << l_prim.cp_
                      << std::dec << "\n";
            std::cout << "r_prim.cp_=" << std::hex << "0x" << r_prim.cp_
                      << std::dec << "\n";
#endif

            bool l_backed_up = false;
            if (table.nonstarter(l_prim.cp_)) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                std::cout << "backing up the left.\n";
#endif
                l_backed_up = back_up_before_nonstarters(
                    first1,
                    l_prim.it_,
                    l_prim.cp_,
                    l_prim.lead_primary_,
                    l_primaries,
                    prev_l_primaries_size);
            }
            bool r_backed_up = false;
            if (table.nonstarter(r_prim.cp_)) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                std::cout << "backing up the right.\n";
#endif
                r_backed_up = back_up_before_nonstarters(
                    first2,
                    r_prim.it_,
                    r_prim.cp_,
                    r_prim.lead_primary_,
                    r_primaries,
                    prev_r_primaries_size);
            }

            if ((l_prim.coll_.leaf || lhs_it == last1) &&
                (r_prim.coll_.leaf || rhs_it == last2) &&
                l_primaries.empty() && r_primaries.empty()) {
                uint32_t l_primary = l_prim.derived_primary_
                                         ? l_prim.derived_primary_ >> 24
                                         : l_prim.lead_primary_;
                uint32_t r_primary = r_prim.derived_primary_
                                         ? r_prim.derived_primary_ >> 24
                                         : r_prim.lead_primary_;
                if (l_primary && r_primary) {
                    if (l_primary < r_primary) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                        std::cout << "early return -1 (lead byte)\n";
                        std::cout << "left: "
                                  << "0x" << std::hex << l_primary << std::dec
                                  << " right: "
                                  << "0x" << std::hex << r_primary << std::dec
                                  << "\n";
#endif
                        return -1;
                    }
                    if (r_primary < l_primary) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                        std::cout << "early return 1 (lead byte)\n";
                        std::cout << "left: "
                                  << "0x" << std::hex << l_primary << std::dec
                                  << " right: "
                                  << "0x" << std::hex << r_primary << std::dec
                                  << "\n";
#endif
                        return 1;
                    }
                }
            }

#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
            {
                std::cout << "before getting primaries:\n";

                std::cout << " left: cps: " << std::hex;
                auto const l_r = boost::text::as_utf32(l_prim.it_, last1);
                for (auto cp : l_r) {
                    std::cout << "0x" << cp << " ";
                }
                std::cout << "\n";

                std::cout << "right: cps: " << std::hex;
                auto const r_r = boost::text::as_utf32(r_prim.it_, last2);
                for (auto cp : r_r) {
                    std::cout << "0x" << cp << " ";
                }
                std::cout << std::dec << "\n";
            }
#endif

            uint32_t l_primary = l_prim.derived_primary_;
            if (!l_primary && lhs_it != last1) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                std::cout << "left get_primary()\n";
#endif
                get_primary(
                    l_prim.it_,
                    last1,
                    l_prim.cp_,
                    l_prim.coll_,
                    l_backed_up,
                    l_primaries);
            }
            uint32_t r_primary = r_prim.derived_primary_;
            if (!r_primary && rhs_it != last2) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                std::cout << "right get_primary()\n";
#endif
                get_primary(
                    r_prim.it_,
                    last2,
                    r_prim.cp_,
                    r_prim.coll_,
                    r_backed_up,
                    r_primaries);
            }

#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
            {
                std::cout << "after getting primaries:\n";

                std::cout << " left: cps: " << std::hex;
                auto const l_r = boost::text::as_utf32(l_prim.it_, last1);
                for (auto cp : l_r) {
                    std::cout << "0x" << cp << " ";
                }
                std::cout << "\n";

                std::cout << "right: cps: " << std::hex;
                auto const r_r = boost::text::as_utf32(r_prim.it_, last2);
                for (auto cp : r_r) {
                    std::cout << "0x" << cp << " ";
                }
                std::cout << std::dec << "\n";
            }
#endif

            auto const mismatches = algorithm::mismatch(
                l_primaries.begin(),
                l_primaries.end(),
                r_primaries.begin(),
                r_primaries.end());

            auto const l_at_end = mismatches.first == l_primaries.end();
            auto const r_at_end = mismatches.second == r_primaries.end();
            if (!l_at_end && !r_at_end) {
                if (*mismatches.first < *mismatches.second) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                    std::cout << "early return -1\n";
                    std::cout << " left: " << std::hex;
                    for (auto p : l_primaries) {
                        std::cout << "0x" << p << " ";
                    }
                    std::cout << "\n";
                    std::cout << "right: " << std::hex;
                    for (auto p : r_primaries) {
                        std::cout << "0x" << p << " ";
                    }
                    std::cout << std::dec << "\n";
#endif
                    return -1;
                } else {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                    std::cout << "early return 1\n";
                    std::cout << " left: " << std::hex;
                    for (auto p : l_primaries) {
                        std::cout << "0x" << p << " ";
                    }
                    std::cout << "\n";
                    std::cout << "right: " << std::hex;
                    for (auto p : r_primaries) {
                        std::cout << "0x" << p << " ";
                    }
                    std::cout << std::dec << "\n";
#endif
                    return 1;
                }
            } else if (l_at_end && !r_at_end && lhs_it == last1) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                std::cout << "early return -1 (right at end)\n";
                std::cout << " left: " << std::hex;
                for (auto p : l_primaries) {
                    std::cout << "0x" << p << " ";
                }
                std::cout << "\n";
                std::cout << "right: " << std::hex;
                for (auto p : r_primaries) {
                    std::cout << "0x" << p << " ";
                }
                std::cout << std::dec << "\n";
#endif
                return -1;
            } else if (!l_at_end && r_at_end && rhs_it == last2) {
#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
                std::cout << "early return 1 (left at end)\n";
                std::cout << " left: " << std::hex;
                for (auto p : l_primaries) {
                    std::cout << "0x" << p << " ";
                }
                std::cout << "\n";
                std::cout << "right: " << std::hex;
                for (auto p : r_primaries) {
                    std::cout << "0x" << p << " ";
                }
                std::cout << std::dec << "\n";
#endif
                return 1;
            }

            l_primaries.erase(l_primaries.begin(), mismatches.first);
            r_primaries.erase(r_primaries.begin(), mismatches.second);

            lhs_it = l_prim.it_;
            rhs_it = r_prim.it_;

            BOOST_ASSERT(boost::text::starts_encoded(lhs_it, last1));
            BOOST_ASSERT(boost::text::starts_encoded(rhs_it, last2));

#if BOOST_TEXT_INSTRUMENT_COLLATE_IMPL
            std::cout << "**************** at end of loop:\n";

            std::cout << " left: cps: " << std::hex;
            auto const l_r = boost::text::as_utf32(lhs_it, last1);
            for (auto cp : l_r) {
                std::cout << "0x" << cp << " ";
            }
            std::cout << "\n       prims: ";
            for (auto p : l_primaries) {
                std::cout << "0x" << p << " ";
            }
            std::cout << "\n";

            std::cout << "right: cps: " << std::hex;
            auto const r_r = boost::text::as_utf32(rhs_it, last2);
            for (auto cp : r_r) {
                std::cout << "0x" << cp << " ";
            }
            std::cout << "\n       prims: ";
            for (auto p : r_primaries) {
                std::cout << "0x" << p << " ";
            }
            std::cout << std::dec << "\n";
#endif
        }

        if (strength == collation_strength::primary)
            return 0;

        auto const lhs =
            boost::text::as_utf32(lhs_identical_prefix, last1);
        auto const rhs =
            boost::text::as_utf32(rhs_identical_prefix, last2);
        text_sort_key const lhs_sk = detail::collation_sort_key(
            lhs.begin(),
            lhs.end(),
            strength,
            case_1st,
            case_lvl,
            weighting,
            l2_order,
            table);
        text_sort_key const rhs_sk = detail::collation_sort_key(
            rhs.begin(),
            rhs.end(),
            strength,
            case_1st,
            case_lvl,
            weighting,
            l2_order,
            table);

        return boost::text::compare(lhs_sk, rhs_sk);
    }

}}}

#ifndef BOOST_TEXT_DOXYGEN

namespace std {
    template<>
    struct hash<boost::text::text_sort_key>
    {
        using argument_type = boost::text::text_sort_key;
        using result_type = std::size_t;
        result_type operator()(argument_type const & key) const noexcept
        {
            return std::accumulate(
                key.begin(),
                key.end(),
                result_type(key.size()),
                boost::text::detail::hash_combine_);
        }
    };
}

#endif

#endif
