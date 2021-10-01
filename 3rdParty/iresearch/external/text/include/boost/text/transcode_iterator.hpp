// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_TRANSCODE_ITERATOR_HPP
#define BOOST_TEXT_TRANSCODE_ITERATOR_HPP

#include <boost/text/config.hpp>
#include <boost/text/concepts.hpp>
#include <boost/text/utf.hpp>
#include <boost/text/detail/algorithm.hpp>

#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <array>
#include <iterator>
#include <type_traits>
#include <stdexcept>


namespace boost { namespace text {

    namespace {
        constexpr uint16_t high_surrogate_base = 0xd7c0;
        constexpr uint16_t low_surrogate_base = 0xdc00;
        constexpr uint32_t high_surrogate_min = 0xd800;
        constexpr uint32_t high_surrogate_max = 0xdbff;
        constexpr uint32_t low_surrogate_min = 0xdc00;
        constexpr uint32_t low_surrogate_max = 0xdfff;
    }

    namespace detail {
        constexpr bool
        in(unsigned char lo, unsigned char c, unsigned char hi) noexcept
        {
            return lo <= c && c <= hi;
        }

        struct throw_on_encoding_error
        {};

        template<typename OutIter>
        inline BOOST_TEXT_CXX14_CONSTEXPR OutIter
        read_into_buf(uint32_t cp, OutIter buf)
        {
            if (cp < 0x80) {
                *buf = static_cast<char>(cp);
                ++buf;
            } else if (cp < 0x800) {
                *buf = static_cast<char>(0xC0 + (cp >> 6));
                ++buf;
                *buf = static_cast<char>(0x80 + (cp & 0x3f));
                ++buf;
            } else if (cp < 0x10000) {
                *buf = static_cast<char>(0xe0 + (cp >> 12));
                ++buf;
                *buf = static_cast<char>(0x80 + ((cp >> 6) & 0x3f));
                ++buf;
                *buf = static_cast<char>(0x80 + (cp & 0x3f));
                ++buf;
            } else {
                *buf = static_cast<char>(0xf0 + (cp >> 18));
                ++buf;
                *buf = static_cast<char>(0x80 + ((cp >> 12) & 0x3f));
                ++buf;
                *buf = static_cast<char>(0x80 + ((cp >> 6) & 0x3f));
                ++buf;
                *buf = static_cast<char>(0x80 + (cp & 0x3f));
                ++buf;
            }
            return buf;
        }

        template<typename OutIter>
        BOOST_TEXT_CXX14_CONSTEXPR OutIter
        write_cp_utf8(uint32_t cp, OutIter out)
        {
            return detail::read_into_buf(cp, out);
        }

        template<typename OutIter>
        BOOST_TEXT_CXX14_CONSTEXPR OutIter
        write_cp_utf16(uint32_t cp, OutIter out)
        {
            if (cp < 0x10000) {
                *out = static_cast<uint16_t>(cp);
                ++out;
            } else {
                *out = static_cast<uint16_t>(cp >> 10) + high_surrogate_base;
                ++out;
                *out = static_cast<uint16_t>(cp & 0x3ff) + low_surrogate_base;
                ++out;
            }
            return out;
        }

        inline constexpr uint32_t
        surrogates_to_cp(uint16_t hi, uint16_t lo) noexcept
        {
            return uint32_t((hi - high_surrogate_base) << 10) +
                   (lo - low_surrogate_base);
        }

        template<typename T, typename U>
        struct enable_utf8_cp : std::enable_if<is_char_iter<T>::value, U>
        {};
        template<typename T, typename U = T>
        using enable_utf8_cp_t = typename enable_utf8_cp<T, U>::type;

        template<typename T, typename U>
        struct enable_utf16_cp : std::enable_if<is_16_iter<T>::value, U>
        {};
        template<typename T, typename U = T>
        using enable_utf16_cp_t = typename enable_utf16_cp<T, U>::type;
    }

    /** The replacement character used to mark invalid portions of a Unicode
        sequence when converting between two encodings.

        \see Unicode 3.2/C10 */
    constexpr uint32_t replacement_character() noexcept { return 0xfffd; }

    /** Returns true iff `c` is a Unicode surrogate. */
    inline BOOST_TEXT_CXX14_CONSTEXPR bool surrogate(uint32_t c) noexcept
    {
        return high_surrogate_min <= c && c <= low_surrogate_max;
    }

    /** Returns true iff `c` is a Unicode high surrogate. */
    inline BOOST_TEXT_CXX14_CONSTEXPR bool high_surrogate(uint32_t c) noexcept
    {
        return high_surrogate_min <= c && c <= high_surrogate_max;
    }

    /** Returns true iff `c` is a Unicode low surrogate. */
    inline BOOST_TEXT_CXX14_CONSTEXPR bool low_surrogate(uint32_t c) noexcept
    {
        return low_surrogate_min <= c && c <= low_surrogate_max;
    }

    /** Returns true iff `c` is a Unicode reserved noncharacter.

        \see Unicode 3.4/D14 */
    inline BOOST_TEXT_CXX14_CONSTEXPR bool
    reserved_noncharacter(uint32_t c) noexcept
    {
        bool const byte01_reserved = (c & 0xffff) >= 0xfffe;
        bool const byte2_at_most_0x10 = ((c & 0xff0000u) >> 16) <= 0x10;
        return (byte01_reserved && byte2_at_most_0x10) ||
               (0xfdd0 <= c && c <= 0xfdef);
    }

    /** Returns true iff `c` is a valid Unicode code point.

        \see Unicode 3.9/D90 */
    inline BOOST_TEXT_CXX14_CONSTEXPR bool valid_code_point(uint32_t c) noexcept
    {
        return c <= 0x10ffff && !surrogate(c) && !reserved_noncharacter(c);
    }

    /** Returns true iff `c` is a UTF-8 lead code unit (which must be followed
        by 1-3 following units). */
    constexpr bool lead_code_unit(unsigned char c) noexcept
    {
        return uint8_t(c - 0xc2) <= 0x32;
    }

    /** Returns true iff `c` is a UTF-8 continuation code unit. */
    constexpr bool continuation(unsigned char c) noexcept
    {
        return (int8_t)c < -0x40;
    }

    /** Given the first (and possibly only) code unit of a UTF-8-encoded code
        point, returns the number of bytes occupied by that code point (in the
        range `[1, 4]`).  Returns a value < 0 if `first_unit` is not a valid
        initial UTF-8 code unit. */
    inline BOOST_TEXT_CXX14_CONSTEXPR int
    utf8_code_units(unsigned char first_unit) noexcept
    {
        return first_unit <= 0x7f
                   ? 1
                   : boost::text::lead_code_unit(first_unit)
                         ? int(0xe0 <= first_unit) + int(0xf0 <= first_unit) + 2
                         : -1;
    }

    /** Given the first (and possibly only) code unit of a UTF-16-encoded code
        point, returns the number of code units occupied by that code point
        (in the range `[1, 2]`).  Returns a negative value if `first_unit` is
        not a valid initial UTF-16 code unit. */
    inline BOOST_TEXT_CXX14_CONSTEXPR int
    utf16_code_units(uint16_t first_unit) noexcept
    {
        if (boost::text::low_surrogate(first_unit))
            return -1;
        if (boost::text::high_surrogate(first_unit))
            return 2;
        return 1;
    }

    namespace detail {
        // optional is not constexpr friendly.
        template<typename Iter>
        struct optional_iter
        {
            constexpr optional_iter() : it_(), valid_(false) {}
            constexpr optional_iter(Iter it) : it_(it), valid_(true) {}

            BOOST_TEXT_CXX14_CONSTEXPR operator bool() const noexcept
            {
                return valid_;
            }
            BOOST_TEXT_CXX14_CONSTEXPR Iter operator*() const noexcept
            {
                BOOST_ASSERT(valid_);
                return it_;
            }
            Iter & operator*() noexcept
            {
                BOOST_ASSERT(valid_);
                return it_;
            }

            friend BOOST_TEXT_CXX14_CONSTEXPR bool
            operator==(optional_iter lhs, optional_iter rhs) noexcept
            {
                return lhs.valid_ == rhs.valid_ &&
                       (!lhs.valid_ || lhs.it_ == rhs.it_);
            }
            friend BOOST_TEXT_CXX14_CONSTEXPR bool
            operator!=(optional_iter lhs, optional_iter rhs) noexcept
            {
                return !(lhs == rhs);
            }

        private:
            Iter it_;
            bool valid_;
        };

        // Follow Table 3-7 in Unicode 3.9/D92
        template<typename Iter>
        BOOST_TEXT_CXX14_CONSTEXPR optional_iter<Iter>
        end_of_invalid_utf8(Iter it) noexcept
        {
            BOOST_ASSERT(!boost::text::continuation(*it));

            if (detail::in(0, *it, 0x7f))
                return optional_iter<Iter>{};

            if (detail::in(0xc2, *it, 0xdf)) {
                auto next = it;
                if (!boost::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }

            if (detail::in(0xe0, *it, 0xe0)) {
                auto next = it;
                if (!detail::in(0xa0, *++next, 0xbf))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xe1, *it, 0xec)) {
                auto next = it;
                if (!boost::text::continuation(*++next))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xed, *it, 0xed)) {
                auto next = it;
                if (!detail::in(0x80, *++next, 0x9f))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xee, *it, 0xef)) {
                auto next = it;
                if (!boost::text::continuation(*++next))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }

