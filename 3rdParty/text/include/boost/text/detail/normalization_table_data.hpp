// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_NORMALIZATION_TABLE_DATA_HPP
#define BOOST_TEXT_DETAIL_NORMALIZATION_TABLE_DATA_HPP

#include <boost/text/detail/normalization_trie.hpp>


namespace boost { namespace text { namespace detail {

    struct normalization_table_data
    {
        normalization_table_data(
            int32_t const * indices,
            normalization_trie const & trie_,
            uint16_t const * extra_data_,
            uint8_t const * small_fcd_) :
            min_decomp_no_cp((uint16_t)(indices[min_decomp_no_cp_index])),
            min_comp_no_maybe_cp(
                (uint16_t)(indices[min_comp_no_maybe_cp_index])),
            min_yes_no((uint16_t)(indices[min_yes_no_index])),
            min_yes_no_mappings_only(
                (uint16_t)(indices[min_yes_no_mappings_only_index])),
            min_no_no((uint16_t)(indices[min_no_no_index])),
            min_no_no_compboundary_before(
                (uint16_t)(indices[min_no_no_comp_boundary_before_index])),
            min_no_no_comp_no_maybe_cc(
                (uint16_t)(indices[min_no_no_comp_no_maybe_cc_index])),
            min_no_no_empty((uint16_t)(indices[min_no_no_empty_index])),
            limit_no_no((uint16_t)(indices[limit_no_no_index])),
            min_maybe_yes((uint16_t)(indices[min_maybe_yes_index])),
            center_no_no_delta((min_maybe_yes >> delta_shift) - max_delta - 1),
            trie(trie_),
            maybe_yes_compositions(extra_data_),
            extra_data(
                maybe_yes_compositions +
                ((min_normal_maybe_yes - min_maybe_yes) >> offset_shift)),
            small_fcd(small_fcd_)
        {
            BOOST_ASSERT((min_maybe_yes & 7) == 0);
        }

        uint16_t get_norm16(int32_t c) const noexcept
        {
            return ((c & 0xfffffc00) == 0xd800) ? inert : trie.fast_get(c);
        }

        bool algorithmic_no_no(uint16_t norm16) const noexcept
        {
            return limit_no_no <= norm16 && norm16 < min_maybe_yes;
        }
        bool decomp_yes(uint16_t norm16) const noexcept
        {
            return norm16 < min_yes_no || min_maybe_yes <= norm16;
        }

        bool maybe(uint16_t norm16) const noexcept
        {
            return min_maybe_yes <= norm16 && norm16 <= jamo_vt;
        }
        bool maybe_or_non_zero_cc(uint16_t norm16) const noexcept
        {
            return min_maybe_yes <= norm16;
        }
        bool is_inert(uint16_t norm16) const noexcept
        {
            return norm16 == inert;
        }
        bool get_jamo_vt(uint16_t norm16) const noexcept
        {
            return norm16 == jamo_vt;
        }

        bool hangul_lv(uint16_t norm16) const noexcept
        {
            return norm16 == min_yes_no;
        }
        bool hangul_lvt(uint16_t norm16) const noexcept
        {
            return norm16 ==
                   (min_yes_no_mappings_only | has_comp_boundary_after);
        }

        bool comp_yes_and_zero_cc(uint16_t norm16) const noexcept
        {
            return norm16 < min_no_no;
        }
        bool most_decomp_yes_and_zero_cc(uint16_t norm16) const noexcept
        {
            return norm16 < min_yes_no || norm16 == min_normal_maybe_yes ||
                   norm16 == jamo_vt;
        }
        bool decomp_no_algorithmic(uint16_t norm16) const noexcept
        {
            return limit_no_no <= norm16;
        }

        int32_t map_algorithmic(int32_t c, uint16_t norm16) const noexcept
        {
            return c + (norm16 >> delta_shift) - center_no_no_delta;
        }
        int32_t get_algorithmic_delta(uint16_t norm16) const noexcept
        {
            return (norm16 >> delta_shift) - center_no_no_delta;
        }

        uint16_t const * get_mapping(uint16_t norm16) const noexcept
        {
            return extra_data + (norm16 >> offset_shift);
        }
        uint16_t const *
        get_compositions_list_for_decomp_yes(uint16_t norm16) const noexcept
        {
            if (norm16 < jamo_l || min_normal_maybe_yes <= norm16) {
                return nullptr;
            } else if (norm16 < min_maybe_yes) {
                return get_mapping(norm16);
            } else {
                return maybe_yes_compositions + norm16 - min_maybe_yes;
            }
        }
        uint16_t const *
        get_compositions_list_for_composite(uint16_t norm16) const noexcept
        {
            uint16_t const * list = get_mapping(norm16);
            return list + 1 + (*list & mapping_length_mask);
        }
        uint16_t const *
        get_compositions_list_for_maybe(uint16_t norm16) const noexcept
        {
            return maybe_yes_compositions +
                   ((norm16 - min_maybe_yes) >> offset_shift);
        }

