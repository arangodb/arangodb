// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_GRAPHEME_VIEW_HPP
#define BOOST_TEXT_GRAPHEME_VIEW_HPP

#include <boost/text/grapheme_iterator.hpp>
#include <boost/text/transcode_algorithm.hpp>
#include <boost/text/transcode_view.hpp>

#include <boost/stl_interfaces/view_interface.hpp>


namespace boost { namespace text {

    namespace detail {
        template<typename CPIter, typename Sentinel>
        using gr_view_sentinel_t = std::conditional_t<
            std::is_same<CPIter, Sentinel>::value,
            grapheme_iterator<CPIter, Sentinel>,
            Sentinel>;
    }

    /** A view over graphemes that occur in an underlying sequence of code
        points. */
#if BOOST_TEXT_USE_CONCEPTS
    template<code_point_iter I, std::sentinel_for<I> S = I>
#else
    template<typename I, typename S = I>
#endif
    struct grapheme_view : stl_interfaces::view_interface<grapheme_view<I, S>>
    {
        using iterator = grapheme_iterator<I, S>;
        using sentinel = detail::gr_view_sentinel_t<I, S>;

        constexpr grapheme_view() : first_(), last_() {}

        /** Construct a grapheme view that covers the entirety of the view
            of graphemes that `begin()` and `end()` lie within. */
        constexpr grapheme_view(iterator first, sentinel last) :
            first_(first),
            last_(last)
        {}

        /** Construct a grapheme view that covers the entirety of the view
            of graphemes that `begin()` and `end()` lie within. */
        constexpr grapheme_view(I first, S last) :
            first_(first, first, last),
            last_(detail::make_iter<sentinel>(first, last, last))
        {}

        /** Construct a view covering a subset of the view of graphemes that
            `begin()` and `end()` lie within. */
#if BOOST_TEXT_USE_CONCEPTS
        template<code_point_iter I2>
        // clang-format off
            requires std::constructible_from<iterator, I2, I2, I2> &&
                std::constructible_from<sentinel, I2, I2, I2>
#else
        template<
            typename I2 = I,
            typename S2 = S,
            typename Enable = std::enable_if_t<std::is_same<I2, S2>::value>>
#endif
        constexpr grapheme_view(
            I2 first, I2 view_first, I2 view_last, I2 last) :
            // clang-format on
            first_(first, view_first, last),
            last_(first, view_last, last)
        {}

        constexpr iterator begin() const noexcept { return first_; }
        constexpr sentinel end() const noexcept { return last_; }

        friend constexpr bool operator==(grapheme_view lhs, grapheme_view rhs)
        {
            return lhs.begin() == rhs.begin() && lhs.end() == rhs.end();
        }
        friend constexpr bool operator!=(grapheme_view lhs, grapheme_view rhs)
        {
            return !(lhs == rhs);
        }

        /** Stream inserter; performs unformatted output, in UTF-8
            encoding. */
        friend std::ostream & operator<<(std::ostream & os, grapheme_view v)
        {
            boost::text::transcode_to_utf8(
                v.begin().base(),
                v.end().base(),
                std::ostreambuf_iterator<char>(os));
            return os;
        }
#if defined(BOOST_TEXT_DOXYGEN) || defined(_MSC_VER)
        /** Stream inserter; performs unformatted output, in UTF-16 encoding.
            Defined on Windows only. */
        friend std::wostream & operator<<(std::wostream & os, grapheme_view v)
        {
            boost::text::transcode_to_utf16(
                v.begin(), v.end(), std::ostreambuf_iterator<wchar_t>(os));
            return os;
        }
#endif

    private:
        iterator first_;
        sentinel last_;
    };

}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    /** Returns a `grapheme_view` over the data in `[first, last)`, transcoding
        the data if necessary. */
    template<typename Iter, typename Sentinel>
    constexpr auto as_graphemes(Iter first, Sentinel last) noexcept
    {
        auto unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto r =
            detail::make_utf32_range_(unpacked.tag_, unpacked.f_, unpacked.l_);
        return grapheme_view<decltype(r.f_), decltype(r.l_)>(r.f_, r.l_);
    }

    namespace dtl {
        template<
            typename Range,
            bool Pointer =
                detail::char_ptr<std::remove_reference_t<Range>>::value ||
                detail::_16_ptr<std::remove_reference_t<Range>>::value ||
                detail::cp_ptr<std::remove_reference_t<Range>>::value>
        struct as_graphemes_dispatch
        {
            static constexpr auto call(Range && r_) noexcept
            {
                auto r = boost::text::as_utf32(r_);
                return grapheme_view<decltype(r.begin()), decltype(r.end())>(
                    r.begin(), r.end());
            }
        };

        template<typename Ptr>
        struct as_graphemes_dispatch<Ptr, true>
        {
            static constexpr auto call(Ptr p) noexcept
            {
                auto r = boost::text::as_utf32(p);
                return grapheme_view<decltype(r.begin()), null_sentinel>(
                    r.begin(), null_sentinel{});
            }
        };
    }

    /** Returns a `grapheme_view` over the data in `r`, transcoding the data
        if necessary. */
    template<typename Range>
    constexpr auto as_graphemes(Range && r) noexcept->decltype(
        dtl::as_graphemes_dispatch<Range &&>::call((Range &&) r))
    {
        return dtl::as_graphemes_dispatch<Range &&>::call((Range &&) r);
    }

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    /** Returns a `grapheme_view` over the data in `[first, last)`, transcoding
        the data if necessary. */
    template<utf_iter I, std::sentinel_for<I> S>
    constexpr auto as_graphemes(I first, S last) noexcept
    {
        auto unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto r =
            detail::make_utf32_range_(unpacked.tag_, unpacked.f_, unpacked.l_);
        return grapheme_view<decltype(r.f_), decltype(r.l_)>(r.f_, r.l_);
    }

    /** Returns a `grapheme_view` over the data in `r`, transcoding the data
        if necessary. */
    template<utf_range_like R>
    constexpr auto as_graphemes(R && r) noexcept
    {
        auto intermediate = boost::text::as_utf32(r);
        return grapheme_view<
            std::ranges::iterator_t<decltype(intermediate)>,
            std::ranges::sentinel_t<decltype(intermediate)>>(
            intermediate.begin(), intermediate.end());
    }

}}}

#endif

#if BOOST_TEXT_USE_CONCEPTS

namespace std::ranges {
    template<typename CPIter, typename Sentinel>
    inline constexpr bool
        enable_borrowed_range<boost::text::grapheme_view<CPIter, Sentinel>> =
            true;
}

#endif

#endif
