// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_UTILITY_HPP
#define BOOST_TEXT_DETAIL_UTILITY_HPP

#include <boost/text/config.hpp>

#include <boost/assert.hpp>
#include <boost/algorithm/cxx14/mismatch.hpp>

#include <ostream>

#include <cstddef>
#include <cstring>
#include <climits>


namespace boost { namespace text { namespace detail {

#ifdef BOOST_TEXT_NO_CXX14_CONSTEXPR

        inline std::ptrdiff_t strlen(char const * c_str) noexcept
        {
            return ::strlen(c_str);
        }

#else

        inline constexpr std::ptrdiff_t strlen(char const * c_str) noexcept
        {
            BOOST_ASSERT(c_str);
            std::size_t retval = 0;
            while (*c_str) {
                ++retval;
                ++c_str;
            }
            return retval;
        }

#endif

        template<typename OStream>
        void insert_fill_chars(OStream & os, std::streamsize n)
        {
            int const chunk_size = 8;
            using char_type = typename OStream::char_type;
            char_type fill_chars[chunk_size];
            std::fill_n(fill_chars, chunk_size, os.fill());
            for (; chunk_size <= n && os.good(); n -= chunk_size) {
                os.write(fill_chars, chunk_size);
            }
            if (0 < n && os.good())
                os.write(fill_chars, n);
        }

        template<typename OStream>
        void pad_width_before(OStream & os, std::streamsize size)
        {
            const bool align_left =
                (os.flags() & std::ostream::adjustfield) == std::ostream::left;
            if (align_left)
                return;
            auto const alignment_size = os.width() - size;
            insert_fill_chars(os, alignment_size);
            os.width(0);
        }

        template<typename OStream>
        void pad_width_after(OStream & os, std::streamsize size)
        {
            const bool align_left =
                (os.flags() & std::ostream::adjustfield) == std::ostream::left;
            if (!align_left)
                return;
            auto const alignment_size = os.width() - size;
            insert_fill_chars(os, alignment_size);
            os.width(0);
        }

        inline BOOST_TEXT_CXX14_CONSTEXPR char const *
        strchr(char const * first, char const * last, char c) noexcept
        {
            while (first != last) {
                if (*first == c)
                    return first;
                ++first;
            }
            return last;
        }

        inline BOOST_TEXT_CXX14_CONSTEXPR char const *
        strrchr(char const * first, char const * last, char c) noexcept
        {
            char const * it = last;
            while (first != it) {
                if (*--it == c)
                    return it;
            }
            return last;
        }

        template<typename T>
        constexpr T min_(T lhs, T rhs) noexcept
        {
            return lhs < rhs ? lhs : rhs;
        }

        template<typename T>
        constexpr T max_(T lhs, T rhs) noexcept
        {
            return lhs < rhs ? rhs : lhs;
        }

#ifdef BOOST_TEXT_NO_CXX14_CONSTEXPR

        inline int compare_impl(
            char const * l_first,
            char const * l_last,
            char const * r_first,
            char const * r_last) noexcept
        {
            auto const iters =
                algorithm::mismatch(l_first, l_last, r_first, r_last);
            if (iters.first == l_last) {
                if (iters.second == r_last)
                    return 0;
                else
                    return -1;
            } else if (iters.second == r_last) {
                return 1;
            } else if (*iters.first < *iters.second) {
                return -1;
            } else {
                return 1;
            }
        }

#else

        inline constexpr int compare_impl(
            char const * l_first,
            char const * l_last,
            char const * r_first,
            char const * r_last) noexcept
        {
            auto const l_size = l_last - l_first;
            auto const r_size = r_last - r_first;
            BOOST_ASSERT(l_size <= INT_MAX);
            BOOST_ASSERT(r_size <= INT_MAX);

            int retval = 0;

            int const size = (int)detail::min_(l_size, r_size);
            if (size != 0) {
                char const * l_it = l_first;
                char const * l_it_end = l_first + size;
                char const * r_it = r_first;
                while (l_it != l_it_end) {
                    char const l_c = *l_it;
                    char const r_c = *r_it;
                    if (l_c < r_c)
                        return -1;
                    if (r_c < l_c)
                        return 1;
                    ++l_it;
                    ++r_it;
                }
            }

            if (retval == 0) {
                if (l_size < r_size)
                    return -1;
                if (l_size == r_size)
                    return 0;
                return 1;
            }

            return retval;
        }

#endif

        template<typename Iter1, typename Iter2>
        int generalized_compare(
            Iter1 l_first, Iter1 l_last, Iter2 r_first, Iter2 r_last) noexcept
        {
            auto const iters =
                algorithm::mismatch(l_first, l_last, r_first, r_last);
            if (iters.first == l_last) {
                if (iters.second == r_last)
                    return 0;
                else
                    return -1;
            } else if (iters.second == r_last) {
                return 1;
            } else if (*iters.first < *iters.second) {
                return -1;
            } else {
                return 1;
            }
        }
}}}

#endif
