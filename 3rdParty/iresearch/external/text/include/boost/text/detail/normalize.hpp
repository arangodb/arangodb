// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_NORMALIZE_HPP
#define BOOST_TEXT_DETAIL_NORMALIZE_HPP

#include <boost/text/transcode_algorithm.hpp>
#include <boost/text/detail/normalization_data.hpp>
#include <boost/text/detail/normalization_table_data.hpp>
#include <boost/text/detail/norm2_nfc_data.hpp>
#include <boost/text/detail/norm2_nfkc_data.hpp>

#include <boost/assert.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/algorithm/cxx14/equal.hpp>


namespace boost { namespace text { namespace detail {

    inline normalization_table_data const & nfc_table()
    {
        static normalization_table_data retval(
            norm2_nfc_data_indexes,
            norm2_nfc_data_trie,
            norm2_nfc_data_extraData,
            norm2_nfc_data_smallFCD);
        return retval;
    }

    inline normalization_table_data const & nfkc_table()
    {
        static normalization_table_data retval(
            norm2_nfkc_data_indexes,
            norm2_nfkc_data_trie,
            norm2_nfkc_data_extraData,
            norm2_nfkc_data_smallFCD);
        return retval;
    }

    template<typename String>
    struct utf8_string_appender
    {
        explicit utf8_string_appender(String & s) : s_(&s) {}

        template<typename CharIter>
        char_iter_ret_t<void, CharIter> append(CharIter first, CharIter last)
        {
            s_->insert(s_->end(), first, last);
        }

        typename String::iterator out() const { return s_->end(); }

    private:
        String * s_;
    };

    template<typename String>
    struct utf16_string_appender
    {
        explicit utf16_string_appender(String & s) : s_(&s) {}

        template<typename CharIter>
        _16_iter_ret_t<void, CharIter> append(CharIter first, CharIter last)
        {
            s_->insert(s_->end(), first, last);
        }

        typename String::iterator out() const { return s_->end(); }

    private:
        String * s_;
    };

    template<typename UTF32OutIter>
    struct utf8_to_utf32_appender
    {
        explicit utf8_to_utf32_appender(UTF32OutIter out) : out_(out) {}

        template<typename CharIter>
        char_iter_ret_t<void, CharIter> append(CharIter first, CharIter last)
        {
            out_ = boost::text::transcode_to_utf32(first, last, out_);
        }

        UTF32OutIter out() const { return out_; }

    private:
        UTF32OutIter out_;
    };

    struct null_appender
    {
        null_appender(bool = true) {}
        template<typename Iter>
        void append(Iter first, Iter last)
        {}
        bool out() const { return true; }
    };

    template<typename String>
    struct utf16_appender
    {
        explicit utf16_appender(String & s) : s_(&s) {}

        template<typename Iter>
        _16_iter_ret_t<void, Iter> append(Iter utf16_first, Iter utf16_last)
        {
            s_->insert(s_->end(), utf16_first, utf16_last);
        }

    private:
        String * s_;
    };

    template<typename String>
    struct utf16_to_utf8_string_appender
    {
        explicit utf16_to_utf8_string_appender(String & s) : s_(&s) {}

        template<typename Iter>
        _16_iter_ret_t<void, Iter> append(Iter utf16_first, Iter utf16_last)
        {
            if (utf16_first == utf16_last)
                return;
            auto const dist = std::distance(utf16_first, utf16_last);
            auto const initial_size = s_->size();
            s_->resize(initial_size + dist * 3, typename String::value_type{});
            auto * s_first = &*s_->begin();
            auto * out = s_first + initial_size;
            out = boost::text::transcode_to_utf8(utf16_first, utf16_last, out);
            s_->resize(out - s_first, typename String::value_type{});
        }

        typename String::iterator out() const noexcept { return s_->end(); }

    private:
        String * s_;
    };

    template<typename UTF32OutIter>
    struct utf16_to_utf32_appender
    {
        explicit utf16_to_utf32_appender(UTF32OutIter out) : out_(out) {}

        template<typename Iter>
        _16_iter_ret_t<void, Iter> append(Iter utf16_first, Iter utf16_last)
        {
            out_ =
                boost::text::transcode_to_utf32(utf16_first, utf16_last, out_);
        }

        UTF32OutIter out() const { return out_; }

    private:
        UTF32OutIter out_;
    };

    template<typename UTF16Appender>
    class reordering_appender;

    template<typename UTF16Appender>
    struct flush_disinhibitor
    {
        flush_disinhibitor(reordering_appender<UTF16Appender> & buf) : buf_(buf)
        {}
        ~flush_disinhibitor() { buf_.inhibit_flushes_ = false; }

    private:
        reordering_appender<UTF16Appender> & buf_;
    };

    template<typename UTF16Appender>
    class reordering_appender
    {
        friend flush_disinhibitor<UTF16Appender>;

    public:
        reordering_appender(
            normalization_table_data const & table, UTF16Appender & appender) :
            table_(table),
            appender_(appender),
            buf_(),
            first_(begin()),
            last_(begin()),
            last_cc_(0),
            inhibit_flushes_(false)
        {}
        ~reordering_appender() { flush(); }