            if (detail::in(0xf0, *it, 0xf0)) {
                auto next = it;
                if (!detail::in(0x90, *++next, 0xbf))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xf1, *it, 0xf3)) {
                auto next = it;
                if (!boost::text::continuation(*++next))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xf4, *it, 0xf4)) {
                auto next = it;
                if (!detail::in(0x80, *++next, 0x8f))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                if (!boost::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }

            return it;
        }

        template<typename Iter>
        BOOST_TEXT_CXX14_CONSTEXPR Iter decrement(Iter it) noexcept
        {
            Iter retval = it;

            int backup = 0;
            while (backup < 4 && boost::text::continuation(*--retval)) {
                ++backup;
            }
            backup = it - retval;

            if (boost::text::continuation(*retval))
                return it - 1;

            optional_iter<Iter> first_invalid = end_of_invalid_utf8(retval);
            if (first_invalid == retval)
                ++*first_invalid;
            while (first_invalid && (*first_invalid - retval) < backup) {
                backup -= *first_invalid - retval;
                retval = *first_invalid;
                first_invalid = end_of_invalid_utf8(retval);
                if (first_invalid == retval)
                    ++*first_invalid;
            }

            if (1 < backup) {
                int const cp_bytes = boost::text::utf8_code_units(*retval);
                if (cp_bytes < backup)
                    retval = it - 1;
            }

            return retval;
        }

        template<typename Iter>
        BOOST_TEXT_CXX14_CONSTEXPR Iter decrement(Iter first, Iter it) noexcept
        {
            Iter retval = it;

            int backup = 0;
            while (backup < 4 && it != first &&
                   boost::text::continuation(*--retval)) {
                ++backup;
            }
            backup = std::distance(retval, it);

            if (boost::text::continuation(*retval)) {
                if (it != first)
                    --it;
                return it;
            }

            optional_iter<Iter> first_invalid = end_of_invalid_utf8(retval);
            if (first_invalid == retval)
                ++*first_invalid;
            while (first_invalid &&
                   std::distance(retval, *first_invalid) < backup) {
                backup -= std::distance(retval, *first_invalid);
                retval = *first_invalid;
                first_invalid = end_of_invalid_utf8(retval);
                if (first_invalid == retval)
                    ++*first_invalid;
            }

            if (1 < backup) {
                int const cp_bytes = boost::text::utf8_code_units(*retval);
                if (cp_bytes < backup) {
                    if (it != first)
                        --it;
                    retval = it;
                }
            }

            return retval;
        }

        enum char_class : uint8_t {
            ill = 0,
            asc = 1,
            cr1 = 2,
            cr2 = 3,
            cr3 = 4,
            l2a = 5,
            l3a = 6,
            l3b = 7,
            l3c = 8,
            l4a = 9,
            l4b = 10,
            l4c = 11,
        };

        enum table_state : uint8_t {
            bgn = 0,
            end = bgn,
            err = 12,
            cs1 = 24,
            cs2 = 36,
            cs3 = 48,
            p3a = 60,
            p3b = 72,
            p4a = 84,
            p4b = 96,
            invalid_table_state = 200
        };

        struct first_cu
        {
            unsigned char initial_octet;
            table_state next;
        };

        namespace {
            constexpr first_cu first_cus[256] = {
                {0x00, bgn}, {0x01, bgn}, {0x02, bgn}, {0x03, bgn}, {0x04, bgn},
                {0x05, bgn}, {0x06, bgn}, {0x07, bgn}, {0x08, bgn}, {0x09, bgn},
                {0x0a, bgn}, {0x0b, bgn}, {0x0c, bgn}, {0x0d, bgn}, {0x0e, bgn},
                {0x0f, bgn}, {0x10, bgn}, {0x11, bgn}, {0x12, bgn}, {0x13, bgn},
                {0x14, bgn}, {0x15, bgn}, {0x16, bgn}, {0x17, bgn}, {0x18, bgn},
                {0x19, bgn}, {0x1a, bgn}, {0x1b, bgn}, {0x1c, bgn}, {0x1d, bgn},
                {0x1e, bgn}, {0x1f, bgn}, {0x20, bgn}, {0x21, bgn}, {0x22, bgn},
                {0x23, bgn}, {0x24, bgn}, {0x25, bgn}, {0x26, bgn}, {0x27, bgn},
                {0x28, bgn}, {0x29, bgn}, {0x2a, bgn}, {0x2b, bgn}, {0x2c, bgn},
                {0x2d, bgn}, {0x2e, bgn}, {0x2f, bgn}, {0x30, bgn}, {0x31, bgn},
                {0x32, bgn}, {0x33, bgn}, {0x34, bgn}, {0x35, bgn}, {0x36, bgn},
                {0x37, bgn}, {0x38, bgn}, {0x39, bgn}, {0x3a, bgn}, {0x3b, bgn},
                {0x3c, bgn}, {0x3d, bgn}, {0x3e, bgn}, {0x3f, bgn}, {0x40, bgn},
                {0x41, bgn}, {0x42, bgn}, {0x43, bgn}, {0x44, bgn}, {0x45, bgn},
                {0x46, bgn}, {0x47, bgn}, {0x48, bgn}, {0x49, bgn}, {0x4a, bgn},
                {0x4b, bgn}, {0x4c, bgn}, {0x4d, bgn}, {0x4e, bgn}, {0x4f, bgn},
                {0x50, bgn}, {0x51, bgn}, {0x52, bgn}, {0x53, bgn}, {0x54, bgn},
                {0x55, bgn}, {0x56, bgn}, {0x57, bgn}, {0x58, bgn}, {0x59, bgn},
                {0x5a, bgn}, {0x5b, bgn}, {0x5c, bgn}, {0x5d, bgn}, {0x5e, bgn},
                {0x5f, bgn}, {0x60, bgn}, {0x61, bgn}, {0x62, bgn}, {0x63, bgn},
                {0x64, bgn}, {0x65, bgn}, {0x66, bgn}, {0x67, bgn}, {0x68, bgn},
                {0x69, bgn}, {0x6a, bgn}, {0x6b, bgn}, {0x6c, bgn}, {0x6d, bgn},
                {0x6e, bgn}, {0x6f, bgn}, {0x70, bgn}, {0x71, bgn}, {0x72, bgn},
                {0x73, bgn}, {0x74, bgn}, {0x75, bgn}, {0x76, bgn}, {0x77, bgn},
                {0x78, bgn}, {0x79, bgn}, {0x7a, bgn}, {0x7b, bgn}, {0x7c, bgn},
                {0x7d, bgn}, {0x7e, bgn}, {0x7f, bgn}, {0x00, err}, {0x01, err},
                {0x02, err}, {0x03, err}, {0x04, err}, {0x05, err}, {0x06, err},
                {0x07, err}, {0x08, err}, {0x09, err}, {0x0a, err}, {0x0b, err},
                {0x0c, err}, {0x0d, err}, {0x0e, err}, {0x0f, err}, {0x10, err},
                {0x11, err}, {0x12, err}, {0x13, err}, {0x14, err}, {0x15, err},
                {0x16, err}, {0x17, err}, {0x18, err}, {0x19, err}, {0x1a, err},
                {0x1b, err}, {0x1c, err}, {0x1d, err}, {0x1e, err}, {0x1f, err},
                {0x20, err}, {0x21, err}, {0x22, err}, {0x23, err}, {0x24, err},
                {0x25, err}, {0x26, err}, {0x27, err}, {0x28, err}, {0x29, err},
                {0x2a, err}, {0x2b, err}, {0x2c, err}, {0x2d, err}, {0x2e, err},
                {0x2f, err}, {0x30, err}, {0x31, err}, {0x32, err}, {0x33, err},
                {0x34, err}, {0x35, err}, {0x36, err}, {0x37, err}, {0x38, err},
                {0x39, err}, {0x3a, err}, {0x3b, err}, {0x3c, err}, {0x3d, err},
                {0x3e, err}, {0x3f, err}, {0xc0, err}, {0xc1, err}, {0x02, cs1},
                {0x03, cs1}, {0x04, cs1}, {0x05, cs1}, {0x06, cs1}, {0x07, cs1},
                {0x08, cs1}, {0x09, cs1}, {0x0a, cs1}, {0x0b, cs1}, {0x0c, cs1},
                {0x0d, cs1}, {0x0e, cs1}, {0x0f, cs1}, {0x10, cs1}, {0x11, cs1},
                {0x12, cs1}, {0x13, cs1}, {0x14, cs1}, {0x15, cs1}, {0x16, cs1},
                {0x17, cs1}, {0x18, cs1}, {0x19, cs1}, {0x1a, cs1}, {0x1b, cs1},
                {0x1c, cs1}, {0x1d, cs1}, {0x1e, cs1}, {0x1f, cs1}, {0x00, p3a},
                {0x01, cs2}, {0x02, cs2}, {0x03, cs2}, {0x04, cs2}, {0x05, cs2},
                {0x06, cs2}, {0x07, cs2}, {0x08, cs2}, {0x09, cs2}, {0x0a, cs2},
                {0x0b, cs2}, {0x0c, cs2}, {0x0d, p3b}, {0x0e, cs2}, {0x0f, cs2},
                {0x00, p4a}, {0x01, cs3}, {0x02, cs3}, {0x03, cs3}, {0x04, p4b},
                {0xf5, err}, {0xf6, err}, {0xf7, err}, {0xf8, err}, {0xf9, err},
                {0xfa, err}, {0xfb, err}, {0xfc, err}, {0xfd, err}, {0xfe, err},
                {0xff, err},
            };

            constexpr char_class octet_classes[256] = {
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, cr1, cr1,
                cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1,
                cr1, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2,
                cr2, cr2, cr2, cr2, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3,
                cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3,
                cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, ill, ill, l2a,
                l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a,
                l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a,
                l2a, l2a, l2a, l3a, l3b, l3b, l3b, l3b, l3b, l3b, l3b, l3b, l3b,
                l3b, l3b, l3b, l3c, l3b, l3b, l4a, l4b, l4b, l4b, l4c, ill, ill,
                ill, ill, ill, ill, ill, ill, ill, ill, ill,
            };

            constexpr table_state transitions[108] = {
                err, end, err, err, err, cs1, p3a, cs2, p3b, p4a, cs3, p4b,
                err, err, err, err, err, err, err, err, err, err, err, err,
                err, err, end, end, end, err, err, err, err, err, err, err,
                err, err, cs1, cs1, cs1, err, err, err, err, err, err, err,
                err, err, cs2, cs2, cs2, err, err, err, err, err, err, err,
                err, err, err, err, cs1, err, err, err, err, err, err, err,
                err, err, cs1, cs1, err, err, err, err, err, err, err, err,
                err, err, err, cs2, cs2, err, err, err, err, err, err, err,
                err, err, cs2, err, err, err, err, err, err, err, err, err,
            };
        }

        template<typename InputIter, typename Sentinel>
        uint32_t advance(InputIter & first, Sentinel last)
        {
            uint32_t retval = 0;

            first_cu const info = first_cus[(unsigned char)*first];
            ++first;

            retval = info.initial_octet;
            int state = info.next;

            while (state != bgn) {
                if (first != last) {
                    unsigned char const cu = *first;
                    retval = (retval << 6) | (cu & 0x3f);
                    char_class const class_ = octet_classes[cu];
                    state = transitions[state + class_];
                    if (state == err)
                        return replacement_character();
                    ++first;
                } else {
                    return replacement_character();
                }
            }

            return retval;
        }

        template<typename Derived, typename Iter>
        struct trans_ins_iter
        {
            using value_type = void;
            using difference_type =
#if BOOST_TEXT_USE_CONCEPTS
                std::ptrdiff_t;
#else
                void;
#endif
            using pointer = void;
            using reference = void;
            using iterator_category = std::output_iterator_tag;

            trans_ins_iter() {}
            trans_ins_iter(Iter it) : it_(it) {}
            Derived & operator*() noexcept { return derived(); }
            Derived & operator++() noexcept { return derived(); }
            Derived operator++(int)noexcept { return derived(); }
            Iter base() const { return it_; }

        protected:
            Iter & iter() { return it_; }

        private:
            Derived & derived() { return static_cast<Derived &>(*this); }
            Iter it_;
        };

        template<typename Derived, typename ValueType>
        using trans_iter = stl_interfaces::iterator_interface<
            Derived,
            std::bidirectional_iterator_tag,
            ValueType,
            ValueType>;
    }

}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    /** Returns the first code unit in `[first, last)` that is not properly
        UTF-8 encoded, or `last` if no such code unit is found. */
    template<typename Iter>
    BOOST_TEXT_CXX14_CONSTEXPR detail::enable_utf8_cp_t<Iter>
    find_invalid_encoding(Iter first, Iter last) noexcept
    {
        while (first != last) {
            int const cp_bytes = boost::text::utf8_code_units(*first);
            if (cp_bytes == -1 || last - first < cp_bytes)
                return first;

            if (detail::end_of_invalid_utf8(first))
                return first;

            first += cp_bytes;
        }

        return last;
    }

    /** Returns the first code unit in `[first, last)` that is not properly
        UTF-16 encoded, or `last` if no such code unit is found. */
    template<typename Iter>
    BOOST_TEXT_CXX14_CONSTEXPR detail::enable_utf16_cp_t<Iter>
    find_invalid_encoding(Iter first, Iter last) noexcept
    {
        while (first != last) {
            int const cp_units = boost::text::utf16_code_units(*first);
            if (cp_units == -1 || last - first < cp_units)
                return first;

            if (cp_units == 2 && !boost::text::low_surrogate(*(first + 1)))
                return first;

            first += cp_units;
        }

        return last;
    }

    /** Returns true iff `[first, last)` is properly UTF-8 encoded. */
    template<typename Iter>
    BOOST_TEXT_CXX14_CONSTEXPR detail::enable_utf8_cp_t<Iter, bool> encoded(
        Iter first, Iter last) noexcept
    {
        return v1::find_invalid_encoding(first, last) == last;
    }

    /** Returns true iff `[first, last)` is properly UTF-16 encoded. */
    template<typename Iter>
    BOOST_TEXT_CXX14_CONSTEXPR detail::enable_utf16_cp_t<Iter, bool> encoded(
        Iter first, Iter last) noexcept
    {
        return v1::find_invalid_encoding(first, last) == last;
    }

    /** Returns true iff `[first, last)` is empty or the initial UTF-8 code units
       in `[first, last)` form a valid Unicode code point. */
    template<typename Iter>
    BOOST_TEXT_CXX14_CONSTEXPR detail::enable_utf8_cp_t<Iter, bool>
    starts_encoded(Iter first, Iter last) noexcept
    {
        if (first == last)
            return true;

        int const cp_bytes = boost::text::utf8_code_units(*first);
        if (cp_bytes == -1 || last - first < cp_bytes)
            return false;

        return !detail::end_of_invalid_utf8(first);
    }

    /** Returns true iff `[first, last)` is empty or the initial UTF-16 code
        units in `[first, last)` form a valid Unicode code point. */
    template<typename Iter>
    BOOST_TEXT_CXX14_CONSTEXPR detail::enable_utf16_cp_t<Iter, bool>
    starts_encoded(Iter first, Iter last) noexcept
    {
        if (first == last)
            return true;

        int const cp_units = boost::text::utf16_code_units(*first);
        if (cp_units == -1 || last - first < cp_units)
            return false;

        return cp_units == 1 || boost::text::low_surrogate(*(first + 1));
    }

    /** Returns true iff `[first, last)` is empty or the final UTF-8 code units
        in `[first, last)` form a valid Unicode code point. */
    template<typename Iter>
    BOOST_TEXT_CXX14_CONSTEXPR detail::enable_utf8_cp_t<Iter, bool>
    ends_encoded(Iter first, Iter last) noexcept
    {
        if (first == last)
            return true;

        auto it = last;
        while (first != --it && boost::text::continuation(*it))
            ;

        return v1::starts_encoded(it, last);
    }

    /** Returns true iff `[first, last)` is empty or the final UTF-16 code units
        in `[first, last)` form a valid Unicode code point. */
    template<typename Iter>
    BOOST_TEXT_CXX14_CONSTEXPR detail::enable_utf16_cp_t<Iter, bool>
    ends_encoded(Iter first, Iter last) noexcept
    {
        if (first == last)
            return true;

        auto it = last;
        if (boost::text::low_surrogate(*--it))
            --it;

        return v1::starts_encoded(it, last);
    }

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    /** Returns the first code unit in `[first, last)` that is not properly
        UTF-8 encoded, or `last` if no such code unit is found. */
    template<utf8_iter I>
    // clang-format off
        requires std::random_access_iterator<I>
    constexpr I find_invalid_encoding(I first, I last) noexcept
    // clang-format on
    {
        while (first != last) {
            int const cp_bytes = boost::text::utf8_code_units(*first);
            if (cp_bytes == -1 || last - first < cp_bytes)
                return first;

            if (detail::end_of_invalid_utf8(first))
                return first;

            first += cp_bytes;
        }

        return last;
    }

    /** Returns the first code unit in `[first, last)` that is not properly
        UTF-16 encoded, or `last` if no such code unit is found. */
    template<utf16_iter I>
    // clang-format off
        requires std::random_access_iterator<I>
    constexpr I find_invalid_encoding(I first, I last) noexcept
    // clang-format on
    {
        while (first != last) {
            int const cp_units = boost::text::utf16_code_units(*first);
            if (cp_units == -1 || last - first < cp_units)
                return first;

            if (cp_units == 2 && !boost::text::low_surrogate(*(first + 1)))
                return first;

            first += cp_units;
        }

        return last;
    }

    /** Returns true iff `[first, last)` is properly UTF-8 encoded. */
    template<utf8_iter I>
    // clang-format off
        requires std::random_access_iterator<I>
    constexpr bool encoded(I first, I last) noexcept
    // clang-format on
    {
        return boost::text::find_invalid_encoding(first, last) == last;
    }

    /** Returns true iff `[first, last)` is properly UTF-16 encoded */
    template<utf16_iter I>
    // clang-format off
        requires std::random_access_iterator<I>
    constexpr bool encoded(I first, I last) noexcept
    // clang-format on
    {
        return boost::text::find_invalid_encoding(first, last) == last;
    }

    /** Returns true iff `[first, last)` is empty or the initial UTF-8 code
        units in `[first, last)` form a valid Unicode code point. */
    template<utf8_iter I>
    // clang-format off
        requires std::random_access_iterator<I>
    constexpr bool starts_encoded(I first, I last) noexcept
    // clang-format on
    {
        if (first == last)
            return true;

        int const cp_bytes = boost::text::utf8_code_units(*first);
        if (cp_bytes == -1 || last - first < cp_bytes)
            return false;

        return !detail::end_of_invalid_utf8(first);
    }

    /** Returns true iff `[first, last)` is empty or the initial UTF-16 code
        units in `[first, last)` form a valid Unicode code point. */
    template<utf16_iter I>
    // clang-format off
        requires std::random_access_iterator<I>
    constexpr bool starts_encoded(I first, I last) noexcept
    // clang-format on
    {
        if (first == last)
            return true;

        int const cp_units = boost::text::utf16_code_units(*first);
        if (cp_units == -1 || last - first < cp_units)
            return false;

        return cp_units == 1 || boost::text::low_surrogate(*(first + 1));
    }

    /** Returns true iff `[first, last)` is empty or the final UTF-8 code units
        in `[first, last)` form a valid Unicode code point. */
    template<utf8_iter I>
    // clang-format off
        requires std::random_access_iterator<I>
    constexpr bool ends_encoded(I first, I last) noexcept
    // clang-format on
    {
        if (first == last)
            return true;

        auto it = last;
        while (first != --it && boost::text::continuation(*it))
            ;

        return boost::text::starts_encoded(it, last);
    }

    /** Returns true iff `[first, last)` is empty or the final UTF-16 code units
        in `[first, last)` form a valid Unicode code point. */
    template<utf16_iter I>
    // clang-format off
        requires std::random_access_iterator<I>
    constexpr bool ends_encoded(I first, I last) noexcept
    // clang-format on
    {
        if (first == last)
            return true;

        auto it = last;
        if (boost::text::low_surrogate(*--it))
            --it;

        return boost::text::starts_encoded(it, last);
    }

}}}

