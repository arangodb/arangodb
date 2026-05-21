// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_NORMALIZATION_TRIE_HPP
#define BOOST_TEXT_DETAIL_NORMALIZATION_TRIE_HPP

#include <boost/assert.hpp>

#include <cstring>
#include <memory>


namespace boost { namespace text { namespace detail {

    namespace {
        constexpr char u8_lead3_t1_bits[] =
            "\x20\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x10\x30\x30";
        constexpr char u8_lead4_t1_bits[] =
            "\x00\x00\x00\x00\x00\x00\x00\x00\x1E\x0F\x0F\x0F\x00\x00\x00\x00";
    }

    // This should be kept layout-compatible with ICU's UCPTrie, to make
    // integrating their generated table data smoother.
    struct normalization_trie
    {
        uint16_t fast_get(int32_t c) const noexcept
        {
            return data[cp_index_(c)];
        }
        uint16_t fast_bmp_get(int32_t cp) const noexcept
        {
            return data[fast_index_(cp)];
        }
        uint16_t fast_supp_get(int32_t cp) const noexcept
        {
            return data[small_index_(cp)];
        }

        template<typename CharIter>
        auto fast_u8_prev(CharIter first, CharIter & last) const noexcept
        {
            int32_t index = (uint8_t) * --last;
            if (0x80 <= index) {
                index = u8_prev_index_(index, first, last);
                last -= index & 7;
                index >>= 3;
            }
            return data[index];
        }
        template<typename Iter>
        auto fast_u16_prev(
            Iter first, Iter & last, int32_t & cp) const noexcept
        {
            cp = *--last;
            int32_t index;
            if (!boost::text::surrogate(cp)) {
                index = fast_index_(cp);
            } else {
                uint16_t c2;
                if (boost::text::low_surrogate(cp) && last != first &&
                    boost::text::high_surrogate(c2 = *std::prev(last))) {
                    --last;
                    cp = detail::surrogates_to_cp(c2, cp);
                    index = small_index_(cp);
                } else {
                    index = data_size - error_value_neg_data_offset;
                }
            }
            return data[index];
        }

        template<typename CharIter, typename Sentinel>
        auto fast_u8_next(
            CharIter & first, Sentinel last) const noexcept
        {
            int32_t lead = (uint8_t)*first++;
            if (0x80 <= lead) {
                uint8_t t1, t2, t3;
                if (first != last &&
                    (lead >= 0xe0
                         ? lead < 0xf0
                               ? u8_lead3_t1_bits[lead &= 0xf] &
                                         (1 << ((t1 = *first) >> 5)) &&
                                     ++first != last &&
                                     (t2 = *first - 0x80) <= 0x3f &&
                                     (lead = ((int32_t)index
                                                  [(lead << 6) + (t1 & 0x3f)]) +
                                             t2,
                                      1)
                               : (lead -= 0xf0) <= 4 &&
                                     u8_lead4_t1_bits[(t1 = *first) >> 4] &
                                         (1 << lead) &&
                                     (lead = (lead << 6) | (t1 & 0x3f),
                                      ++first != last) &&
                                     (t2 = *first - 0x80) <= 0x3f &&
                                     ++first != last &&
                                     (t3 = *first - 0x80) <= 0x3f &&
                                     (lead =
                                          lead >= shifted_12_high_start
                                              ? data_size -
                                                    high_value_neg_data_offset
                                              : small_u8_index_(lead, t2, t3),
                                      1)
                         : lead >= 0xc2 && (t1 = *first - 0x80) <= 0x3f &&
                               (lead = (int32_t)index[lead & 0x1f] + t1, 1))) {
                    ++first;
                } else {
                    lead = data_size - error_value_neg_data_offset;
                }
            }
            return data[lead];
        }
        template<typename Iter, typename Sentinel>
        auto
        fast_u16_next(Iter & first, Sentinel last, int32_t & cp) const noexcept
        {
            cp = *first++;
            int32_t index;
            if (!boost::text::surrogate(cp)) {
                index = fast_index_(cp);
            } else {
                uint16_t c2;
                if (boost::text::high_surrogate(cp) && first != last &&
                    boost::text::low_surrogate(c2 = *first)) {
                    ++first;
                    cp = detail::surrogates_to_cp(cp, c2);
                    index = small_index_(cp);
                } else {
                    index = data_size - error_value_neg_data_offset;
                }
            }
            return data[index];
        }

        int32_t cp_index_(int32_t cp) const noexcept
        {
            return cp <= 0xffff ? fast_index_(cp)
                                : cp <= 0x10ffff
                                      ? small_index_(cp)
                                      : data_size - error_value_neg_data_offset;
        }

        int32_t fast_index_(int32_t cp) const noexcept
        {
            return (int32_t)index[cp >> fast_shift] + (cp & fast_data_mask);
        }

        int32_t small_index_(int32_t cp) const noexcept
        {
            return high_start <= cp ? data_size - high_value_neg_data_offset
                                    : small_index_impl_(cp);
        }

