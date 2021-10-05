// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_UNPACK_HPP
#define BOOST_TEXT_DETAIL_UNPACK_HPP

#include <boost/text/transcode_iterator.hpp>


namespace boost { namespace text { namespace detail {

    struct utf8_tag
    {};
    struct utf16_tag
    {};
    struct utf32_tag
    {};

    template<typename Tag, typename Iter, typename Sentinel = Iter>
    struct tagged_range
    {
        Iter f_;
        Sentinel l_;
        Tag tag_;
    };

    template<
        typename Iter,
        typename Sentinel,
        bool UTF8 = is_char_iter<Iter>::value,
        bool UTF16 = is_16_iter<Iter>::value,
        bool UTF32 = is_cp_iter<Iter>::value>
    struct unpack_iterator_and_sentinel_impl
    {
    };

    template<typename Iter, typename Sentinel>
    struct unpack_iterator_and_sentinel_impl<Iter, Sentinel, true, false, false>
    {
        static constexpr auto call(Iter first, Sentinel last) noexcept
        {
            return tagged_range<utf8_tag, Iter, Sentinel>{first, last};
        }
    };
    template<typename Iter, typename Sentinel>
    struct unpack_iterator_and_sentinel_impl<Iter, Sentinel, false, true, false>
    {
        static constexpr auto call(Iter first, Sentinel last) noexcept
        {
            return tagged_range<utf16_tag, Iter, Sentinel>{first, last};
        }
    };
    template<typename Iter, typename Sentinel>
    struct unpack_iterator_and_sentinel_impl<Iter, Sentinel, false, false, true>
    {
        static constexpr auto call(Iter first, Sentinel last) noexcept
        {
            return tagged_range<utf32_tag, Iter, Sentinel>{first, last};
        }
    };

    template<typename Iter, typename Sentinel>
    constexpr auto
    unpack_iterator_and_sentinel(Iter first, Sentinel last) noexcept
        -> decltype(unpack_iterator_and_sentinel_impl<
                    std::remove_cv_t<Iter>,
                    std::remove_cv_t<Sentinel>>::call(first, last))
    {
        using iterator = std::remove_cv_t<Iter>;
        using sentinel = std::remove_cv_t<Sentinel>;
        return detail::unpack_iterator_and_sentinel_impl<iterator, sentinel>::
            call(first, last);
    }

    // 8 -> 32
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_8_to_32_iterator<Iter> first,
        utf_8_to_32_iterator<Iter> last) noexcept;
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_8_to_32_iterator<Iter, Sentinel> first, Sentinel last) noexcept;
    // 32 -> 8
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_32_to_8_iterator<Iter> first,
        utf_32_to_8_iterator<Iter> last) noexcept;
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_32_to_8_iterator<Iter, Sentinel> first, Sentinel last) noexcept;
    // 16 -> 32
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_16_to_32_iterator<Iter> first,
        utf_16_to_32_iterator<Iter> last) noexcept;
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_16_to_32_iterator<Iter, Sentinel> first, Sentinel last) noexcept;
    // 32 -> 16
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_32_to_16_iterator<Iter> first,
        utf_32_to_16_iterator<Iter> last) noexcept;
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_32_to_16_iterator<Iter, Sentinel> first, Sentinel last) noexcept;
    // 8 -> 16
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_8_to_16_iterator<Iter> first,
        utf_8_to_16_iterator<Iter> last) noexcept;
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_8_to_16_iterator<Iter, Sentinel> first, Sentinel last) noexcept;
    // 16 -> 8
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_16_to_8_iterator<Iter> first,
        utf_16_to_8_iterator<Iter> last) noexcept;
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_16_to_8_iterator<Iter, Sentinel> first, Sentinel last) noexcept;

    // 8 -> 32
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_8_to_32_iterator<Iter> first,
        utf_8_to_32_iterator<Iter> last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last.base());
    }
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_8_to_32_iterator<Iter, Sentinel> first, Sentinel last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last);
    }
    // 32 -> 8
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_32_to_8_iterator<Iter> first,
        utf_32_to_8_iterator<Iter> last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last.base());
    }
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_32_to_8_iterator<Iter, Sentinel> first, Sentinel last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last);
    }

    // 16 -> 32
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_16_to_32_iterator<Iter> first,
        utf_16_to_32_iterator<Iter> last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last.base());
    }
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_16_to_32_iterator<Iter, Sentinel> first, Sentinel last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last);
    }
    // 32 -> 16
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_32_to_16_iterator<Iter> first,
        utf_32_to_16_iterator<Iter> last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last.base());
    }
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_32_to_16_iterator<Iter, Sentinel> first, Sentinel last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last);
    }

    // 8 -> 16
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_8_to_16_iterator<Iter> first,
        utf_8_to_16_iterator<Iter> last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last.base());
    }
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_8_to_16_iterator<Iter, Sentinel> first, Sentinel last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last);
    }
    // 16 -> 8
    template<typename Iter>
    constexpr auto unpack_iterator_and_sentinel(
        utf_16_to_8_iterator<Iter> first,
        utf_16_to_8_iterator<Iter> last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last.base());
    }
    template<typename Iter, typename Sentinel>
    constexpr auto unpack_iterator_and_sentinel(
        utf_16_to_8_iterator<Iter, Sentinel> first, Sentinel last) noexcept
    {
        return detail::unpack_iterator_and_sentinel(first.base(), last);
    }

}}}

#endif