#endif

namespace boost { namespace text {

    /** An error handler type that can be used with the converting iterators;
        provides the Unicode replacement character on errors. */
    struct use_replacement_character
    {
        constexpr uint32_t operator()(char const *) const noexcept
        {
            return replacement_character();
        }
    };

    /** An error handler type that can be used with the converting iterators;
        throws `std::logic_error` on errors. */
    struct throw_logic_error
    {
        uint32_t operator()(char const * msg) const
        {
            boost::throw_exception(std::logic_error(msg));
            return 0;
        }
    };


    /** A sentinel type that compares equal to a pointer to a 1-, 2-, or
        4-byte integral value, iff the pointer is null. */
    struct null_sentinel
    {
        constexpr null_sentinel base() const noexcept
        {
            return null_sentinel{};
        }
    };

#if defined(BOOST_TEXT_DOXYGEN)

    /** Only participates in overload resolution when `T` is an integral type,
        and `sizeof(T)` is 1, 2, or 4.*/
    template<typename T>
    constexpr bool operator==(T * p, null_sentinel);

    /** Only participates in overload resolution when `T` is an integral type,
        and `sizeof(T)` is 1, 2, or 4.*/
    template<typename T>
    constexpr bool operator!=(T * p, null_sentinel);

    /** Only participates in overload resolution when `T` is an integral type,
        and `sizeof(T)` is 1, 2, or 4.*/
    template<typename T>
    constexpr bool operator==(null_sentinel, T * p);

    /** Only participates in overload resolution when `T` is an integral type,
        and `sizeof(T)` is 1, 2, or 4.*/
    template<typename T>
    constexpr bool operator!=(null_sentinel, T * p);

#else

#if BOOST_TEXT_USE_CONCEPTS

    template<typename T>
        // clang-format off
        requires utf8_code_unit<T> || utf16_code_unit<T> || utf32_code_unit<T>
    constexpr auto operator==(T * p, null_sentinel)
    // clang-format on
    {
        return *p == 0;
    }
#if 1 // TODO: This should not be necessary, one better support for op==
      // rewriting is widely supported.
    template<typename T>
        // clang-format off
        requires utf8_code_unit<T> || utf16_code_unit<T> || utf32_code_unit<T>
    constexpr auto operator!=(T * p, null_sentinel)
    // clang-format on
    {
        return *p != 0;
    }
    template<typename T>
        // clang-format off
        requires utf8_code_unit<T> || utf16_code_unit<T> || utf32_code_unit<T>
    constexpr auto operator==(null_sentinel, T * p)
    // clang-format on
    {
        return *p == 0;
    }
    template<typename T>
        // clang-format off
        requires utf8_code_unit<T> || utf16_code_unit<T> || utf32_code_unit<T>
    constexpr auto operator!=(null_sentinel, T * p)
    // clang-format on
    {
        return *p != 0;
    }
#endif

#else

    namespace detail {
        template<
            typename Iter,
            bool UTF8 = char_ptr<Iter>::value || _16_ptr<Iter>::value ||
                        cp_ptr<Iter>::value>
        struct null_sent_eq_dispatch
        {};

        template<typename Ptr>
        struct null_sent_eq_dispatch<Ptr, true>
        {
            static constexpr bool call(Ptr p) noexcept { return *p == 0; }
        };
    }

    template<typename T>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(T * p, null_sentinel)
        ->decltype(detail::null_sent_eq_dispatch<T *>::call(p))
    {
        return detail::null_sent_eq_dispatch<T *>::call(p);
    }
    template<typename T>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(T * p, null_sentinel)
        ->decltype(detail::null_sent_eq_dispatch<T *>::call(p))
    {
        return !detail::null_sent_eq_dispatch<T *>::call(p);
    }
    template<typename T>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(null_sentinel, T * p)
        ->decltype(detail::null_sent_eq_dispatch<T *>::call(p))
    {
        return detail::null_sent_eq_dispatch<T *>::call(p);
    }
    template<typename T>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(null_sentinel, T * p)
        ->decltype(detail::null_sent_eq_dispatch<T *>::call(p))
    {
        return !detail::null_sent_eq_dispatch<T *>::call(p);
    }

#endif

#endif


    /** A UTF-8 to UTF-16 converting iterator. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<
        utf8_iter I,
        std::sentinel_for<I> S = I,
        transcoding_error_handler ErrorHandler = use_replacement_character>
#else
    template<
        typename I,
        typename Sentinel = I,
        typename ErrorHandler = use_replacement_character>
#endif
    struct utf_8_to_16_iterator;


    /** A UTF-32 to UTF-8 converting iterator. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<
        utf32_iter I,
        std::sentinel_for<I> S = I,
        transcoding_error_handler ErrorHandler = use_replacement_character>
#else
    template<
        typename I,
        typename S = I,
        typename ErrorHandler = use_replacement_character>
#endif
    struct utf_32_to_8_iterator
        : detail::trans_iter<utf_32_to_8_iterator<I, S, ErrorHandler>, char>
    {
        static bool const throw_on_error =
            !noexcept(std::declval<ErrorHandler>()(0));

#if !BOOST_TEXT_USE_CONCEPTS
        static_assert(
            std::is_same<
                typename std::iterator_traits<I>::iterator_category,
                std::bidirectional_iterator_tag>::value ||
                std::is_same<
                    typename std::iterator_traits<I>::iterator_category,
                    std::random_access_iterator_tag>::value,
            "utf_32_to_8_iterator requires its I parameter to be at least "
            "bidirectional.");
        static_assert(
            sizeof(typename std::iterator_traits<I>::value_type) == 4,
            "utf_32_to_8_iterator requires its I parameter to produce a "
            "4-byte value_type.");
#endif

        constexpr utf_32_to_8_iterator() noexcept :
            first_(), it_(), last_(), index_(4), buf_()
        {}
        explicit BOOST_TEXT_CXX14_CONSTEXPR
        utf_32_to_8_iterator(I first, I it, S last) noexcept :
            first_(first), it_(it), last_(last), index_(0), buf_()
        {
            if (it_ != last_)
                read_into_buf();
        }
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<typename I2, typename S2>
        // clang-format off
        requires std::convertible_to<I2, I> && std::convertible_to<S2, S>
#else
        template<
            typename I2,
            typename S2,
            typename Enable = std::enable_if_t<
                std::is_convertible<I2, I>::value &&
                std::is_convertible<S2, S>::value>>
#endif
        constexpr utf_32_to_8_iterator(
            utf_32_to_8_iterator<I2, S2, ErrorHandler> const & other) noexcept :
            // clang-format on
            first_(other.first_),
            it_(other.it_),
            last_(other.last_),
            index_(other.index_),
            buf_(other.buf_)
        {}

        BOOST_TEXT_CXX14_CONSTEXPR char operator*() const
            noexcept(!throw_on_error)
        {
            return buf_[index_];
        }

        BOOST_TEXT_CXX14_CONSTEXPR I base() const noexcept { return it_; }

        BOOST_TEXT_CXX14_CONSTEXPR utf_32_to_8_iterator &
        operator++() noexcept(!throw_on_error)
        {
            ++index_;
            if (at_buf_end()) {
                BOOST_ASSERT(it_ != last_);
                ++it_;
                index_ = 0;
                if (it_ != last_)
                    read_into_buf();
            }
            return *this;
        }

        BOOST_TEXT_CXX14_CONSTEXPR utf_32_to_8_iterator &
        operator--() noexcept(!throw_on_error)
        {
            if (0 < index_) {
                --index_;
            } else {
                BOOST_ASSERT(it_ != first_);
                --it_;
                auto out = read_into_buf();
                index_ = out - buf_.data() - 1;
            }
            return *this;
        }

        template<
            typename I1,
            typename S1,
            typename I2,
            typename S2,
            typename ErrorHandler2>
        friend BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
            utf_32_to_8_iterator<I1, S1, ErrorHandler2> const & lhs,
            utf_32_to_8_iterator<I2, S2, ErrorHandler2> const & rhs) noexcept
            -> decltype(lhs.base() == rhs.base());

        friend bool
        operator==(utf_32_to_8_iterator lhs, utf_32_to_8_iterator rhs) noexcept
        {
            return lhs.base() == rhs.base() && lhs.index_ == rhs.index_;
        }

        using base_type =
            detail::trans_iter<utf_32_to_8_iterator<I, S, ErrorHandler>, char>;
        using base_type::operator++;
        using base_type::operator--;

#ifndef BOOST_TEXT_DOXYGEN
    private:
        constexpr bool buf_empty() const noexcept { return index_ == 4; }

        constexpr bool at_buf_end() const noexcept
        {
            return buf_[index_] == '\0';
        }

        BOOST_TEXT_CXX14_CONSTEXPR char *
        read_into_buf() noexcept(!throw_on_error)
        {
            uint32_t cp = static_cast<uint32_t>(*it_);
            index_ = 0;
            char * retval = detail::read_into_buf(cp, buf_.data());
            *retval = 0;
            return retval;
        }

        I first_;
        I it_;
        S last_;
        int index_;
        std::array<char, 5> buf_;

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<
            utf32_iter I2,
            std::sentinel_for<I2> S2,
            transcoding_error_handler ErrorHandler2>
#else
        template<typename I2, typename S2, typename ErrorHandler2>
#endif
        friend struct utf_32_to_8_iterator;

#endif
    };

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_32_to_8_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() == rhs)
    {
        return lhs.base() == rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        Sentinel lhs,
        utf_32_to_8_iterator<Iter, Sentinel, ErrorHandler> const & rhs) noexcept
        -> decltype(rhs.base() == lhs)
    {
        return rhs.base() == lhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_32_to_8_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() != rhs)
    {
        return lhs.base() != rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        Sentinel lhs,
        utf_32_to_8_iterator<Iter, Sentinel, ErrorHandler> const & rhs) noexcept
        -> decltype(rhs.base() != lhs)
    {
        return rhs.base() != lhs;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_32_to_8_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_32_to_8_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(lhs.base() == rhs.base())
    {
        return lhs.base() == rhs.base() && rhs.index_ == lhs.index_;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_32_to_8_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_32_to_8_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(!(lhs == rhs))
    {
        return !(lhs == rhs);
    }


    /** An out iterator that converts UTF-32 to UTF-8. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<std::output_iterator<uint8_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_32_to_8_out_iterator
        : detail::trans_ins_iter<utf_32_to_8_out_iterator<Iter>, Iter>
    {
        utf_32_to_8_out_iterator() noexcept {}
        explicit utf_32_to_8_out_iterator(Iter it) noexcept :
            detail::trans_ins_iter<utf_32_to_8_out_iterator<Iter>, Iter>(it)
        {}

        utf_32_to_8_out_iterator & operator=(uint32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf8(cp, out);
            return *this;
        }

        Iter base() const
        {
            return const_cast<utf_32_to_8_out_iterator * const>(this)->iter();
        }
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-32 to UTF-8. */
    template<typename Cont>
    struct utf_32_to_8_insert_iterator : detail::trans_ins_iter<
                                             utf_32_to_8_insert_iterator<Cont>,
                                             std::insert_iterator<Cont>>
    {
        utf_32_to_8_insert_iterator() noexcept {}
        utf_32_to_8_insert_iterator(
            Cont & c, typename Cont::iterator it) noexcept :
            detail::trans_ins_iter<
                utf_32_to_8_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it))
        {}