        bool comp_boundary_before(int32_t c, uint16_t norm16) const noexcept
        {
            return c < min_comp_no_maybe_cp ||
                   norm16_comp_boundary_before(norm16);
        }
        bool norm16_comp_boundary_before(uint16_t norm16) const noexcept
        {
            return norm16 < min_no_no_comp_no_maybe_cc ||
                   algorithmic_no_no(norm16);
        }
        template<typename Iter, typename Sentinel>
        bool
        comp_boundary_before_utf16(Iter first, Sentinel last) const noexcept
        {
            if (first == last || *first < min_comp_no_maybe_cp)
                return true;
            int32_t c = 0;
            uint16_t norm16 = trie.fast_u16_next(first, last, c);
            return norm16_comp_boundary_before(norm16);
        }
        template<typename CharIter, typename Sentinel>
        bool
        comp_boundary_before_utf8(CharIter first, Sentinel last) const noexcept
        {
            if (first == last)
                return true;
            uint16_t norm16 = trie.fast_u8_next(first, last);
            return norm16_comp_boundary_before(norm16);
        }
        template<typename Iter, typename Sentinel>
        bool comp_boundary_after_utf16(
            Iter first, Sentinel last, bool only_contiguous) const noexcept
        {
            if (first == last)
                return true;
            int32_t c = 0;
            uint16_t norm16 = trie.fast_u16_prev(first, last, c);
            return norm16_comp_boundary_after(norm16, only_contiguous);
        }
        template<typename CharIter>
        bool comp_boundary_after_utf8(
            CharIter first, CharIter last, bool only_contiguous) const noexcept
        {
            if (first == last)
                return true;
            uint16_t norm16 = trie.fast_u8_prev(first, last);
            return norm16_comp_boundary_after(norm16, only_contiguous);
        }
        bool norm16_comp_boundary_after(
            uint16_t norm16, bool only_contiguous) const noexcept
        {
            return (norm16 & has_comp_boundary_after) != 0 &&
                   (!only_contiguous ||
                    trail_cc01_for_comp_boundary_after(norm16));
        }
        bool trail_cc01_for_comp_boundary_after(uint16_t norm16) const noexcept
        {
            return is_inert(norm16) ||
                   (decomp_no_algorithmic(norm16)
                        ? (norm16 & delta_tccc_mask) <= delta_tccc_1
                        : *get_mapping(norm16) <= 0x1ff);
        }

        uint8_t get_cc_from_no_no(uint16_t norm16) const noexcept
        {
            uint16_t const * mapping = get_mapping(norm16);
            if (*mapping & mapping_has_ccc_lccc_word)
                return *--mapping;
            return 0;
        }
        uint8_t
        get_trail_cc_from_comp_yes_and_zero_cc(uint16_t norm16) const noexcept
        {
            if (norm16 <= min_yes_no)
                return 0;
            return *get_mapping(norm16) >> 8;
        }
        template<typename Iter>
        uint8_t
        get_previous_trail_cc_utf16(Iter first, Iter last) const noexcept
        {
            if (first == last)
                return 0;
            int32_t cp = *--last;
            if (boost::text::low_surrogate(cp)) {
                uint16_t cu = 0;
                if (first != last &&
                    boost::text::high_surrogate(cu = *std::prev(last))) {
                    --last;
                    cp = detail::surrogates_to_cp(cu, cp);
                }
            }
            return get_fcd_16(cp);
        }
        template<typename CharIter>
        uint8_t
        get_previous_trail_cc_utf8(CharIter first, CharIter last) const noexcept
        {
            if (first == last)
                return 0;
            int32_t i = (int32_t)(last - first);
            int32_t c = (uint8_t)first[--i];
            if (0x80 <= c)
                c = normalization_trie::utf8_prev_cp(first, 0, &i, c);
            return get_fcd_16(c);
        }

        uint16_t const * get_compositions_list(uint16_t norm16) const noexcept
        {
            return decomp_yes(norm16)
                       ? get_compositions_list_for_decomp_yes(norm16)
                       : get_compositions_list_for_composite(norm16);
        }