        flush_disinhibitor<UTF16Appender> inhibit_flush()
        {
            inhibit_flushes_ = true;
            return flush_disinhibitor<UTF16Appender>(*this);
        }

        int size() const noexcept { return last_ - begin(); }

        uint16_t const * begin() const { return buf_.data(); }
        uint16_t const * end() const { return last_; }
        uint16_t * begin() { return buf_.data(); }
        uint16_t * end() { return last_; }

        template<typename Iter>
        bool equals_utf16(Iter first, Iter last) const
        {
            return algorithm::equal(begin(), end(), first, last);
        }
        template<typename CharIter>
        bool equals_utf8(CharIter first, CharIter last) const
        {
            auto const other = boost::text::as_utf16(first, last);
            return algorithm::equal(begin(), end(), other.begin(), other.end());
        }

        void append(int32_t c, uint8_t cc)
        {
            if (c <= 0xffff)
                return append_bmp(c, cc);
            if (last_cc_ <= cc || cc == 0) {
                if (cc == 0 && !inhibit_flushes_)
                    flush();
                last_ = detail::write_cp_utf16(c, last_);
                last_cc_ = cc;
                if (cc <= 1)
                    first_ = last_;
            } else {
                insert(c, cc);
            }
        }

        void append(
            uint16_t const * first,
            uint16_t const * last,
            uint8_t lead_cc,
            uint8_t trail_cc)
        {
            auto next_cp = [](uint16_t const *& first,
                              uint16_t const * last) -> int32_t {
                int32_t cp = *first++;
                if (boost::text::high_surrogate(cp)) {
                    uint16_t cu = 0;
                    if (first != last &&
                        boost::text::low_surrogate(cu = *first)) {
                        ++first;
                        cp = detail::surrogates_to_cp(cp, cu);
                    }
                }
                return cp;
            };

            if (last_cc_ <= lead_cc || lead_cc == 0) {
                if (trail_cc <= 1)
                    first_ = last_ + (last - first);
                else if (lead_cc <= 1)
                    first_ = last_ + 1;
                last_ = std::copy(first, last, last_);
                last_cc_ = trail_cc;
            } else {
                int32_t c = next_cp(first, last);
                insert(c, lead_cc);
                while (first != last) {
                    c = next_cp(first, last);
                    if (first != last) {
                        lead_cc = table_.get_cc_from_yes_or_maybe(
                            table_.trie.fast_get(c));
                    } else {
                        lead_cc = trail_cc;
                    }
                    append(c, lead_cc);
                }
            }
        }

        void append_bmp(uint16_t c, uint8_t cc)
        {
            if (last_cc_ <= cc || cc == 0) {
                if (cc == 0 && !inhibit_flushes_)
                    flush();
                *last_++ = c;
                last_cc_ = cc;
                if (cc <= 1)
                    first_ = last_;
            } else {
                insert(c, cc);
            }
        }

        template<typename U16Iter>
        void append_ccc_zero(U16Iter first, U16Iter last)
        {
            BOOST_ASSERT(first != last);
            if (!inhibit_flushes_) {
                flush();
                auto second_to_last = std::prev(last);
                if (boost::text::low_surrogate(*second_to_last))
                    --second_to_last;
                appender_.append(first, second_to_last);
                last_ = std::copy(second_to_last, last, last_);
            } else {
                last_ = std::copy(first, last, last_);
            }
            last_cc_ = 0;
            first_ = last_;
        }

        void clear()
        {
            first_ = last_ = begin();
            last_cc_ = 0;
        }

        void set_last(uint16_t * last)
        {
            first_ = last_ = last;
            last_cc_ = 0;
        }

    private:
        void flush()
        {
            appender_.append(begin(), last_);
            clear();
        }

        void insert(int32_t c, uint8_t cc)
        {
            for (set_first(), skip_previous(); cc < previous_cc();) {
            }
            uint16_t * prev_last = last_;
            last_ += uint32_t(c) <= 0xffff ? 1 : 2;
            uint16_t * it = last_ - (prev_last - code_point_last_);
            std::copy(code_point_last_, prev_last, it);
            write_code_point(code_point_last_, c);
            if (cc <= 1)
                first_ = it;
        }
        static void write_code_point(uint16_t * it, int32_t c)
        {
            if (c <= 0xffff)
                *it = c;
            else
                detail::write_cp_utf16(c, it);
        }

        void set_first() { code_point_first_ = last_; }
        void skip_previous()
        {
            code_point_last_ = code_point_first_;
            uint16_t c = *--code_point_first_;
            if (boost::text::low_surrogate(c) && begin() < code_point_first_ &&
                boost::text::high_surrogate(*(code_point_first_ - 1))) {
                --code_point_first_;
            }
        }
        uint8_t previous_cc()
        {
            code_point_last_ = code_point_first_;
            if (code_point_first_ <= first_)
                return 0;
            int32_t c = *--code_point_first_;
            uint16_t c2;
            if (boost::text::low_surrogate(c) && begin() < code_point_first_ &&
                boost::text::high_surrogate(c2 = *(code_point_first_ - 1))) {
                --code_point_first_;
                c = detail::surrogates_to_cp(c2, c);
            }
            return table_.get_cc_from_yes_or_maybe_cp(c);
        }