        utf_32_to_8_insert_iterator & operator=(uint32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf8(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-32 to UTF-8. */
    template<typename Cont>
    struct utf_32_to_8_front_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_8_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        utf_32_to_8_front_insert_iterator() noexcept {}
        explicit utf_32_to_8_front_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_32_to_8_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c))
        {}

        utf_32_to_8_front_insert_iterator & operator=(uint32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf8(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-32 to UTF-8. */
    template<typename Cont>
    struct utf_32_to_8_back_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_8_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        utf_32_to_8_back_insert_iterator() noexcept {}
        explicit utf_32_to_8_back_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_32_to_8_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c))
        {}

        utf_32_to_8_back_insert_iterator & operator=(uint32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf8(cp, out);
            return *this;
        }
    };


    /** A UTF-8 to UTF-32 converting iterator. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<
        utf8_iter I,
        std::sentinel_for<I> S = I,
        transcoding_error_handler ErrorHandler = use_replacement_character>
#else
    template<
        typename I,
        typename S = I,
        typename ErrorHandler = use_replacement_character>
#endif
    struct utf_8_to_32_iterator
        : detail::trans_iter<utf_8_to_32_iterator<I, S, ErrorHandler>, uint32_t>
    {
        static bool const throw_on_error =
            !noexcept(std::declval<ErrorHandler>()(0));

        constexpr utf_8_to_32_iterator() noexcept : first_(), it_(), last_() {}
        explicit constexpr utf_8_to_32_iterator(I first, I it, S last) noexcept
            :
            first_(first), it_(it), last_(last)
        {}
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<typename I2, typename S2>
        // clang-format off
        requires std::convertible_to<I2, I> && std::convertible_to<S2, S>
#else
        template<
            typename I2,
            typename S2,
            typename Enable = std::enable_if_t<
                std::is_convertible<I2, I>::value &&
                std::is_convertible<S2, S>::value>>
#endif
        constexpr utf_8_to_32_iterator(
            utf_8_to_32_iterator<I2, S2, ErrorHandler> const & other) noexcept :
            // clang-format on
            first_(other.first_),
            it_(other.it_),
            last_(other.last_)
        {}

        BOOST_TEXT_CXX14_CONSTEXPR uint32_t operator*() const
            noexcept(!throw_on_error)
        {
            BOOST_ASSERT(!at_end(it_));
            unsigned char curr_c = *it_;
            if (curr_c < 0x80)
                return curr_c;
            return get_value().value_;
        }

        BOOST_TEXT_CXX14_CONSTEXPR I base() const noexcept { return it_; }

        BOOST_TEXT_CXX14_CONSTEXPR utf_8_to_32_iterator &
        operator++() noexcept(!throw_on_error)
        {
            BOOST_ASSERT(it_ != last_);
            it_ = increment();
            return *this;
        }

        BOOST_TEXT_CXX14_CONSTEXPR utf_8_to_32_iterator &
        operator--() noexcept(!throw_on_error)
        {
            BOOST_ASSERT(it_ != first_);
            it_ = detail::decrement(first_, it_);
            return *this;
        }

        friend bool
        operator==(utf_8_to_32_iterator lhs, utf_8_to_32_iterator rhs) noexcept
        {
            return lhs.base() == rhs.base();
        }

        using base_type = detail::
            trans_iter<utf_8_to_32_iterator<I, S, ErrorHandler>, uint32_t>;
        using base_type::operator++;
        using base_type::operator--;

#ifndef BOOST_TEXT_DOXYGEN
    private:
        struct get_value_result
        {
            uint32_t value_;
            I it_;
        };

        BOOST_TEXT_CXX14_CONSTEXPR bool check_continuation(
            unsigned char c,
            unsigned char lo = 0x80,
            unsigned char hi = 0xbf) const noexcept(!throw_on_error)
        {
            if (detail::in(lo, c, hi)) {
                return true;
            } else {
                ErrorHandler{}(
                    "Invalid UTF-8 sequence; an expected continuation "
                    "code unit is missing.");
                return false;
            }
        }

        BOOST_TEXT_CXX14_CONSTEXPR bool at_end(I it) const
            noexcept(!throw_on_error)
        {
            if (it == last_) {
                ErrorHandler{}(
                    "Invalid UTF-8 sequence; expected another code unit "
                    "before the end of string.");
                return true;
            } else {
                return false;
            }
        }

        BOOST_TEXT_CXX14_CONSTEXPR get_value_result get_value() const
            noexcept(!throw_on_error)
        {
            // It turns out that this naive implementation is faster than the
            // table implementation for the converting iterators.
#if 1
            /*
                Unicode 3.9/D92
                Table 3-7. Well-Formed UTF-8 Byte Sequences

                Code Points        First Byte Second Byte Third Byte Fourth Byte
                ===========        ========== =========== ========== ===========
                U+0000..U+007F     00..7F
                U+0080..U+07FF     C2..DF     80..BF
                U+0800..U+0FFF     E0         A0..BF      80..BF
                U+1000..U+CFFF     E1..EC     80..BF      80..BF
                U+D000..U+D7FF     ED         80..9F      80..BF
                U+E000..U+FFFF     EE..EF     80..BF      80..BF
                U+10000..U+3FFFF   F0         90..BF      80..BF     80..BF
                U+40000..U+FFFFF   F1..F3     80..BF      80..BF     80..BF
                U+100000..U+10FFFF F4         80..8F      80..BF     80..BF
            */

            uint32_t value = 0;
            I next = it_;
            unsigned char curr_c = *next;

            // One-byte case handled by caller

            // Two-byte
            if (detail::in(0xc2, curr_c, 0xdf)) {
                value = curr_c & 0b00011111;
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                // Three-byte
            } else if (curr_c == 0xe0) {
                value = curr_c & 0b00001111;
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c, 0xa0, 0xbf))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
            } else if (detail::in(0xe1, curr_c, 0xec)) {
                value = curr_c & 0b00001111;
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
            } else if (curr_c == 0xed) {
                value = curr_c & 0b00001111;
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c, 0x80, 0x9f))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
            } else if (detail::in(0xed, curr_c, 0xef)) {
                value = curr_c & 0b00001111;
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                // Four-byte
            } else if (curr_c == 0xf0) {
                value = curr_c & 0b00000111;
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c, 0x90, 0xbf))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
            } else if (detail::in(0xf1, curr_c, 0xf3)) {
                value = curr_c & 0b00000111;
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
            } else if (curr_c == 0xf4) {
                value = curr_c & 0b00000111;
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c, 0x80, 0x8f))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
                if (at_end(next))
                    return get_value_result{replacement_character(), next};
                curr_c = *next;
                if (!check_continuation(curr_c))
                    return get_value_result{replacement_character(), next};
                value = (value << 6) + (curr_c & 0b00111111);
                ++next;
            } else {
                value = ErrorHandler{}("Invalid initial UTF-8 code unit.");
                ++next;
            }
            return get_value_result{value, next};
#else
            I next = it_;
            uint32_t const value = detail::advance(next, last_);
            return get_value_result{value, next};
#endif
        }

        BOOST_TEXT_CXX14_CONSTEXPR I increment() const noexcept(!throw_on_error)
        {
            unsigned char curr_c = *it_;
            if (curr_c < 0x80)
                return std::next(it_);
            return get_value().it_;
        }

        I first_;
        I it_;
        S last_;

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<
            utf8_iter I2,
            std::sentinel_for<I2> S2,
            transcoding_error_handler ErrorHandler2>
#else
        template<typename I2, typename S2, typename ErrorHandler2>
#endif
        friend struct utf_8_to_16_iterator;

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<
            utf8_iter I2,
            std::sentinel_for<I2> S2,
            transcoding_error_handler ErrorHandler2>
#else
        template<typename I2, typename S2, typename ErrorHandler2>
#endif
        friend struct utf_8_to_32_iterator;

#endif
    };

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_8_to_32_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() == rhs)
    {
        return lhs.base() == rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        Sentinel lhs,
        utf_8_to_32_iterator<Iter, Sentinel, ErrorHandler> const & rhs) noexcept
        -> decltype(rhs.base() == lhs)
    {
        return rhs.base() == lhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_8_to_32_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() != rhs)
    {
        return lhs.base() != rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        Sentinel lhs,
        utf_8_to_32_iterator<Iter, Sentinel, ErrorHandler> const & rhs) noexcept
        -> decltype(rhs.base() != lhs)
    {
        return rhs.base() != lhs;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_8_to_32_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_8_to_32_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(lhs.base() == rhs.base())
    {
        return lhs.base() == rhs.base();
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_8_to_32_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_8_to_32_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(!(lhs == rhs))
    {
        return !(lhs == rhs);
    }


    namespace detail {
        template<typename OutIter>
        OutIter assign_8_to_32_insert(
            unsigned char cu, uint32_t & cp, int & state, OutIter out)
        {
            auto write = [&] {
                *out = cp;
                ++out;
                state = invalid_table_state;
            };
            auto start_cp = [&] {
                first_cu const info = first_cus[cu];
                state = info.next;
                cp = info.initial_octet;
                if (state == bgn)
                    write();
            };
            if (state == invalid_table_state) {
                start_cp();
            } else {
                cp = (cp << 6) | (cu & 0x3f);
                char_class const class_ = octet_classes[cu];
                state = transitions[state + class_];
                if (state == bgn) {
                    write();
                } else if (state == err) {
                    *out = replacement_character();
                    ++out;
                    start_cp();
                }
            }
            return out;
        }
    }

    /** An out iterator that converts UTF-8 to UTF-32. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<std::output_iterator<uint32_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_8_to_32_out_iterator
        : detail::trans_ins_iter<utf_8_to_32_out_iterator<Iter>, Iter>
    {
        utf_8_to_32_out_iterator() noexcept {}
        explicit utf_8_to_32_out_iterator(Iter it) noexcept :
            detail::trans_ins_iter<utf_8_to_32_out_iterator<Iter>, Iter>(it),
            state_(detail::invalid_table_state)
        {}

        utf_8_to_32_out_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_32_insert(cu, cp_, state_, out);
            return *this;
        }

        Iter base() const
        {
            return const_cast<utf_8_to_32_out_iterator * const>(this)->iter();
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        uint32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-8 to UTF-32. */
    template<typename Cont>
    struct utf_8_to_32_insert_iterator : detail::trans_ins_iter<
                                             utf_8_to_32_insert_iterator<Cont>,
                                             std::insert_iterator<Cont>>
    {
        utf_8_to_32_insert_iterator() noexcept {}
        utf_8_to_32_insert_iterator(
            Cont & c, typename Cont::iterator it) noexcept :
            detail::trans_ins_iter<
                utf_8_to_32_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it)),
            state_(detail::invalid_table_state)
        {}

        utf_8_to_32_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_32_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        uint32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-8 to UTF-32. */
    template<typename Cont>
    struct utf_8_to_32_front_insert_iterator
        : detail::trans_ins_iter<
              utf_8_to_32_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        utf_8_to_32_front_insert_iterator() noexcept {}
        explicit utf_8_to_32_front_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_8_to_32_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c)),
            state_(detail::invalid_table_state)
        {}

        utf_8_to_32_front_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_32_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        uint32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-8 to UTF-32. */
    template<typename Cont>
    struct utf_8_to_32_back_insert_iterator
        : detail::trans_ins_iter<
              utf_8_to_32_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        utf_8_to_32_back_insert_iterator() noexcept {}
        explicit utf_8_to_32_back_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_8_to_32_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c)),
            state_(detail::invalid_table_state)
        {}

        utf_8_to_32_back_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_32_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        uint32_t cp_;