        uint16_t get_fcd_16(int32_t c) const noexcept
        {
            if (c < min_decomp_no_cp) {
                return 0;
            } else if (c <= 0xffff) {
                if (!single_lead_might_have_non_zero_fcd_16(c))
                    return 0;
            }
            return get_fcd_16_from_norm_data(c);
        }
        uint16_t get_fcd_16_from_norm_data(int32_t c) const noexcept
        {
            uint16_t norm16 = get_norm16(c);
            if (limit_no_no <= norm16) {
                if (min_normal_maybe_yes <= norm16) {
                    norm16 = get_cc_from_normal_yes_or_maybe(norm16);
                    return norm16 | (norm16 << 8);
                } else if (min_maybe_yes <= norm16) {
                    return 0;
                } else {
                    uint16_t delta_trail_cc = norm16 & delta_tccc_mask;
                    if (delta_trail_cc <= delta_tccc_1)
                        return delta_trail_cc >> offset_shift;
                    c = map_algorithmic(c, norm16);
                    norm16 = trie.fast_get(c);
                }
            }
            if (norm16 <= min_yes_no || hangul_lvt(norm16))
                return 0;
            uint16_t const * mapping = get_mapping(norm16);
            uint16_t first_unit = *mapping;
            norm16 = first_unit >> 8;
            if (first_unit & mapping_has_ccc_lccc_word)
                norm16 |= *(mapping - 1) & 0xff00;
            return norm16;
        }
        bool single_lead_might_have_non_zero_fcd_16(int32_t lead) const noexcept
        {
            uint8_t bits = small_fcd[lead >> 8];
            if (bits == 0)
                return false;
            return (bool)((bits >> ((lead >> 5) & 7)) & 1);
        }

        uint8_t get_cc(uint16_t norm16) const noexcept
        {
            if (min_normal_maybe_yes <= norm16)
                return get_cc_from_normal_yes_or_maybe(norm16);
            if (norm16 < min_no_no || limit_no_no <= norm16)
                return 0;
            return get_cc_from_no_no(norm16);
        }
        uint8_t get_cc_from_normal_yes_or_maybe(uint16_t norm16) const noexcept
        {
            return norm16 >> offset_shift;
        }
        uint8_t get_cc_from_yes_or_maybe(uint16_t norm16) const noexcept
        {
            return min_normal_maybe_yes <= norm16
                       ? get_cc_from_normal_yes_or_maybe(norm16)
                       : 0;
        }
        uint8_t get_cc_from_yes_or_maybe_cp(int32_t c) const noexcept
        {
            if (c < min_comp_no_maybe_cp)
                return 0;
            return get_cc_from_yes_or_maybe(get_norm16(c));
        }

        uint16_t min_decomp_no_cp;
        uint16_t min_comp_no_maybe_cp;
        uint16_t min_yes_no;
        uint16_t min_yes_no_mappings_only;
        uint16_t min_no_no;
        uint16_t min_no_no_compboundary_before;
        uint16_t min_no_no_comp_no_maybe_cc;
        uint16_t min_no_no_empty;
        uint16_t limit_no_no;
        uint16_t min_maybe_yes;
        uint16_t center_no_no_delta;

        normalization_trie trie;
        uint16_t const * maybe_yes_compositions;
        uint16_t const * extra_data;
        uint8_t const * small_fcd;

        enum {
            min_yes_yes_with_cc = 0xfe02,
            jamo_vt = 0xfe00,
            min_normal_maybe_yes = 0xfc00,
            jamo_l = 2,
            inert = 1,
            has_comp_boundary_after = 1,
            offset_shift = 1,
            delta_tccc_0 = 0,
            delta_tccc_1 = 2,
            delta_tccc_gt_1 = 4,
            delta_tccc_mask = 6,
            delta_shift = 3,
            max_delta = 0x40,

            mapping_has_ccc_lccc_word = 0x80,
            mapping_length_mask = 0x1f,

            comp_1_last_tuple = 0x8000,
            comp_1_triple = 1,
            comp_1_trail_limit = 0x3400,
            comp_1_trail_mask = 0x7ffe,
            comp_1_trail_shift = 9,
            comp_2_trail_shift = 6,
            comp_2_trail_mask = 0xffc0
        };

        enum {
            min_decomp_no_cp_index = 8,
            min_comp_no_maybe_cp_index,
            min_yes_no_index,
            min_no_no_index,
            limit_no_no_index,
            min_maybe_yes_index,
            min_yes_no_mappings_only_index,
            min_no_no_comp_boundary_before_index,
            min_no_no_comp_no_maybe_cc_index,
            min_no_no_empty_index,
            min_lccc_cp_index,
            reserved19_index,

            total_indices
        };
    };

}}}

#endif