        normalization_table_data const & table_;
        UTF16Appender & appender_;
        std::array<uint16_t, 1024> buf_;
        uint16_t * first_;
        uint16_t * last_;
        uint8_t last_cc_;
        bool inhibit_flushes_;
        uint16_t * code_point_first_;
        uint16_t * code_point_last_;
    };

    template<typename UTF8Appender>
    void append_utf16(
        uint16_t const * first, uint16_t const * last, UTF8Appender & appender)
    {
        int const buffer_size = 200;
        char buffer[buffer_size];

        char * out = buffer;
        int32_t const capacity = buffer_size - (4 - 1);
        char * const out_last = buffer + capacity;

        while (first != last) {
            while (first != last && out != out_last) {
                int32_t c = *first++;
                if (boost::text::high_surrogate(c))
                    c = detail::surrogates_to_cp(c, *first++);
                out = detail::write_cp_utf8(c, out);
            }
            appender.append(buffer, out);
        }
    }

    template<typename UTF8Appender>
    void append_utf32(int32_t c, UTF8Appender & appender)
    {
        char utf8[4];
        auto const last = detail::write_cp_utf8(c, utf8);
        appender.append(utf8, last);
    }

    template<typename CharIter>
    int32_t utf8_to_cp(CharIter first, CharIter last) noexcept
    {
        switch (last - first) {
        case 1: return first[0];
        case 2: return ((first[0] & 0x1f) << 6) | ((uint8_t)first[1] & 0x3f);
        case 3:
            return (uint16_t)(
                (first[0] << 12) | (((uint8_t)first[1] & 0x3f) << 6) |
                ((uint8_t)first[2] & 0x3f));
        case 4:
            return ((first[0] & 0x7) << 18) |
                   (((uint8_t)first[1] & 0x3f) << 12) |
                   (((uint8_t)first[2] & 0x3f) << 6) |
                   ((uint8_t)first[3] & 0x3f);
        default: BOOST_ASSERT(!"unreachable");
        }
        return -1;
    }

    template<typename CharIter>
    int32_t previous_hangul_or_jamo(CharIter first, CharIter last) noexcept
    {
        if (3 <= (last - first)) {
            last -= 3;
            uint8_t l = *last;
            uint8_t t1, t2;
            if (0xe1 <= l && l <= 0xed &&
                (t1 = (uint8_t)((uint8_t)last[1] - 0x80)) <= 0x3f &&
                (t2 = (uint8_t)((uint8_t)last[2] - 0x80)) <= 0x3f &&
                (l < 0xed || t1 <= 0x1f)) {
                return ((l & 0xf) << 12) | (t1 << 6) | t2;
            }
        }
        return -1;
    }

    template<typename CharIter, typename Sentinel>
    int32_t jamo_t_minus_base(CharIter first, Sentinel last) noexcept
    {
        if (3 <= boost::text::distance(first, last) &&
            (uint8_t)*first == 0xe1) {
            if ((uint8_t)first[1] == 0x86) {
                uint8_t t = first[2];
                if (0xa8 <= t && t <= 0xbf)
                    return t - 0xa7;
            } else if ((uint8_t)first[1] == 0x87) {
                uint8_t t = first[2];
                if ((int8_t)t <= (int8_t)0x82u)
                    return t - (0xa7 - 0x40);
            }
        }
        return -1;
    }

    template<typename CharIter, typename UTF8Appender>
    void append_code_point_delta(
        CharIter first, CharIter last, int32_t delta, UTF8Appender & appender)
    {
        char buffer[4];
        auto buffer_last = buffer;
        if ((first - last) == 1) {
            buffer[0] = *first + delta;
            buffer_last = buffer + 1;
        } else {
            int32_t trail = *std::prev(last) + delta;
            if (0x80 <= trail && trail <= 0xbf) {
                --last;
                buffer_last = std::copy(first, last, buffer);
                *buffer_last++ = trail;
            } else {
                int32_t const c = detail::utf8_to_cp(first, last) + delta;
                buffer_last = detail::write_cp_utf8(c, buffer);
            }
        }
        appender.append(buffer, buffer_last);
    }