#endif
    };


    /** A UTF-32 to UTF-16 converting iterator. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<
        utf32_iter I,
        std::sentinel_for<I> S = I,
        transcoding_error_handler ErrorHandler = use_replacement_character>
#else
    template<
        typename I,
        typename S = I,
        typename ErrorHandler = use_replacement_character>
#endif
    struct utf_32_to_16_iterator
        : detail::
              trans_iter<utf_32_to_16_iterator<I, S, ErrorHandler>, uint16_t>
    {
        static bool const throw_on_error =
            !noexcept(std::declval<ErrorHandler>()(0));

#if !BOOST_TEXT_USE_CONCEPTS
        static_assert(
            std::is_same<
                typename std::iterator_traits<I>::iterator_category,
                std::bidirectional_iterator_tag>::value ||
                std::is_same<
                    typename std::iterator_traits<I>::iterator_category,
                    std::random_access_iterator_tag>::value,
            "utf_32_to_16_iterator requires its I parameter to be at "
            "least "
            "bidirectional.");
        static_assert(
            sizeof(typename std::iterator_traits<I>::value_type) == 4,
            "utf_32_to_16_iterator requires its I parameter to produce a "
            "4-byte value_type.");
#endif

        constexpr utf_32_to_16_iterator() noexcept :
            first_(), it_(), last_(), index_(2), buf_()
        {}
        explicit BOOST_TEXT_CXX14_CONSTEXPR
        utf_32_to_16_iterator(I first, I it, S last) noexcept :
            first_(first), it_(it), last_(last), index_(0), buf_()
        {
            if (it_ != last_)
                read_into_buf();
        }
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<typename I2, typename S2>
        // clang-format off
        requires std::convertible_to<I2, I> && std::convertible_to<S2, S>
#else
        template<
            typename I2,
            typename S2,
            typename Enable = std::enable_if_t<
                std::is_convertible<I2, I>::value &&
                std::is_convertible<S2, S>::value>>
#endif
        constexpr utf_32_to_16_iterator(
            utf_32_to_16_iterator<I2, S2, ErrorHandler> const & other) noexcept
            // clang-format on
            :
            first_(other.first_),
            it_(other.it_),
            last_(other.last_),
            index_(other.index_),
            buf_(other.buf_)
        {}

        BOOST_TEXT_CXX14_CONSTEXPR uint16_t operator*() const
            noexcept(!throw_on_error)
        {
            return buf_[index_];
        }

        BOOST_TEXT_CXX14_CONSTEXPR I base() const noexcept { return it_; }

        BOOST_TEXT_CXX14_CONSTEXPR utf_32_to_16_iterator &
        operator++() noexcept(!throw_on_error)
        {
            ++index_;
            if (at_buf_end()) {
                BOOST_ASSERT(it_ != last_);
                ++it_;
                index_ = 0;
                if (it_ != last_)
                    read_into_buf();
            }
            return *this;
        }

        BOOST_TEXT_CXX14_CONSTEXPR utf_32_to_16_iterator &
        operator--() noexcept(!throw_on_error)
        {
            if (0 < index_) {
                --index_;
            } else {
                BOOST_ASSERT(it_ != first_);
                --it_;
                auto out = read_into_buf();
                index_ = out - buf_.data() - 1;
            }
            return *this;
        }

        template<
            typename I1,
            typename S1,
            typename I2,
            typename S2,
            typename ErrorHandler2>
        friend BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
            utf_32_to_16_iterator<I1, S1, ErrorHandler2> const & lhs,
            utf_32_to_16_iterator<I2, S2, ErrorHandler2> const & rhs) noexcept
            -> decltype(lhs.base() == rhs.base());

        friend bool operator==(
            utf_32_to_16_iterator lhs, utf_32_to_16_iterator rhs) noexcept
        {
            return lhs.base() == rhs.base() && lhs.index_ == rhs.index_;
        }

        using base_type = detail::
            trans_iter<utf_32_to_16_iterator<I, S, ErrorHandler>, uint16_t>;
        using base_type::operator++;
        using base_type::operator--;

#ifndef BOOST_TEXT_DOXYGEN
    private:
        constexpr bool at_buf_end() const noexcept { return buf_[index_] == 0; }

        BOOST_TEXT_CXX14_CONSTEXPR uint16_t *
        read_into_buf() noexcept(!throw_on_error)
        {
            auto const last = detail::write_cp_utf16(*it_, buf_.data());
            *last = 0;
            return last;
        }

        I first_;
        I it_;
        S last_;
        int index_;
        std::array<uint16_t, 4> buf_;

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<
            utf32_iter I2,
            std::sentinel_for<I2> S2,
            transcoding_error_handler ErrorHandler2>
#else
        template<typename I2, typename S2, typename ErrorHandler2>
#endif
        friend struct utf_32_to_16_iterator;
#endif
    };

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_32_to_16_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() == rhs)
    {
        return lhs.base() == rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        Sentinel lhs,
        utf_32_to_16_iterator<Iter, Sentinel, ErrorHandler> const &
            rhs) noexcept -> decltype(rhs.base() == lhs)
    {
        return rhs.base() == lhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_32_to_16_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() != rhs)
    {
        return lhs.base() != rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        Sentinel lhs,
        utf_32_to_16_iterator<Iter, Sentinel, ErrorHandler> const &
            rhs) noexcept -> decltype(rhs.base() != lhs)
    {
        return rhs.base() != lhs;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_32_to_16_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_32_to_16_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(lhs.base() == rhs.base())
    {
        return lhs.base() == rhs.base() && rhs.index_ == lhs.index_;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_32_to_16_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_32_to_16_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(!(lhs == rhs))
    {
        return !(lhs == rhs);
    }


    /** An out iterator that converts UTF-8 to UTF-16. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<std::output_iterator<uint16_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_32_to_16_out_iterator
        : detail::trans_ins_iter<utf_32_to_16_out_iterator<Iter>, Iter>
    {
        utf_32_to_16_out_iterator() noexcept {}
        explicit utf_32_to_16_out_iterator(Iter it) noexcept :
            detail::trans_ins_iter<utf_32_to_16_out_iterator<Iter>, Iter>(it)
        {}

        utf_32_to_16_out_iterator & operator=(uint32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf16(cp, out);
            return *this;
        }

        Iter base() const
        {
            return const_cast<utf_32_to_16_out_iterator * const>(this)->iter();
        }
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-32 to UTF-16. */
    template<typename Cont>
    struct utf_32_to_16_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_16_insert_iterator<Cont>,
              std::insert_iterator<Cont>>
    {
        utf_32_to_16_insert_iterator() noexcept {}
        utf_32_to_16_insert_iterator(
            Cont & c, typename Cont::iterator it) noexcept :
            detail::trans_ins_iter<
                utf_32_to_16_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it))
        {}

        utf_32_to_16_insert_iterator & operator=(uint32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf16(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-32 to UTF-16. */
    template<typename Cont>
    struct utf_32_to_16_front_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_16_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        utf_32_to_16_front_insert_iterator() noexcept {}
        explicit utf_32_to_16_front_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_32_to_16_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c))
        {}

        utf_32_to_16_front_insert_iterator & operator=(uint32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf16(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-32 to UTF-16. */
    template<typename Cont>
    struct utf_32_to_16_back_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_16_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        utf_32_to_16_back_insert_iterator() noexcept {}
        explicit utf_32_to_16_back_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_32_to_16_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c))
        {}

        utf_32_to_16_back_insert_iterator & operator=(uint32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf16(cp, out);
            return *this;
        }
    };


    /** A UTF-16 to UTF-32 converting iterator. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<
        utf16_iter I,
        std::sentinel_for<I> S = I,
        transcoding_error_handler ErrorHandler = use_replacement_character>
#else
    template<
        typename I,
        typename S = I,
        typename ErrorHandler = use_replacement_character>
#endif
    struct utf_16_to_32_iterator
        : detail::
              trans_iter<utf_16_to_32_iterator<I, S, ErrorHandler>, uint32_t>
    {
        static bool const throw_on_error =
            !noexcept(std::declval<ErrorHandler>()(0));

#if !BOOST_TEXT_USE_CONCEPTS
        static_assert(
            std::is_same<
                typename std::iterator_traits<I>::iterator_category,
                std::bidirectional_iterator_tag>::value ||
                std::is_same<
                    typename std::iterator_traits<I>::iterator_category,
                    std::random_access_iterator_tag>::value,
            "utf_16_to_32_iterator requires its I parameter to be at "
            "least "
            "bidirectional.");
        static_assert(
            sizeof(typename std::iterator_traits<I>::value_type) == 2,
            "utf_16_to_32_iterator requires its I parameter to produce a "
            "2-byte value_type.");
#endif

        constexpr utf_16_to_32_iterator() noexcept : first_(), it_(), last_() {}
        explicit constexpr utf_16_to_32_iterator(I first, I it, S last) noexcept
            :
            first_(first), it_(it), last_(last)
        {}
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<typename I2, typename S2>
        // clang-format off
        requires std::convertible_to<I2, I> && std::convertible_to<S2, S>
#else
        template<
            typename I2,
            typename S2,
            typename Enable = std::enable_if_t<
                std::is_convertible<I2, I>::value &&
                std::is_convertible<S2, S>::value>>
#endif
        constexpr utf_16_to_32_iterator(
            utf_16_to_32_iterator<I2, S2, ErrorHandler> const & other) noexcept
            :
        // clang-format off
            first_(other.first_), it_(other.it_), last_(other.last_)
        {}

        BOOST_TEXT_CXX14_CONSTEXPR uint32_t operator*() const
            noexcept(!throw_on_error)
        {
            BOOST_ASSERT(!at_end(it_));
            return get_value(*it_).value_;
        }

        BOOST_TEXT_CXX14_CONSTEXPR I base() const noexcept { return it_; }

        BOOST_TEXT_CXX14_CONSTEXPR utf_16_to_32_iterator &
        operator++() noexcept(!throw_on_error)
        {
            BOOST_ASSERT(it_ != last_);
            it_ = increment();
            return *this;
        }

        BOOST_TEXT_CXX14_CONSTEXPR utf_16_to_32_iterator &
        operator--() noexcept(!throw_on_error)
        {
            BOOST_ASSERT(it_ != first_);
            if (boost::text::low_surrogate(*--it_)) {
                if (it_ != first_ &&
                    boost::text::high_surrogate(*std::prev(it_)))
                    --it_;
            }
            return *this;
        }

        friend bool operator==(
            utf_16_to_32_iterator lhs, utf_16_to_32_iterator rhs) noexcept
        {
            return lhs.base() == rhs.base();
        }

        using base_type = detail::
            trans_iter<utf_16_to_32_iterator<I, S, ErrorHandler>, uint32_t>;
        using base_type::operator++;
        using base_type::operator--;

#ifndef BOOST_TEXT_DOXYGEN
    private:
        struct get_value_result
        {
            uint32_t value_;
            I it_;
        };

        BOOST_TEXT_CXX14_CONSTEXPR bool at_end(I it) const
            noexcept(!throw_on_error)
        {
            if (it == last_) {
                ErrorHandler{}(
                    "Invalid UTF-16 sequence; expected another code unit "
                    "before the end of string.");
                return true;
            } else {
                return false;
            }
        }

        BOOST_TEXT_CXX14_CONSTEXPR get_value_result
        get_value(uint16_t curr) const noexcept(!throw_on_error)
        {
            uint32_t value = 0;
            I next = std::next(it_);

            if (high_surrogate(curr)) {
                value = (curr - high_surrogate_base) << 10;
                if (at_end(next)) {
                    return get_value_result{replacement_character(), next};
                }
                curr = *next++;
                if (!low_surrogate(curr)) {
                    return get_value_result{replacement_character(), next};
                }
                value += curr - low_surrogate_base;
            } else if (low_surrogate(curr)) {
                value = ErrorHandler{}("Invalid initial UTF-16 code unit.");
                return get_value_result{replacement_character(), next};
            } else {
                value = curr;
            }

            if (!valid_code_point(value)) {
                value = ErrorHandler{}(
                    "UTF-16 sequence results in invalid UTF-32 code point.");
            }

            return get_value_result{value, next};
        }

        BOOST_TEXT_CXX14_CONSTEXPR I increment() const noexcept(!throw_on_error)
        {
            return get_value(*it_).it_;
        }

        I first_;
        I it_;
        S last_;

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<
            utf32_iter I2,
            std::sentinel_for<I2> S2,
            transcoding_error_handler ErrorHandler2>
#else
        template<typename I2, typename S2, typename ErrorHandler2>
#endif
        friend struct utf_32_to_16_iterator;

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<
            utf16_iter I2,
            std::sentinel_for<I2> S2,
            transcoding_error_handler ErrorHandler2>
#else
        template<typename I2, typename S2, typename ErrorHandler2>
#endif
        friend struct utf_16_to_32_iterator;

#endif
    };

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_16_to_32_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() == rhs)
    {
        return lhs.base() == rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        Sentinel lhs,
        utf_16_to_32_iterator<Iter, Sentinel, ErrorHandler> const &
            rhs) noexcept -> decltype(rhs.base() == lhs)
    {
        return rhs.base() == lhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_16_to_32_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() != rhs)
    {
        return lhs.base() != rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        Sentinel lhs,
        utf_16_to_32_iterator<Iter, Sentinel, ErrorHandler> const &
            rhs) noexcept -> decltype(rhs.base() != lhs)
    {
        return rhs.base() != lhs;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_16_to_32_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_16_to_32_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(lhs.base() == rhs.base())
    {
        return lhs.base() == rhs.base();
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_16_to_32_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_16_to_32_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(!(lhs == rhs))
    {
        return !(lhs == rhs);
    }


    namespace detail {
        template<typename OutIter>
        OutIter
        assign_16_to_32_insert(uint16_t & prev_cu, uint16_t cu, OutIter out)
        {
            if (high_surrogate(cu)) {
                if (prev_cu) {
                    *out = replacement_character();
                    ++out;
                }
                prev_cu = cu;
            } else if (low_surrogate(cu)) {
                if (prev_cu) {
                    *out = detail::surrogates_to_cp(prev_cu, cu);
                    ++out;
                } else {
                    *out = replacement_character();
                    ++out;
                }
                prev_cu = 0;
            } else {
                if (prev_cu) {
                    *out = replacement_character();
                    ++out;
                }
                *out = cu;
                ++out;
                prev_cu = 0;
            }
            return out;
        }
    }

    /** An out iterator that converts UTF-16 to UTF-32. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<std::output_iterator<uint32_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_16_to_32_out_iterator
        : detail::trans_ins_iter<utf_16_to_32_out_iterator<Iter>, Iter>
    {
        utf_16_to_32_out_iterator() noexcept {}
        explicit utf_16_to_32_out_iterator(Iter it) noexcept :
            detail::trans_ins_iter<utf_16_to_32_out_iterator<Iter>, Iter>(it),
            prev_cu_(0)
        {}

        utf_16_to_32_out_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_32_insert(prev_cu_, cu, out);
            return *this;
        }

        Iter base() const
        {
            return const_cast<utf_16_to_32_out_iterator * const>(this)->iter();
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        uint16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-16 to UTF-32. */
    template<typename Cont>
    struct utf_16_to_32_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_32_insert_iterator<Cont>,
              std::insert_iterator<Cont>>
    {
        utf_16_to_32_insert_iterator() noexcept {}
        utf_16_to_32_insert_iterator(
            Cont & c, typename Cont::iterator it) noexcept :
            detail::trans_ins_iter<
                utf_16_to_32_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it)),
            prev_cu_(0)
        {}

        utf_16_to_32_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_32_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        uint16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-16 to UTF-32. */
    template<typename Cont>
    struct utf_16_to_32_front_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_32_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        utf_16_to_32_front_insert_iterator() noexcept {}
        explicit utf_16_to_32_front_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_16_to_32_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c)),
            prev_cu_(0)
        {}

        utf_16_to_32_front_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_32_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        uint16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-16 to UTF-32. */
    template<typename Cont>
    struct utf_16_to_32_back_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_32_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        utf_16_to_32_back_insert_iterator() noexcept {}
        explicit utf_16_to_32_back_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_16_to_32_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c)),
            prev_cu_(0)
        {}

        utf_16_to_32_back_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_32_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        uint16_t prev_cu_;