        int32_t small_index_impl_(int32_t cp) const noexcept
        {
            int32_t i1 = cp >> shift_1;
            BOOST_ASSERT(0xffff < cp && cp < high_start);
            i1 += bmp_index_length - omitted_bmp_index_1_length;
            int32_t i3_block =
                index[(int32_t)index[i1] + ((cp >> shift_2) & index_2_mask)];
            int32_t i3 = (cp >> shift_3) & index_3_mask;
            int32_t data_block;
            if ((i3_block & 0x8000) == 0) {
                data_block = index[i3_block + i3];
            } else {
                i3_block = (i3_block & 0x7fff) + (i3 & ~7) + (i3 >> 3);
                i3 &= 7;
                data_block =
                    ((int32_t)index[i3_block++] << (2 + (2 * i3))) & 0x30000;
                data_block |= index[i3_block + i3];
            }
            return data_block + (cp & small_data_mask);
        }

        int32_t
        small_u8_index_(int32_t b1, uint8_t b2, uint8_t b3) const noexcept
        {
            int32_t const cp = (b1 << 12) | (b2 << 6) | b3;
            if (high_start <= cp)
                return data_size - high_value_neg_data_offset;
            return small_index_impl_(cp);
        }

        template<typename CharIter>
        int32_t
        u8_prev_index_(int32_t cp, CharIter first, CharIter last) const noexcept
        {
            int32_t i, length;
            if ((last - first) <= 7) {
                i = length = int32_t(last - first);
            } else {
                i = length = 7;
                first = last - 7;
            }
            cp = utf8_prev_cp(first, 0, &i, cp);
            i = length - i;
            int32_t idx = cp_index_(cp);
            return (idx << 3) | i;
        }

        template<typename CharIter>
        static int32_t utf8_prev_cp(
            CharIter it, int32_t start, int32_t * pi, int32_t c) noexcept
        {
            int32_t i = *pi;
            if (boost::text::continuation(c) && i > start) {
                uint8_t b1 = it[--i];
                if (boost::text::lead_code_unit(b1)) {
                    if (b1 < 0xe0) {
                        *pi = i;
                        return ((b1 - 0xc0) << 6) | (c & 0x3f);
                    } else if (
                        b1 < 0xf0 ? valid_lead3_and_t1(b1, c)
                                  : valid_lead4_and_t1(b1, c)) {
                        *pi = i;
                        return -1;
                    }
                } else if (boost::text::continuation(b1) && i > start) {
                    c &= 0x3f;
                    uint8_t b2 = it[--i];
                    if (0xe0 <= b2 && b2 <= 0xf4) {
                        if (b2 < 0xf0) {
                            b2 &= 0xf;
                            if (valid_lead3_and_t1(b2, b1)) {
                                *pi = i;
                                c = (b2 << 12) | ((b1 & 0x3f) << 6) | c;
                                return c;
                            }
                        } else if (valid_lead4_and_t1(b2, b1)) {
                            *pi = i;
                            return -1;
                        }
                    } else if (boost::text::continuation(b2) && i > start) {
                        uint8_t b3 = it[--i];
                        if (0xf0 <= b3 && b3 <= 0xf4) {
                            b3 &= 7;
                            if (valid_lead4_and_t1(b3, b2)) {
                                *pi = i;
                                c = (b3 << 18) | ((b2 & 0x3f) << 12) |
                                    ((b1 & 0x3f) << 6) | c;
                                return c;
                            }
                        }
                    }
                }
            }
            return -1;
        }
        static bool valid_lead3_and_t1(uint8_t lead, uint8_t t1) noexcept
        {
            return u8_lead3_t1_bits[lead & 0xf] & (1 << (t1 >> 5));
        }
        static bool valid_lead4_and_t1(uint8_t lead, uint8_t t1) noexcept
        {
            return u8_lead4_t1_bits[t1 >> 4] & (1 << (lead & 7));
        }

        enum {
            fast_shift = 6,
            fast_data_block_length = 1 << fast_shift,
            fast_data_mask = fast_data_block_length - 1,
            small_max = 0xfff,
            error_value_neg_data_offset = 1,
            high_value_neg_data_offset = 2,
            bmp_index_length = 0x10000 >> fast_shift,
            small_limit = 0x1000,
            small_index_length = small_limit >> fast_shift,
            shift_3 = 4,
            shift_2 = 5 + shift_3,
            shift_1 = 5 + shift_2,
            shift_2_3 = shift_2 - shift_3,
            shift_1_2 = shift_1 - shift_2,
            omitted_bmp_index_1_length = 0x10000 >> shift_1,
            index_2_block_length = 1 << shift_1_2,
            index_2_mask = index_2_block_length - 1,
            cp_per_index_2_entry = 1 << shift_2,
            index_3_block_length = 1 << shift_2_3,
            index_3_mask = index_3_block_length - 1,
            small_data_block_length = 1 << shift_3,
            small_data_mask = small_data_block_length - 1
        };

        uint16_t const * index;
        uint16_t const * data;

        int32_t index_size;
        int32_t data_size;
        int32_t high_start;
        uint16_t shifted_12_high_start;

        int8_t ignore_0;
        int8_t ignore_1;
        uint32_t ignore_2;
        uint16_t ignore_3;

        uint16_t index3_null_offset;
        int32_t data_null_offset;
        uint32_t null_value;
    };

}}}

#endif