    template<typename UTF16Appender>
    bool decompose(
        normalization_table_data const & table,
        int32_t c,
        uint16_t norm16,
        reordering_appender<UTF16Appender> & buffer)
    {
        if (table.limit_no_no <= norm16) {
            if (table.maybe_or_non_zero_cc(norm16)) {
                buffer.append(c, table.get_cc_from_yes_or_maybe(norm16));
                return true;
            }
            c = table.map_algorithmic(c, norm16);
            norm16 = table.trie.fast_get(c);
        }
        if (norm16 < table.min_yes_no) {
            buffer.append(c, 0);
            return true;
        } else if (table.hangul_lv(norm16) || table.hangul_lvt(norm16)) {
            auto const decomp = decompose_hangul_syllable<3, uint16_t>(c);
            buffer.append_ccc_zero(decomp.begin(), decomp.end());
            return true;
        }
        uint16_t const * mapping = table.get_mapping(norm16);
        uint16_t first_unit = *mapping;
        int32_t length = first_unit & table.mapping_length_mask;
        uint8_t lead_cc, trail_cc;
        trail_cc = (uint8_t)(first_unit >> 8);
        if (first_unit & table.mapping_has_ccc_lccc_word)
            lead_cc = (uint8_t)(*(mapping - 1) >> 8);
        else
            lead_cc = 0;
        buffer.append(mapping + 1, mapping + 1 + length, lead_cc, trail_cc);
        return true;
    }

    template<typename Iter, typename Sentinel, typename UTF16Appender>
    Iter decompose_short_utf16(
        normalization_table_data const & table,
        Iter first,
        Sentinel last,
        bool stop_at_comp_boundary,
        bool only_contiguous,
        reordering_appender<UTF16Appender> & buffer)
    {
        while (first != last) {
            if (stop_at_comp_boundary && *first < table.min_comp_no_maybe_cp)
                return first;
            Iter prev_first = first;
            int32_t c = 0;
            uint16_t norm16 = table.trie.fast_u16_next(first, last, c);
            if (stop_at_comp_boundary &&
                table.norm16_comp_boundary_before(norm16))
                return prev_first;
            if (!detail::decompose(table, c, norm16, buffer))
                return first;
            if (stop_at_comp_boundary &&
                table.norm16_comp_boundary_after(norm16, only_contiguous)) {
                return first;
            }
        }
        return first;
    }

    template<typename CharIter, typename Sentinel, typename UTF16Appender>
    CharIter decompose_short_utf8(
        normalization_table_data const & table,
        CharIter first,
        Sentinel last,
        bool stop_at_comp_boundary,
        bool only_contiguous,
        reordering_appender<UTF16Appender> & buffer)
    {
        while (first != last) {
            CharIter prev_first = first;
            uint16_t norm16 = table.trie.fast_u8_next(first, last);
            int32_t c = -1;
            if (table.limit_no_no <= norm16) {
                if (table.maybe_or_non_zero_cc(norm16)) {
                    c = detail::utf8_to_cp(prev_first, first);
                    buffer.append(c, table.get_cc_from_yes_or_maybe(norm16));
                    continue;
                }
                if (stop_at_comp_boundary)
                    return prev_first;
                c = detail::utf8_to_cp(prev_first, first);
                c = table.map_algorithmic(c, norm16);
                norm16 = table.trie.fast_get(c);
            } else if (
                stop_at_comp_boundary &&
                norm16 < table.min_no_no_comp_no_maybe_cc) {
                return prev_first;
            }
            BOOST_ASSERT(norm16 != table.inert);
            if (norm16 < table.min_yes_no) {
                if (c < 0)
                    c = detail::utf8_to_cp(prev_first, first);
                buffer.append(c, 0);
            } else if (table.hangul_lv(norm16) || table.hangul_lvt(norm16)) {
                if (c < 0)
                    c = detail::utf8_to_cp(prev_first, first);
                auto const decomp = decompose_hangul_syllable<3, uint16_t>(c);
                buffer.append_ccc_zero(decomp.begin(), decomp.end());
            } else {
                uint16_t const * mapping = table.get_mapping(norm16);
                uint16_t first_unit = *mapping;
                int32_t length = first_unit & table.mapping_length_mask;
                uint8_t trail_cc = (uint8_t)(first_unit >> 8);
                uint8_t lead_cc;
                if (first_unit & table.mapping_has_ccc_lccc_word)
                    lead_cc = (uint8_t)(*(mapping - 1) >> 8);
                else
                    lead_cc = 0;
                buffer.append(
                    mapping + 1, mapping + 1 + length, lead_cc, trail_cc);
            }
            if (stop_at_comp_boundary &&
                table.norm16_comp_boundary_after(norm16, only_contiguous)) {
                return first;
            }
        }
        return first;
    }