#endif
    };


    /** A UTF-16 to UTF-8 converting iterator. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<
        utf16_iter I,
        std::sentinel_for<I> S = I,
        transcoding_error_handler ErrorHandler = use_replacement_character>
#else
    template<
        typename I,
        typename S = I,
        typename ErrorHandler = use_replacement_character>
#endif
    struct utf_16_to_8_iterator
        : detail::trans_iter<utf_16_to_8_iterator<I, S, ErrorHandler>, char>
    {
        static bool const throw_on_error =
            !noexcept(std::declval<ErrorHandler>()(0));

#if !BOOST_TEXT_USE_CONCEPTS
        static_assert(
            std::is_same<
                typename std::iterator_traits<I>::iterator_category,
                std::bidirectional_iterator_tag>::value ||
                std::is_same<
                    typename std::iterator_traits<I>::iterator_category,
                    std::random_access_iterator_tag>::value,
            "utf_16_to_8_iterator requires its I parameter to be at least "
            "bidirectional.");
        static_assert(
            sizeof(typename std::iterator_traits<I>::value_type) == 2,
            "utf_16_to_8_iterator requires its I parameter to produce a "
            "2-byte value_type.");
#endif

        constexpr utf_16_to_8_iterator() noexcept :
            first_(), it_(), last_(), index_(4), buf_()
        {}
        explicit BOOST_TEXT_CXX14_CONSTEXPR
        utf_16_to_8_iterator(I first, I it, S last) noexcept :
            first_(first), it_(it), last_(last), index_(0), buf_()
        {
            if (it_ != last_)
                read_into_buf();
        }
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<typename I2, typename S2>
        // clang-format off
        requires std::convertible_to<I2, I> && std::convertible_to<S2, S>
#else
        template<
            typename I2,
            typename S2,
            typename Enable = std::enable_if_t<
                std::is_convertible<I2, I>::value &&
                std::is_convertible<S2, S>::value>>
#endif
        constexpr utf_16_to_8_iterator(
            utf_16_to_8_iterator<I2, S2> const & other) noexcept :
            // clang-format on
            first_(other.first_),
            it_(other.it_),
            last_(other.last_),
            index_(other.index_),
            buf_(other.buf_)
        {}

        BOOST_TEXT_CXX14_CONSTEXPR char operator*() const
            noexcept(!throw_on_error)
        {
            return buf_[index_];
        }

        BOOST_TEXT_CXX14_CONSTEXPR I base() const noexcept { return it_; }

        BOOST_TEXT_CXX14_CONSTEXPR utf_16_to_8_iterator &
        operator++() noexcept(!throw_on_error)
        {
            ++index_;
            if (at_buf_end()) {
                BOOST_ASSERT(it_ != last_);
                increment();
                index_ = 0;
                if (it_ != last_)
                    read_into_buf();
            }
            return *this;
        }

        BOOST_TEXT_CXX14_CONSTEXPR utf_16_to_8_iterator &
        operator--() noexcept(!throw_on_error)
        {
            if (0 < index_) {
                --index_;
            } else {
                BOOST_ASSERT(it_ != first_);
                decrement();
                auto out = read_into_buf();
                index_ = out - buf_.data() - 1;
            }
            return *this;
        }

        template<
            typename I1,
            typename S1,
            typename I2,
            typename S2,
            typename ErrorHandler2>
        friend BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
            utf_16_to_8_iterator<I1, S1, ErrorHandler2> const & lhs,
            utf_16_to_8_iterator<I2, S2, ErrorHandler2> const & rhs) noexcept
            -> decltype(lhs.base() == rhs.base());

        friend bool
        operator==(utf_16_to_8_iterator lhs, utf_16_to_8_iterator rhs) noexcept
        {
            return lhs.base() == rhs.base() && lhs.index_ == rhs.index_;
        }

        using base_type =
            detail::trans_iter<utf_16_to_8_iterator<I, S, ErrorHandler>, char>;
        using base_type::operator++;
        using base_type::operator--;

#ifndef BOOST_TEXT_DOXYGEN
    private:
        BOOST_TEXT_CXX14_CONSTEXPR bool at_end(I it) const
            noexcept(!throw_on_error)
        {
            if (it == last_) {
                ErrorHandler{}(
                    "Invalid UTF-16 sequence; expected another code unit "
                    "before the end of string.");
                return true;
            } else {
                return false;
            }
        }

        constexpr bool at_buf_end() const noexcept
        {
            return buf_[index_] == '\0';
        }

        BOOST_TEXT_CXX14_CONSTEXPR char *
        read_into_buf() noexcept(!throw_on_error)
        {
            I next = it_;

            uint32_t first = static_cast<uint32_t>(*next);
            uint32_t second = 0;
            uint32_t cp = first;
            if (boost::text::high_surrogate(first)) {
                if (at_end(++next))
                    cp = replacement_character();
                else {
                    second = static_cast<uint32_t>(*next);
                    if (!boost::text::low_surrogate(second)) {
                        ErrorHandler{}(
                            "Invalid UTF-16 sequence; expected low surrogate "
                            "after high surrogate.");
                        cp = replacement_character();
                    } else {
                        cp = (first << 10) + second + surrogate_offset;
                    }
                }
            } else if (boost::text::surrogate(first)) {
                ErrorHandler{}("Invalid initial UTF-16 code unit.");
                cp = replacement_character();
            }

            char * retval = detail::read_into_buf(cp, buf_.data());
            *retval = 0;
            return retval;
        }

        BOOST_TEXT_CXX14_CONSTEXPR void increment() noexcept
        {
            if (boost::text::high_surrogate(*it_)) {
                ++it_;
                if (it_ != last_ && boost::text::low_surrogate(*it_))
                    ++it_;
            } else {
                ++it_;
            }
        }

        BOOST_TEXT_CXX14_CONSTEXPR void decrement() noexcept
        {
            if (boost::text::low_surrogate(*--it_)) {
                if (it_ != first_)
                    --it_;
            }
        }

        I first_;
        I it_;
        S last_;
        int index_;
        std::array<char, 5> buf_;

        // Unicode 3.8/D71-D74

        static uint32_t const surrogate_offset =
            0x10000 - (high_surrogate_min << 10) - low_surrogate_min;

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<
            utf16_iter I2,
            std::sentinel_for<I2> S2,
            transcoding_error_handler ErrorHandler2>
#else
        template<typename I2, typename S2, typename ErrorHandler2>
#endif
        friend struct utf_16_to_8_iterator;
#endif
    };

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_16_to_8_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() == rhs)
    {
        return lhs.base() == rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        Sentinel lhs,
        utf_16_to_8_iterator<Iter, Sentinel, ErrorHandler> const & rhs) noexcept
        -> decltype(rhs.base() == lhs)
    {
        return rhs.base() == lhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_16_to_8_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() != rhs)
    {
        return lhs.base() != rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        Sentinel lhs,
        utf_16_to_8_iterator<Iter, Sentinel, ErrorHandler> const & rhs) noexcept
        -> decltype(rhs.base() != lhs)
    {
        return rhs.base() != lhs;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_16_to_8_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_16_to_8_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(lhs.base() == rhs.base())
    {
        return lhs.base() == rhs.base() && rhs.index_ == lhs.index_;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_16_to_8_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_16_to_8_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(!(lhs == rhs))
    {
        return !(lhs == rhs);
    }


    namespace detail {
        template<typename OutIter>
        OutIter
        assign_16_to_8_insert(uint16_t & prev_cu, uint16_t cu, OutIter out)
        {
            if (high_surrogate(cu)) {
                if (prev_cu)
                    out = detail::write_cp_utf8(replacement_character(), out);
                prev_cu = cu;
            } else if (low_surrogate(cu)) {
                if (prev_cu) {
                    auto const cp = detail::surrogates_to_cp(prev_cu, cu);
                    out = detail::write_cp_utf8(cp, out);
                } else {
                    out = detail::write_cp_utf8(replacement_character(), out);
                }
                prev_cu = 0;
            } else {
                if (prev_cu)
                    out = detail::write_cp_utf8(replacement_character(), out);
                out = detail::write_cp_utf8(cu, out);
                prev_cu = 0;
            }
            return out;
        }
    }

    /** An out iterator that converts UTF-16 to UTF-8. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<std::output_iterator<uint8_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_16_to_8_out_iterator
        : detail::trans_ins_iter<utf_16_to_8_out_iterator<Iter>, Iter>
    {
        utf_16_to_8_out_iterator() noexcept {}
        explicit utf_16_to_8_out_iterator(Iter it) noexcept :
            detail::trans_ins_iter<utf_16_to_8_out_iterator<Iter>, Iter>(it),
            prev_cu_(0)
        {}

        utf_16_to_8_out_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_8_insert(prev_cu_, cu, out);
            return *this;
        }

        Iter base() const
        {
            return const_cast<utf_16_to_8_out_iterator * const>(this)->iter();
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        uint16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-16 to UTF-8. */
    template<typename Cont>
    struct utf_16_to_8_insert_iterator : detail::trans_ins_iter<
                                             utf_16_to_8_insert_iterator<Cont>,
                                             std::insert_iterator<Cont>>
    {
        utf_16_to_8_insert_iterator() noexcept {}
        utf_16_to_8_insert_iterator(
            Cont & c, typename Cont::iterator it) noexcept :
            detail::trans_ins_iter<
                utf_16_to_8_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it)),
            prev_cu_(0)
        {}

        utf_16_to_8_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_8_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        uint16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-16 to UTF-8. */
    template<typename Cont>
    struct utf_16_to_8_front_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_8_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        utf_16_to_8_front_insert_iterator() noexcept {}
        explicit utf_16_to_8_front_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_16_to_8_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c)),
            prev_cu_(0)
        {}

        utf_16_to_8_front_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_8_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        uint16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-16 to UTF-8. */
    template<typename Cont>
    struct utf_16_to_8_back_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_8_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        utf_16_to_8_back_insert_iterator() noexcept {}
        explicit utf_16_to_8_back_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_16_to_8_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c)),
            prev_cu_(0)
        {}

        utf_16_to_8_back_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_8_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        uint16_t prev_cu_;
#endif
    };


#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<
        utf8_iter I,
        std::sentinel_for<I> S,
        transcoding_error_handler ErrorHandler>
#else
    template<typename I, typename S, typename ErrorHandler>
#endif
    struct utf_8_to_16_iterator
        : detail::trans_iter<utf_8_to_16_iterator<I, S, ErrorHandler>, uint16_t>
    {
        static bool const throw_on_error =
            !noexcept(std::declval<ErrorHandler>()(0));

        constexpr utf_8_to_16_iterator() noexcept : it_(), index_(2), buf_() {}
        explicit BOOST_TEXT_CXX14_CONSTEXPR
        utf_8_to_16_iterator(I first, I it, S last) noexcept :
            it_(first, it, last), index_(0), buf_()
        {
            if (it_.it_ != it_.last_)
                read_into_buf();
        }
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<typename I2, typename S2>
        // clang-format off
        requires std::convertible_to<I2, I> && std::convertible_to<S2, S>
#else
        template<
            typename I2,
            typename S2,
            typename Enable = std::enable_if_t<
                std::is_convertible<I2, I>::value &&
                std::is_convertible<S2, S>::value>>
#endif
        constexpr utf_8_to_16_iterator(
            utf_8_to_16_iterator<I2, S2, ErrorHandler> const & other) noexcept :
            // clang-format on
            it_(other.it_),
            index_(other.index_),
            buf_(other.buf_)
        {}

        BOOST_TEXT_CXX14_CONSTEXPR uint16_t operator*() const
            noexcept(!throw_on_error)
        {
            return buf_[index_];
        }

        BOOST_TEXT_CXX14_CONSTEXPR I base() const noexcept
        {
            return it_.base();
        }

        BOOST_TEXT_CXX14_CONSTEXPR utf_8_to_16_iterator &
        operator++() noexcept(!throw_on_error)
        {
            ++index_;
            if (at_buf_end()) {
                BOOST_ASSERT(it_.it_ != it_.last_);
                ++it_;
                index_ = 0;
                if (it_.it_ != it_.last_)
                    read_into_buf();
            }
            return *this;
        }

        BOOST_TEXT_CXX14_CONSTEXPR utf_8_to_16_iterator &
        operator--() noexcept(!throw_on_error)
        {
            if (0 < index_) {
                --index_;
            } else {
                BOOST_ASSERT(it_.it_ != it_.first_);
                --it_;
                auto out = read_into_buf();
                index_ = out - buf_.data() - 1;
            }
            return *this;
        }

        template<
            typename I1,
            typename S1,
            typename I2,
            typename S2,
            typename ErrorHandler2>
        friend BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
            utf_8_to_16_iterator<I1, S1, ErrorHandler2> const & lhs,
            utf_8_to_16_iterator<I2, S2, ErrorHandler2> const & rhs) noexcept
            -> decltype(lhs.base() == rhs.base());

        friend bool
        operator==(utf_8_to_16_iterator lhs, utf_8_to_16_iterator rhs) noexcept
        {
            return lhs.base() == rhs.base() && lhs.index_ == rhs.index_;
        }

        using base_type = detail::
            trans_iter<utf_8_to_16_iterator<I, S, ErrorHandler>, uint16_t>;
        using base_type::operator++;
        using base_type::operator--;

#ifndef BOOST_TEXT_DOXYGEN
    private:
        constexpr bool at_buf_end() const noexcept { return buf_[index_] == 0; }

        BOOST_TEXT_CXX14_CONSTEXPR uint16_t *
        read_into_buf() noexcept(!throw_on_error)
        {
            auto const last = detail::write_cp_utf16(*it_, buf_.data());
            *last = 0;
            return last;
        }

        utf_8_to_32_iterator<I, S> it_;
        int index_;
        std::array<uint16_t, 4> buf_;

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
        template<
            utf8_iter I2,
            std::sentinel_for<I2> S2,
            transcoding_error_handler ErrorHandler2>
#else
        template<typename I2, typename S2, typename ErrorHandler2>
#endif
        friend struct utf_8_to_16_iterator;
#endif
    };

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_8_to_16_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() == rhs)
    {
        return lhs.base() == rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        Sentinel lhs,
        utf_8_to_16_iterator<Iter, Sentinel, ErrorHandler> const & rhs) noexcept
        -> decltype(rhs.base() == lhs)
    {
        return rhs.base() == lhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_8_to_16_iterator<Iter, Sentinel, ErrorHandler> const & lhs,
        Sentinel rhs) noexcept -> decltype(lhs.base() != rhs)
    {
        return lhs.base() != rhs;
    }

    template<typename Iter, typename Sentinel, typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        Sentinel lhs,
        utf_8_to_16_iterator<Iter, Sentinel, ErrorHandler> const & rhs) noexcept
        -> decltype(rhs.base() != lhs)
    {
        return rhs.base() != lhs;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator==(
        utf_8_to_16_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_8_to_16_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(lhs.base() == rhs.base())
    {
        return lhs.base() == rhs.base() && rhs.index_ == lhs.index_;
    }

    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename ErrorHandler>
    BOOST_TEXT_CXX14_CONSTEXPR auto operator!=(
        utf_8_to_16_iterator<Iter1, Sentinel1, ErrorHandler> const & lhs,
        utf_8_to_16_iterator<Iter2, Sentinel2, ErrorHandler> const &
            rhs) noexcept -> decltype(!(lhs == rhs))
    {
        return !(lhs == rhs);
    }


    namespace detail {
        template<typename OutIter>
        OutIter assign_8_to_16_insert(
            unsigned char cu, uint32_t & cp, int & state, OutIter out)
        {
            auto write = [&] {
                out = detail::write_cp_utf16(cp, out);
                state = invalid_table_state;
            };
            auto start_cp = [&] {
                first_cu const info = first_cus[cu];
                state = info.next;
                cp = info.initial_octet;
                if (state == bgn)
                    write();
            };
            if (state == invalid_table_state) {
                start_cp();
            } else {
                cp = (cp << 6) | (cu & 0x3f);
                char_class const class_ = octet_classes[cu];
                state = transitions[state + class_];
                if (state == bgn) {
                    write();
                } else if (state == err) {
                    out = detail::write_cp_utf16(replacement_character(), out);
                    start_cp();
                }
            }
            return out;
        }
    }

    /** An out iterator that converts UTF-8 to UTF-16. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<std::output_iterator<uint16_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_8_to_16_out_iterator
        : detail::trans_ins_iter<utf_8_to_16_out_iterator<Iter>, Iter>
    {
        utf_8_to_16_out_iterator() noexcept {}
        explicit utf_8_to_16_out_iterator(Iter it) noexcept :
            detail::trans_ins_iter<utf_8_to_16_out_iterator<Iter>, Iter>(it),
            state_(detail::invalid_table_state)
        {}

        utf_8_to_16_out_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_16_insert(cu, cp_, state_, out);
            return *this;
        }

        Iter base() const
        {
            return const_cast<utf_8_to_16_out_iterator * const>(this)->iter();
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        uint32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-8 to UTF-16. */
    template<typename Cont>
    struct utf_8_to_16_insert_iterator : detail::trans_ins_iter<
                                             utf_8_to_16_insert_iterator<Cont>,
                                             std::insert_iterator<Cont>>
    {
        utf_8_to_16_insert_iterator() noexcept {}
        utf_8_to_16_insert_iterator(
            Cont & c, typename Cont::iterator it) noexcept :
            detail::trans_ins_iter<
                utf_8_to_16_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it)),
            state_(detail::invalid_table_state)
        {}

        utf_8_to_16_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_16_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        uint32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-8 to UTF-16. */
    template<typename Cont>
    struct utf_8_to_16_front_insert_iterator
        : detail::trans_ins_iter<
              utf_8_to_16_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        utf_8_to_16_front_insert_iterator() noexcept {}
        explicit utf_8_to_16_front_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_8_to_16_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c)),
            state_(detail::invalid_table_state)
        {}

        utf_8_to_16_front_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_16_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        uint32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-8 to UTF-16. */
    template<typename Cont>
    struct utf_8_to_16_back_insert_iterator
        : detail::trans_ins_iter<
              utf_8_to_16_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        utf_8_to_16_back_insert_iterator() noexcept {}
        explicit utf_8_to_16_back_insert_iterator(Cont & c) noexcept :
            detail::trans_ins_iter<
                utf_8_to_16_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c)),
            state_(detail::invalid_table_state)
        {}

        utf_8_to_16_back_insert_iterator & operator=(uint16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_16_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        uint32_t cp_;
#endif
    };

}}

#include <boost/text/detail/unpack.hpp>

namespace boost { namespace text { namespace detail {

    template<typename Tag>
    struct make_utf8_dispatch;

    template<>
    struct make_utf8_dispatch<detail::utf8_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr Iter call(Iter first, Iter it, Sentinel last) noexcept
        {
            return it;
        }
    };

    template<>
    struct make_utf8_dispatch<detail::utf16_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_16_to_8_iterator<Iter, Sentinel>
        call(Iter first, Iter it, Sentinel last) noexcept
        {
            return utf_16_to_8_iterator<Iter, Sentinel>(first, it, last);
        }
    };