    inline int32_t combine(
        normalization_table_data const & table,
        uint16_t const * list,
        int32_t trail)
    {
        uint16_t key1, first_unit;
        if (trail < table.comp_1_trail_limit) {
            key1 = (uint16_t)(trail << 1);
            while ((first_unit = *list) < key1) {
                list += 2 + (first_unit & table.comp_1_triple);
            }
            if (key1 == (first_unit & table.comp_1_trail_mask)) {
                if (first_unit & table.comp_1_triple)
                    return ((int32_t)list[1] << 16) | list[2];
                else
                    return list[1];
            }
        } else {
            key1 = (uint16_t)(
                table.comp_1_trail_limit +
                (((trail >> table.comp_1_trail_shift)) & ~table.comp_1_triple));
            uint16_t key2 = (uint16_t)(trail << table.comp_2_trail_shift);
            uint16_t second_unit;
            for (;;) {
                if ((first_unit = *list) < key1) {
                    list += 2 + (first_unit & table.comp_1_triple);
                } else if (key1 == (first_unit & table.comp_1_trail_mask)) {
                    if ((second_unit = list[1]) < key2) {
                        if (first_unit & table.comp_1_last_tuple)
                            break;
                        else
                            list += 3;
                    } else if (
                        key2 == (second_unit & table.comp_2_trail_mask)) {
                        return ((int32_t)(
                                    second_unit & ~table.comp_2_trail_mask)
                                << 16) |
                               list[2];
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
        }
        return -1;
    }

    template<typename UTF16Appender>
    void recompose(
        normalization_table_data const & table,
        reordering_appender<UTF16Appender> & buffer,
        int32_t recompose_first_index,
        bool only_contiguous)
    {
        uint16_t * it = buffer.begin() + recompose_first_index;
        uint16_t * last = buffer.end();
        if (it == last)
            return;

        uint16_t * starter = nullptr;
        uint16_t * remove = nullptr;
        uint16_t const * compositions_list = nullptr;
        int32_t c = 0;
        int32_t composite_and_fwd = 0;
        uint16_t norm16 = 0;
        uint8_t prev_cc = 0;
        bool starter_supplementary = false;

        for (;;) {
            norm16 = table.trie.fast_u16_next(it, last, c);
            uint8_t cc = table.get_cc_from_yes_or_maybe(norm16);
            if (table.maybe(norm16) && compositions_list != nullptr &&
                (prev_cc < cc || prev_cc == 0)) {
                if (table.get_jamo_vt(norm16)) {
                    if (c < TBase) {
                        uint16_t prev = *starter - LBase;
                        if (prev < LCount) {
                            remove = it - 1;
                            uint16_t syllable =
                                SBase + (prev * VCount + (c - VBase)) * TCount;
                            uint16_t t;
                            if (it != last &&
                                (t = (uint16_t)(*it - TBase)) < TCount) {
                                ++it;
                                syllable += t;
                            }
                            *starter = syllable;
                            last = std::copy(it, last, remove);
                            it = remove;
                        }
                    }
                    if (it == last)
                        break;
                    compositions_list = nullptr;
                    continue;
                } else if (
                    0 <= (composite_and_fwd =
                              detail::combine(table, compositions_list, c))) {
                    int32_t composite = composite_and_fwd >> 1;

                    remove = it - (uint32_t(c) <= 0xffff ? 1 : 2);
                    if (starter_supplementary) {
                        if ((uint32_t)(composite - 0x10000) <= 0xfffff) {
                            detail::write_cp_utf16(composite, starter);
                        } else {
                            *starter = (uint16_t)composite;
                            starter_supplementary = false;
                            std::copy(starter + 2, remove, starter + 1);
                            --remove;
                        }
                    } else if ((uint32_t)(composite - 0x10000) <= 0xfffff) {
                        starter_supplementary = true;
                        ++starter;
                        ++remove;
                        std::copy_backward(starter, remove - 1, remove);
                        --starter;
                        detail::write_cp_utf16(composite, starter);
                    } else {
                        *starter = (uint16_t)composite;
                    }

                    if (remove < it) {
                        last = std::copy(it, last, remove);
                        it = remove;
                    }

                    if (it == last)
                        break;
                    if (composite_and_fwd & 1) {
                        compositions_list =
                            table.get_compositions_list_for_composite(
                                table.trie.fast_get(composite));
                    } else {
                        compositions_list = nullptr;
                    }

                    continue;
                }
            }

            prev_cc = cc;
            if (it == last)
                break;

            if (cc == 0) {
                if ((compositions_list =
                         table.get_compositions_list_for_decomp_yes(norm16)) !=
                    nullptr) {
                    if (uint32_t(c) <= 0xffff) {
                        starter_supplementary = false;
                        starter = it - 1;
                    } else {
                        starter_supplementary = true;
                        starter = it - 2;
                    }
                }
            } else if (only_contiguous) {
                compositions_list = nullptr;
            }
        }
        buffer.set_last(last);
    }

    template<
        bool WriteToOut,
        typename Iter,
        typename Sentinel,
        typename UTF16Appender>
    Iter decompose(
        normalization_table_data const & table,
        Iter first,
        Sentinel last,
        reordering_appender<UTF16Appender> & buffer)
    {
        int32_t const min_no_cp = table.min_decomp_no_cp;

        Iter prev_first = first;
        int32_t c = 0;
        uint16_t norm16 = 0;

        Iter prev_boundary = first;
        uint8_t prev_cc = 0;

        for (;;) {
            for (prev_first = first; first != last;) {
                if ((c = *first) < min_no_cp ||
                    table.most_decomp_yes_and_zero_cc(
                        norm16 = table.trie.fast_bmp_get(c))) {
                    ++first;
                } else if (!boost::text::high_surrogate(c)) {
                    break;
                } else {
                    uint16_t c2;
                    auto next = std::next(first);
                    if (next != last &&
                        boost::text::low_surrogate(c2 = *next)) {
                        c = detail::surrogates_to_cp(c, c2);
                        norm16 = table.trie.fast_supp_get(c);
                        if (table.most_decomp_yes_and_zero_cc(norm16))
                            std::advance(first, 2);
                        else
                            break;
                    } else {
                        ++first;
                    }
                }
            }
            if (first != prev_first) {
                if (WriteToOut) {
                    buffer.append_ccc_zero(prev_first, first);
                } else {
                    prev_cc = 0;
                    prev_boundary = first;
                }
            }
            if (first == last)
                break;

            std::advance(first, uint32_t(c) <= 0xffff ? 1 : 2);
            if (WriteToOut) {
                if (!detail::decompose(table, c, norm16, buffer))
                    break;
            } else {
                if (table.decomp_yes(norm16)) {
                    uint8_t cc = table.get_cc_from_yes_or_maybe(norm16);
                    if (prev_cc <= cc || cc == 0) {
                        prev_cc = cc;
                        if (cc <= 1)
                            prev_boundary = first;
                        continue;
                    }
                }
                return prev_boundary;
            }
        }
        return first;
    }

    template<
        bool OnlyContiguous,
        bool WriteToOut,
        typename Iter,
        typename Sentinel,
        typename UTF16Appender>
    bool compose(
        normalization_table_data const & table,
        Iter first,
        Sentinel last,
        reordering_appender<UTF16Appender> & buffer)
    {
        Iter prev_boundary = first;
        int32_t min_no_maybe_cp = table.min_comp_no_maybe_cp;

        for (;;) {
            Iter prev_first = first;
            int32_t c = 0;
            uint16_t norm16 = 0;
            for (;;) {
                if (first == last) {
                    if (prev_boundary != last && WriteToOut)
                        buffer.append_ccc_zero(prev_boundary, first);
                    return true;
                }
                if ((c = *first) < min_no_maybe_cp ||
                    table.comp_yes_and_zero_cc(
                        norm16 = table.trie.fast_bmp_get(c))) {
                    ++first;
                } else {
                    prev_first = first++;
                    if (!boost::text::high_surrogate(c)) {
                        break;
                    } else {
                        uint16_t c2 = 0;
                        if (first != last &&
                            boost::text::low_surrogate(c2 = *first)) {
                            ++first;
                            c = detail::surrogates_to_cp(c, c2);
                            norm16 = table.trie.fast_supp_get(c);
                            if (!table.comp_yes_and_zero_cc(norm16))
                                break;
                        }
                    }
                }
            }

            if (!table.maybe_or_non_zero_cc(norm16)) {
                if (!WriteToOut)
                    return false;
                if (table.decomp_no_algorithmic(norm16)) {
                    if (table.norm16_comp_boundary_after(
                            norm16, OnlyContiguous) ||
                        table.comp_boundary_before_utf16(first, last)) {
                        if (prev_boundary != prev_first) {
                            buffer.append_ccc_zero(prev_boundary, prev_first);
                        }
                        buffer.append(table.map_algorithmic(c, norm16), 0);
                        prev_boundary = first;
                        continue;
                    }
                } else if (norm16 < table.min_no_no_compboundary_before) {
                    if (table.norm16_comp_boundary_after(
                            norm16, OnlyContiguous) ||
                        table.comp_boundary_before_utf16(first, last)) {
                        if (prev_boundary != prev_first) {
                            buffer.append_ccc_zero(prev_boundary, prev_first);
                        }
                        uint16_t const * mapping =
                            reinterpret_cast<uint16_t const *>(
                                table.get_mapping(norm16));
                        int32_t length = *mapping++ & table.mapping_length_mask;
                        buffer.append_ccc_zero(mapping, mapping + length);
                        prev_boundary = first;
                        continue;
                    }
                } else if (table.min_no_no_empty <= norm16) {
                    if (table.comp_boundary_before_utf16(first, last) ||
                        table.comp_boundary_after_utf16(
                            prev_boundary, prev_first, OnlyContiguous)) {
                        if (prev_boundary != prev_first) {
                            buffer.append_ccc_zero(prev_boundary, prev_first);
                        }
                        prev_boundary = first;
                        continue;
                    }
                }
            } else if (
                table.get_jamo_vt(norm16) && prev_boundary != prev_first) {
                uint16_t prev = *std::prev(prev_first);
                if (c < TBase) {
                    uint16_t l = (uint16_t)(prev - LBase);
                    if (l < LCount) {
                        if (!WriteToOut)
                            return false;
                        int32_t t;
                        if (first != last &&
                            0 < (t = ((int32_t)*first - TBase)) && t < TCount) {
                            ++first;
                        } else if (table.comp_boundary_before_utf16(
                                       first, last)) {
                            t = 0;
                        } else {
                            t = -1;
                        }
                        if (0 <= t) {
                            int32_t syllable =
                                SBase + (l * VCount + (c - VBase)) * TCount + t;
                            --prev_first;
                            if (prev_boundary != prev_first) {
                                buffer.append_ccc_zero(
                                    prev_boundary, prev_first);
                            }
                            buffer.append_bmp(syllable, 0);
                            prev_boundary = first;
                            continue;
                        }
                    }
                } else if (detail::hangul_lv(prev)) {
                    if (!WriteToOut)
                        return false;
                    int32_t syllable = prev + c - TBase;
                    --prev_first;
                    if (prev_boundary != prev_first)
                        buffer.append_ccc_zero(prev_boundary, prev_first);
                    buffer.append_bmp(syllable, 0);
                    prev_boundary = first;
                    continue;
                }
            } else if (table.jamo_vt < norm16) {
                uint8_t cc = table.get_cc_from_normal_yes_or_maybe(norm16);
                if (OnlyContiguous && cc < table.get_previous_trail_cc_utf16(
                                               prev_boundary, prev_first)) {
                    if (!WriteToOut)
                        return false;
                } else {
                    Iter next_first = first;
                    uint16_t n16 = 0;
                    for (;;) {
                        if (first == last) {
                            if (WriteToOut)
                                buffer.append_ccc_zero(prev_boundary, first);
                            return true;
                        }
                        uint8_t prev_cc = cc;
                        next_first = first;
                        n16 = table.trie.fast_u16_next(next_first, last, c);
                        if (table.min_yes_yes_with_cc <= n16) {
                            cc = table.get_cc_from_normal_yes_or_maybe(n16);
                            if (cc < prev_cc) {
                                if (!WriteToOut)
                                    return false;
                                break;
                            }
                        } else {
                            break;
                        }
                        first = next_first;
                    }
                    if (table.norm16_comp_boundary_before(n16)) {
                        if (table.comp_yes_and_zero_cc(n16))
                            first = next_first;
                        continue;
                    }
                }
            }

            if (prev_boundary != prev_first &&
                !table.norm16_comp_boundary_before(norm16)) {
                Iter it = prev_first;
                norm16 = table.trie.fast_u16_prev(prev_boundary, it, c);
                if (!table.norm16_comp_boundary_after(norm16, OnlyContiguous))
                    prev_first = it;
            }
            if (WriteToOut && prev_boundary != prev_first)
                buffer.append_ccc_zero(prev_boundary, prev_first);
            auto const no_flush = buffer.inhibit_flush();
            int32_t recompose_first_index = buffer.size();
            detail::decompose_short_utf16(
                table, prev_first, first, false, OnlyContiguous, buffer);
            first = detail::decompose_short_utf16(
                table, first, last, true, OnlyContiguous, buffer);
            BOOST_ASSERT(std::distance(prev_first, first) <= INT32_MAX);
            detail::recompose(
                table, buffer, recompose_first_index, OnlyContiguous);
            if (!WriteToOut) {
                if (!buffer.equals_utf16(prev_first, first))
                    return false;
                buffer.clear();
            }
            prev_boundary = first;
        }
        return true;
    }

    template<
        bool OnlyContiguous,
        bool WriteToOut,
        typename CharIter,
        typename Sentinel,
        typename UTF8Appender>
    bool compose_utf8(
        normalization_table_data const & table,
        CharIter first,
        Sentinel last,
        UTF8Appender & appender)
    {
        container::small_vector<uint16_t, 1024> s16;
        uint8_t min_no_maybe_lead =
            table.min_comp_no_maybe_cp <= 0x7f
                ? table.min_comp_no_maybe_cp
                : table.min_comp_no_maybe_cp <= 0x7ff
                      ? 0xc0 + (table.min_comp_no_maybe_cp >> 6)
                      : 0xe0;
        CharIter prev_boundary = first;

        for (;;) {
            CharIter prev_first = first;
            uint16_t norm16 = 0;
            for (;;) {
                if (first == last) {
                    if (prev_boundary != last && WriteToOut)
                        appender.append(prev_boundary, first);
                    return true;
                }
                if ((uint8_t)*first < min_no_maybe_lead) {
                    ++first;
                } else {
                    prev_first = first;
                    norm16 = table.trie.fast_u8_next(first, last);
                    if (!table.comp_yes_and_zero_cc(norm16))
                        break;
                }
            }

            if (!table.maybe_or_non_zero_cc(norm16)) {
                if (!WriteToOut)
                    return false;
                if (table.decomp_no_algorithmic(norm16)) {
                    if (table.norm16_comp_boundary_after(
                            norm16, OnlyContiguous) ||
                        table.comp_boundary_before_utf8(first, last)) {
                        if (prev_boundary != prev_first)
                            appender.append(prev_boundary, prev_first);
                        detail::append_code_point_delta(
                            prev_first,
                            first,
                            table.get_algorithmic_delta(norm16),
                            appender);
                        prev_boundary = first;
                        continue;
                    }
                } else if (norm16 < table.min_no_no_compboundary_before) {
                    if (table.norm16_comp_boundary_after(
                            norm16, OnlyContiguous) ||
                        table.comp_boundary_before_utf8(first, last)) {
                        if (prev_boundary != prev_first)
                            appender.append(prev_boundary, prev_first);
                        uint16_t const * mapping = table.get_mapping(norm16);
                        int32_t length = *mapping++ & table.mapping_length_mask;
                        detail::append_utf16(
                            mapping, mapping + length, appender);
                        prev_boundary = first;
                        continue;
                    }
                } else if (table.min_no_no_empty <= norm16) {
                    if (table.comp_boundary_before_utf8(first, last) ||
                        table.comp_boundary_after_utf8(
                            prev_boundary, prev_first, OnlyContiguous)) {
                        if (prev_boundary != prev_first)
                            appender.append(prev_boundary, prev_first);
                        prev_boundary = first;
                        continue;
                    }
                }
            } else if (table.get_jamo_vt(norm16)) {
                BOOST_ASSERT(
                    (first - prev_first) == 3 && (uint8_t)*prev_first == 0xe1);
                int32_t prev =
                    detail::previous_hangul_or_jamo(prev_boundary, prev_first);
                if ((uint8_t)prev_first[1] == 0x85) {
                    int32_t l = prev - LBase;
                    if ((uint32_t)l < LCount) {
                        if (!WriteToOut)
                            return false;
                        int32_t t = detail::jamo_t_minus_base(first, last);
                        if (0 <= t) {
                            first += 3;
                        } else if (table.comp_boundary_before_utf8(
                                       first, last)) {
                            t = 0;
                        }
                        if (0 <= t) {
                            int32_t syllable =
                                SBase +
                                (l * VCount + ((uint8_t)prev_first[2] - 0xa1)) *
                                    TCount +
                                t;
                            prev_first -= 3;
                            if (prev_boundary != prev_first)
                                appender.append(prev_boundary, prev_first);
                            detail::append_utf32(syllable, appender);
                            prev_boundary = first;
                            continue;
                        }
                    }
                } else if (detail::hangul_lv(prev)) {
                    if (!WriteToOut)
                        return false;
                    int32_t syllable =
                        prev + detail::jamo_t_minus_base(prev_first, first);
                    prev_first -= 3;
                    if (prev_boundary != prev_first)
                        appender.append(prev_boundary, prev_first);
                    detail::append_utf32(syllable, appender);
                    prev_boundary = first;
                    continue;
                }
            } else if (table.jamo_vt < norm16) {
                uint8_t cc = table.get_cc_from_normal_yes_or_maybe(norm16);
                if (OnlyContiguous && cc < table.get_previous_trail_cc_utf8(
                                               prev_boundary, prev_first)) {
                    if (!WriteToOut)
                        return false;
                } else {
                    CharIter next_first = first;
                    uint16_t n16 = 0;
                    for (;;) {
                        if (first == last) {
                            if (WriteToOut)
                                appender.append(prev_boundary, first);
                            return true;
                        }
                        uint8_t prev_cc = cc;
                        next_first = first;
                        n16 = table.trie.fast_u8_next(next_first, last);
                        if (table.min_yes_yes_with_cc <= n16) {
                            cc = table.get_cc_from_normal_yes_or_maybe(n16);
                            if (cc < prev_cc) {
                                if (!WriteToOut)
                                    return false;
                                break;
                            }
                        } else {
                            break;
                        }
                        first = next_first;
                    }
                    if (table.norm16_comp_boundary_before(n16)) {
                        if (table.comp_yes_and_zero_cc(n16))
                            first = next_first;
                        continue;
                    }
                }
            }

            if (prev_boundary != prev_first &&
                !table.norm16_comp_boundary_before(norm16)) {
                CharIter it = prev_first;
                norm16 = table.trie.fast_u8_prev(prev_boundary, it);
                if (!table.norm16_comp_boundary_after(norm16, OnlyContiguous))
                    prev_first = it;
            }
            bool equals_utf8 = true;
            {
                s16.clear();
                utf16_appender<container::small_vector<uint16_t, 1024>>
                    buffer_appender(s16);
                reordering_appender<
                    utf16_appender<container::small_vector<uint16_t, 1024>>>
                    buffer(table, buffer_appender);
                auto const no_flush = buffer.inhibit_flush();
                detail::decompose_short_utf8(
                    table, prev_first, first, false, OnlyContiguous, buffer);
                first = detail::decompose_short_utf8(
                    table, first, last, true, OnlyContiguous, buffer);
                BOOST_ASSERT(first - prev_first <= INT32_MAX);
                detail::recompose(table, buffer, 0, OnlyContiguous);
                equals_utf8 = buffer.equals_utf8(prev_first, first);
            }
            if (!equals_utf8) {
                if (!WriteToOut)
                    return false;
                if (prev_boundary != prev_first)
                    appender.append(prev_boundary, prev_first);
                detail::append_utf16(
                    s16.data(), s16.data() + s16.size(), appender);
                prev_boundary = first;
            }
        }
        return true;
    }

}}}

#endif