    template<>
    struct make_utf8_dispatch<detail::utf32_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_32_to_8_iterator<Iter, Sentinel>
        call(Iter first, Iter it, Sentinel last) noexcept
        {
            return utf_32_to_8_iterator<Iter, Sentinel>(first, it, last);
        }
    };

    template<typename Tag>
    struct make_utf16_dispatch;

    template<>
    struct make_utf16_dispatch<detail::utf8_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_8_to_16_iterator<Iter, Sentinel>
        call(Iter first, Iter it, Sentinel last) noexcept
        {
            return utf_8_to_16_iterator<Iter, Sentinel>(first, it, last);
        }
    };

    template<>
    struct make_utf16_dispatch<detail::utf16_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr Iter call(Iter first, Iter it, Sentinel last) noexcept
        {
            return it;
        }
    };

    template<>
    struct make_utf16_dispatch<detail::utf32_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_32_to_16_iterator<Iter, Sentinel>
        call(Iter first, Iter it, Sentinel last) noexcept
        {
            return utf_32_to_16_iterator<Iter, Sentinel>(first, it, last);
        }
    };

    template<typename Tag>
    struct make_utf32_dispatch;

    template<>
    struct make_utf32_dispatch<detail::utf8_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_8_to_32_iterator<Iter, Sentinel>
        call(Iter first, Iter it, Sentinel last) noexcept
        {
            return utf_8_to_32_iterator<Iter, Sentinel>(first, it, last);
        }
    };

    template<>
    struct make_utf32_dispatch<detail::utf16_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_16_to_32_iterator<Iter, Sentinel>
        call(Iter first, Iter it, Sentinel last) noexcept
        {
            return utf_16_to_32_iterator<Iter, Sentinel>(first, it, last);
        }
    };

    template<>
    struct make_utf32_dispatch<detail::utf32_tag>
    {
        template<typename Iter, typename Sentinel>
        static constexpr Iter call(Iter first, Iter it, Sentinel last) noexcept
        {
            return it;
        }
    };

    template<
        typename Cont,
        typename UTF8,
        typename UTF16,
        typename UTF32,
        int Bytes = sizeof(typename Cont::value_type)>
    struct from_utf8_dispatch
    {
        using type = UTF8;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf8_dispatch<Cont, UTF8, UTF16, UTF32, 2>
    {
        using type = UTF16;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf8_dispatch<Cont, UTF8, UTF16, UTF32, 4>
    {
        using type = UTF32;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    using from_utf8_dispatch_t =
        typename from_utf8_dispatch<Cont, UTF8, UTF16, UTF32>::type;

    template<
        typename Cont,
        typename UTF8,
        typename UTF16,
        typename UTF32,
        int Bytes = sizeof(typename Cont::value_type)>
    struct from_utf16_dispatch
    {
        using type = UTF16;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf16_dispatch<Cont, UTF8, UTF16, UTF32, 1>
    {
        using type = UTF8;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf16_dispatch<Cont, UTF8, UTF16, UTF32, 4>
    {
        using type = UTF32;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    using from_utf16_dispatch_t =
        typename from_utf16_dispatch<Cont, UTF8, UTF16, UTF32>::type;

    template<
        typename Cont,
        typename UTF8,
        typename UTF16,
        typename UTF32,
        int Bytes = sizeof(typename Cont::value_type)>
    struct from_utf32_dispatch
    {
        using type = UTF32;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf32_dispatch<Cont, UTF8, UTF16, UTF32, 1>
    {
        using type = UTF8;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf32_dispatch<Cont, UTF8, UTF16, UTF32, 2>
    {
        using type = UTF16;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    using from_utf32_dispatch_t =
        typename from_utf32_dispatch<Cont, UTF8, UTF16, UTF32>::type;

}}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    /** Returns a `utf_32_to_8_out_iterator<Iter>` constructed from the given
        iterator. */
    template<typename Iter>
    utf_32_to_8_out_iterator<Iter> utf_32_to_8_out(Iter it) noexcept
    {
        return utf_32_to_8_out_iterator<Iter>(it);
    }

    /** Returns a `utf_8_to_32_out_iterator<Iter>` constructed from the given
        iterator. */
    template<typename Iter>
    utf_8_to_32_out_iterator<Iter> utf_8_to_32_out(Iter it) noexcept
    {
        return utf_8_to_32_out_iterator<Iter>(it);
    }

    /** Returns a `utf_32_to_16_out_iterator<Iter>` constructed from the given
        iterator. */
    template<typename Iter>
    utf_32_to_16_out_iterator<Iter> utf_32_to_16_out(Iter it) noexcept
    {
        return utf_32_to_16_out_iterator<Iter>(it);
    }

    /** Returns a `utf_16_to_32_out_iterator<Iter>` constructed from the given
        iterator. */
    template<typename Iter>
    utf_16_to_32_out_iterator<Iter> utf_16_to_32_out(Iter it) noexcept
    {
        return utf_16_to_32_out_iterator<Iter>(it);
    }

    /** Returns a `utf_16_to_8_out_iterator<Iter>` constructed from the given
        iterator. */
    template<typename Iter>
    utf_16_to_8_out_iterator<Iter> utf_16_to_8_out(Iter it) noexcept
    {
        return utf_16_to_8_out_iterator<Iter>(it);
    }

    /** Returns a `utf_8_to_16_out_iterator<Iter>` constructed from the given
        iterator. */
    template<typename Iter>
    utf_8_to_16_out_iterator<Iter> utf_8_to_16_out(Iter it) noexcept
    {
        return utf_8_to_16_out_iterator<Iter>(it);
    }

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-8. */
    template<typename Iter, typename Sentinel>
    auto utf8_iterator(Iter first, Iter it, Sentinel last) noexcept
    {
        auto const unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto const unpacked_it =
            detail::unpack_iterator_and_sentinel(it, last).f_;
        using tag_type = decltype(unpacked.tag_);
        return detail::make_utf8_dispatch<tag_type>::call(
            unpacked.f_, unpacked_it, unpacked.l_);
    }

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-16. */
    template<typename Iter, typename Sentinel>
    auto utf16_iterator(Iter first, Iter it, Sentinel last) noexcept
    {
        auto const unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto const unpacked_it =
            detail::unpack_iterator_and_sentinel(it, last).f_;
        using tag_type = decltype(unpacked.tag_);
        return detail::make_utf16_dispatch<tag_type>::call(
            unpacked.f_, unpacked_it, unpacked.l_);
    }

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-32. */
    template<typename Iter, typename Sentinel>
    auto utf32_iterator(Iter first, Iter it, Sentinel last) noexcept
    {
        auto const unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto const unpacked_it =
            detail::unpack_iterator_and_sentinel(it, last).f_;
        using tag_type = decltype(unpacked.tag_);
        return detail::make_utf32_dispatch<tag_type>::call(
            unpacked.f_, unpacked_it, unpacked.l_);
    }

    /** Returns a inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf8_inserter(Cont & c, typename Cont::iterator it) noexcept
    {
        using result_type = detail::from_utf8_dispatch_t<
            Cont,
            std::insert_iterator<Cont>,
            utf_8_to_16_insert_iterator<Cont>,
            utf_8_to_32_insert_iterator<Cont>>;
        return result_type(c, it);
    }

    /** Returns a inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf16_inserter(Cont & c, typename Cont::iterator it) noexcept
    {
        using result_type = detail::from_utf16_dispatch_t<
            Cont,
            utf_16_to_8_insert_iterator<Cont>,
            std::insert_iterator<Cont>,
            utf_16_to_32_insert_iterator<Cont>>;
        return result_type(c, it);
    }

    /** Returns a inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf32_inserter(Cont & c, typename Cont::iterator it) noexcept
    {
        using result_type = detail::from_utf32_dispatch_t<
            Cont,
            utf_32_to_8_insert_iterator<Cont>,
            utf_32_to_16_insert_iterator<Cont>,
            std::insert_iterator<Cont>>;
        return result_type(c, it);
    }

    /** Returns a back-inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf8_back_inserter(Cont & c) noexcept
    {
        using result_type = detail::from_utf8_dispatch_t<
            Cont,
            std::back_insert_iterator<Cont>,
            utf_8_to_16_back_insert_iterator<Cont>,
            utf_8_to_32_back_insert_iterator<Cont>>;
        return result_type(c);
    }

    /** Returns a back-inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf16_back_inserter(Cont & c) noexcept
    {
        using result_type = detail::from_utf16_dispatch_t<
            Cont,
            utf_16_to_8_back_insert_iterator<Cont>,
            std::back_insert_iterator<Cont>,
            utf_16_to_32_back_insert_iterator<Cont>>;
        return result_type(c);
    }

    /** Returns a back-inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf32_back_inserter(Cont & c) noexcept
    {
        using result_type = detail::from_utf32_dispatch_t<
            Cont,
            utf_32_to_8_back_insert_iterator<Cont>,
            utf_32_to_16_back_insert_iterator<Cont>,
            std::back_insert_iterator<Cont>>;
        return result_type(c);
    }

    /** Returns a front-inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf8_front_inserter(Cont & c) noexcept
    {
        using result_type = detail::from_utf8_dispatch_t<
            Cont,
            std::front_insert_iterator<Cont>,
            utf_8_to_16_front_insert_iterator<Cont>,
            utf_8_to_32_front_insert_iterator<Cont>>;
        return result_type(c);
    }

    /** Returns a front-inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf16_front_inserter(Cont & c) noexcept
    {
        using result_type = detail::from_utf16_dispatch_t<
            Cont,
            utf_16_to_8_front_insert_iterator<Cont>,
            std::front_insert_iterator<Cont>,
            utf_16_to_32_front_insert_iterator<Cont>>;
        return result_type(c);
    }

    /** Returns a front-inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    auto from_utf32_front_inserter(Cont & c) noexcept
    {
        using result_type = detail::from_utf32_dispatch_t<
            Cont,
            utf_32_to_8_front_insert_iterator<Cont>,
            utf_32_to_16_front_insert_iterator<Cont>,
            std::front_insert_iterator<Cont>>;
        return result_type(c);
    }

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    /** Returns a `utf_32_to_8_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<uint8_t> O>
    utf_32_to_8_out_iterator<O> utf_32_to_8_out(O it) noexcept
    {
        return utf_32_to_8_out_iterator<O>(it);
    }

    /** Returns a `utf_8_to_32_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<uint32_t> O>
    utf_8_to_32_out_iterator<O> utf_8_to_32_out(O it) noexcept
    {
        return utf_8_to_32_out_iterator<O>(it);
    }

    /** Returns a `utf_32_to_16_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<uint16_t> O>
    utf_32_to_16_out_iterator<O> utf_32_to_16_out(O it) noexcept
    {
        return utf_32_to_16_out_iterator<O>(it);
    }

    /** Returns a `utf_16_to_32_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<uint32_t> O>
    utf_16_to_32_out_iterator<O> utf_16_to_32_out(O it) noexcept
    {
        return utf_16_to_32_out_iterator<O>(it);
    }

    /** Returns a `utf_16_to_8_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<uint8_t> O>
    utf_16_to_8_out_iterator<O> utf_16_to_8_out(O it) noexcept
    {
        return utf_16_to_8_out_iterator<O>(it);
    }

    /** Returns a `utf_8_to_16_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<uint16_t> O>
    utf_8_to_16_out_iterator<O> utf_8_to_16_out(O it) noexcept
    {
        return utf_8_to_16_out_iterator<O>(it);
    }

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-8. */
    template<std::bidirectional_iterator I, std::sentinel_for<I> S>
    auto utf8_iterator(I first, I it, S last) noexcept
    {
        return v1::utf8_iterator(first, it, last);
    }

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-16. */
    template<std::bidirectional_iterator I, std::sentinel_for<I> S>
    auto utf16_iterator(I first, I it, S last) noexcept
    {
        return v1::utf16_iterator(first, it, last);
    }

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-32. */
    template<std::bidirectional_iterator I, std::sentinel_for<I> S>
    auto utf32_iterator(I first, I it, S last) noexcept
    {
        return v1::utf32_iterator(first, it, last);
    }

    /** Returns a inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf8_inserter(Cont & c, typename Cont::iterator it) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return std::insert_iterator<Cont>(c, it);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_8_to_16_insert_iterator<Cont>(c, it);
        } else {
            return utf_8_to_32_insert_iterator<Cont>(c, it);
        }
    }

    /** Returns a inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf16_inserter(Cont & c, typename Cont::iterator it) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_16_to_8_insert_iterator<Cont>(c, it);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return std::insert_iterator<Cont>(c, it);
        } else {
            return utf_16_to_32_insert_iterator<Cont>(c, it);
        }
    }

    /** Returns a inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf32_inserter(Cont & c, typename Cont::iterator it) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_32_to_8_insert_iterator<Cont>(c, it);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_32_to_16_insert_iterator<Cont>(c, it);
        } else {
            return std::insert_iterator<Cont>(c, it);
        }
    }

    /** Returns a back-inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf8_back_inserter(Cont & c) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return std::back_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_8_to_16_back_insert_iterator<Cont>(c);
        } else {
            return utf_8_to_32_back_insert_iterator<Cont>(c);
        }
    }

    /** Returns a back-inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf16_back_inserter(Cont & c) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_16_to_8_back_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return std::back_insert_iterator<Cont>(c);
        } else {
            return utf_16_to_32_back_insert_iterator<Cont>(c);
        }
    }

    /** Returns a back-inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf32_back_inserter(Cont & c) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_32_to_8_back_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_32_to_16_back_insert_iterator<Cont>(c);
        } else {
            return std::back_insert_iterator<Cont>(c);
        }
    }

    /** Returns a front-inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf8_front_inserter(Cont & c) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return std::front_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_8_to_16_front_insert_iterator<Cont>(c);
        } else {
            return utf_8_to_32_front_insert_iterator<Cont>(c);
        }
    }

    /** Returns a front-inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf16_front_inserter(Cont & c) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_16_to_8_front_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return std::front_insert_iterator<Cont>(c);
        } else {
            return utf_16_to_32_front_insert_iterator<Cont>(c);
        }
    }

    /** Returns a front-inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    // clang-format off
        requires requires { typename Cont::value_type; } &&
        std::is_integral_v<typename Cont::value_type>
    auto from_utf32_front_inserter(Cont & c) noexcept
    // clang-format on
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_32_to_8_front_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_32_to_16_front_insert_iterator<Cont>(c);
        } else {
            return std::front_insert_iterator<Cont>(c);
        }
    }

}}}

#endif

#endif
