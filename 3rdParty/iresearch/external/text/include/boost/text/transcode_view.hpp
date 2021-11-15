// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_TRANSCODE_VIEW_HPP
#define BOOST_TEXT_TRANSCODE_VIEW_HPP

#include <boost/text/transcode_algorithm.hpp>
#include <boost/text/concepts.hpp>
#include <boost/text/dangling.hpp>
#include <boost/text/detail/unpack.hpp>

#include <boost/stl_interfaces/view_interface.hpp>


namespace boost { namespace text {

    namespace detail {

        // UTF-8
        template<typename Iter, typename Sentinel>
        constexpr auto make_utf8_range_(utf8_tag, Iter f, Sentinel l) noexcept
        {
            return tagged_range<utf8_tag, Iter, Sentinel>{f, l};
        }
        template<typename Iter, typename Sentinel>
        constexpr auto make_utf8_range_(utf16_tag, Iter f_, Sentinel l) noexcept
        {
            auto f = utf_16_to_8_iterator<Iter, Sentinel>(f_, f_, l);
            return tagged_range<utf8_tag, decltype(f), Sentinel>{f, l};
        }
        template<typename Iter>
        constexpr auto make_utf8_range_(utf16_tag, Iter f_, Iter l_) noexcept
        {
            auto f = utf_16_to_8_iterator<Iter>(f_, f_, l_);
            auto l = utf_16_to_8_iterator<Iter>(f_, l_, l_);
            return tagged_range<utf8_tag, decltype(f)>{f, l};
        }
        template<typename Iter, typename Sentinel>
        constexpr auto make_utf8_range_(utf32_tag, Iter f_, Sentinel l) noexcept
        {
            auto f = utf_32_to_8_iterator<Iter, Sentinel>(f_, f_, l);
            return tagged_range<utf8_tag, decltype(f), Sentinel>{f, l};
        }
        template<typename Iter>
        constexpr auto make_utf8_range_(utf32_tag, Iter f_, Iter l_) noexcept
        {
            auto f = utf_32_to_8_iterator<Iter>(f_, f_, l_);
            auto l = utf_32_to_8_iterator<Iter>(f_, l_, l_);
            return tagged_range<utf8_tag, decltype(f)>{f, l};
        }

        // UTF-16
        template<typename Iter, typename Sentinel>
        constexpr auto make_utf16_range_(utf8_tag, Iter f_, Sentinel l) noexcept
        {
            auto f = utf_8_to_16_iterator<Iter, Sentinel>(f_, f_, l);
            return tagged_range<utf16_tag, decltype(f), Sentinel>{f, l};
        }
        template<typename Iter>
        constexpr auto make_utf16_range_(utf8_tag, Iter f_, Iter l_) noexcept
        {
            auto f = utf_8_to_16_iterator<Iter>(f_, f_, l_);
            auto l = utf_8_to_16_iterator<Iter>(f_, l_, l_);
            return tagged_range<utf16_tag, decltype(f)>{f, l};
        }
        template<typename Iter, typename Sentinel>
        constexpr auto make_utf16_range_(utf16_tag, Iter f, Sentinel l) noexcept
        {
            return tagged_range<utf16_tag, Iter, Sentinel>{f, l};
        }
        template<typename Iter, typename Sentinel>
        constexpr auto
        make_utf16_range_(utf32_tag, Iter f_, Sentinel l) noexcept
        {
            auto f = utf_32_to_16_iterator<Iter, Sentinel>(f_, f_, l);
            return tagged_range<utf16_tag, decltype(f), Sentinel>{f, l};
        }
        template<typename Iter>
        constexpr auto make_utf16_range_(utf32_tag, Iter f_, Iter l_) noexcept
        {
            auto f = utf_32_to_16_iterator<Iter>(f_, f_, l_);
            auto l = utf_32_to_16_iterator<Iter>(f_, l_, l_);
            return tagged_range<utf16_tag, decltype(f)>{f, l};
        }

        // UTF-32
        template<typename Iter, typename Sentinel>
        constexpr auto make_utf32_range_(utf8_tag, Iter f_, Sentinel l) noexcept
        {
            auto f = utf_8_to_32_iterator<Iter, Sentinel>(f_, f_, l);
            return tagged_range<utf32_tag, decltype(f), Sentinel>{f, l};
        }
        template<typename Iter>
        constexpr auto make_utf32_range_(utf8_tag, Iter f_, Iter l_) noexcept
        {
            auto f = utf_8_to_32_iterator<Iter>(f_, f_, l_);
            auto l = utf_8_to_32_iterator<Iter>(f_, l_, l_);
            return tagged_range<utf32_tag, decltype(f)>{f, l};
        }
        template<typename Iter, typename Sentinel>
        constexpr auto
        make_utf32_range_(utf16_tag, Iter f_, Sentinel l) noexcept
        {
            auto f = utf_16_to_32_iterator<Iter, Sentinel>(f_, f_, l);
            return tagged_range<utf32_tag, decltype(f), Sentinel>{f, l};
        }
        template<typename Iter>
        constexpr auto make_utf32_range_(utf16_tag, Iter f_, Iter l_) noexcept
        {
            auto f = utf_16_to_32_iterator<Iter>(f_, f_, l_);
            auto l = utf_16_to_32_iterator<Iter>(f_, l_, l_);
            return tagged_range<utf32_tag, decltype(f)>{f, l};
        }
        template<typename Iter, typename Sentinel>
        constexpr auto make_utf32_range_(utf32_tag, Iter f, Sentinel l) noexcept
        {
            return tagged_range<utf32_tag, Iter, Sentinel>{f, l};
        }

        template<typename ResultType, typename Iterator, typename Sentinel>
        constexpr auto
        make_iter(Iterator first, Iterator it, Sentinel last) noexcept
            -> decltype(ResultType(first, it, last))
        {
            return ResultType(first, it, last);
        }
        template<typename ResultType>
        constexpr auto
        make_iter(ResultType first, ResultType it, ResultType last) noexcept
            -> decltype(ResultType(it))
        {
            return it;
        }
        template<typename ResultType, typename Sentinel>
        constexpr auto
        make_iter(ResultType first, ResultType it, Sentinel last) noexcept
            -> decltype(ResultType(it))
        {
            return it;
        }
        template<typename ResultType, typename Iterator>
        constexpr auto
        make_iter(Iterator first, ResultType it, ResultType last) noexcept
            -> decltype(ResultType(it))
        {
            return it;
        }
    }

    /** A view over UTF-8 code units. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<utf8_iter I, std::sentinel_for<I> S = I>
#else
    template<typename I, typename S = I>
#endif
    struct utf8_view : stl_interfaces::view_interface<utf8_view<I, S>>
    {
        using iterator = I;
        using sentinel = S;

        constexpr utf8_view() noexcept {}
        constexpr utf8_view(iterator first, sentinel last) noexcept :
            first_(detail::unpack_iterator_and_sentinel(first, last).f_),
            last_(detail::unpack_iterator_and_sentinel(first, last).l_)
        {}

        constexpr iterator begin() const noexcept
        {
            return detail::make_iter<iterator>(first_, first_, last_);
        }
        constexpr sentinel end() const noexcept
        {
            return detail::make_iter<sentinel>(first_, last_, last_);
        }

        friend constexpr bool operator==(utf8_view lhs, utf8_view rhs)
        {
            return lhs.begin() == rhs.begin() && lhs.end() == rhs.end();
        }
        friend constexpr bool operator!=(utf8_view lhs, utf8_view rhs)
        {
            return !(lhs == rhs);
        }

        /** Stream inserter; performs unformatted output, in UTF-8
            encoding. */
        friend std::ostream & operator<<(std::ostream & os, utf8_view v)
        {
            auto out = std::ostreambuf_iterator<char>(os);
            for (auto it = v.begin(); it != v.end(); ++it, ++out) {
                *out = *it;
            }
            return os;
        }
#if defined(BOOST_TEXT_DOXYGEN) || defined(_MSC_VER)
        /** Stream inserter; performs unformatted output, in UTF-16 encoding.
            Defined on Windows only. */
        friend std::wostream & operator<<(std::wostream & os, utf8_view v)
        {
            boost::text::detail::transcode_utf_8_to_16(
                v.begin(), v.end(), std::ostreambuf_iterator<wchar_t>(os));
            return os;
        }
#endif

    private:
        using iterator_t = decltype(detail::unpack_iterator_and_sentinel(
                                        std::declval<I>(), std::declval<S>())
                                        .f_);
        using sentinel_t = decltype(detail::unpack_iterator_and_sentinel(
                                        std::declval<I>(), std::declval<S>())
                                        .l_);

        iterator_t first_;
        sentinel_t last_;
    };

    /** A view over UTF-16 code units. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<utf16_iter I, std::sentinel_for<I> S = I>
#else
    template<typename I, typename S = I>
#endif
    struct utf16_view : stl_interfaces::view_interface<utf16_view<I, S>>
    {
        using iterator = I;
        using sentinel = S;

        constexpr utf16_view() noexcept {}
        constexpr utf16_view(iterator first, sentinel last) noexcept :
            first_(detail::unpack_iterator_and_sentinel(first, last).f_),
            last_(detail::unpack_iterator_and_sentinel(first, last).l_)
        {}

        constexpr iterator begin() const noexcept
        {
            return detail::make_iter<iterator>(first_, first_, last_);
        }
        constexpr sentinel end() const noexcept
        {
            return detail::make_iter<sentinel>(first_, last_, last_);
        }

        friend constexpr bool operator==(utf16_view lhs, utf16_view rhs)
        {
            return lhs.begin() == rhs.begin() && lhs.end() == rhs.end();
        }
        friend constexpr bool operator!=(utf16_view lhs, utf16_view rhs)
        {
            return !(lhs == rhs);
        }

        /** Stream inserter; performs unformatted output, in UTF-8
            encoding. */
        friend std::ostream & operator<<(std::ostream & os, utf16_view v)
        {
            boost::text::transcode_to_utf8(
                v.begin(), v.end(), std::ostreambuf_iterator<char>(os));
            return os;
        }
#if defined(BOOST_TEXT_DOXYGEN) || defined(_MSC_VER)
        /** Stream inserter; performs unformatted output, in UTF-16 encoding.
            Defined on Windows only. */
        friend std::wostream & operator<<(std::wostream & os, utf16_view v)
        {
            auto out = std::ostreambuf_iterator<wchar_t>(os);
            for (auto it = v.begin(); it != v.end(); ++it, ++out) {
                *out = *it;
            }
            return os;
        }
#endif

    private:
        using iterator_t = decltype(detail::unpack_iterator_and_sentinel(
                                        std::declval<I>(), std::declval<S>())
                                        .f_);
        using sentinel_t = decltype(detail::unpack_iterator_and_sentinel(
                                        std::declval<I>(), std::declval<S>())
                                        .l_);

        iterator_t first_;
        sentinel_t last_;
    };

    /** A view over UTF-32 code units. */
#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS
    template<utf32_iter I, std::sentinel_for<I> S = I>
#else
    template<typename I, typename S = I>
#endif
    struct utf32_view : stl_interfaces::view_interface<utf32_view<I, S>>
    {
        using iterator = I;
        using sentinel = S;

        constexpr utf32_view() noexcept {}
        constexpr utf32_view(iterator first, sentinel last) noexcept :
            first_(detail::unpack_iterator_and_sentinel(first, last).f_),
            last_(detail::unpack_iterator_and_sentinel(first, last).l_)
        {}

        constexpr iterator begin() const noexcept
        {
            return detail::make_iter<iterator>(first_, first_, last_);
        }
        constexpr sentinel end() const noexcept
        {
            return detail::make_iter<sentinel>(first_, last_, last_);
        }

        friend constexpr bool operator==(utf32_view lhs, utf32_view rhs)
        {
            return lhs.begin() == rhs.begin() && lhs.end() == rhs.end();
        }
        friend constexpr bool operator!=(utf32_view lhs, utf32_view rhs)
        {
            return !(lhs == rhs);
        }

        /** Stream inserter; performs unformatted output, in UTF-8
            encoding. */
        friend std::ostream & operator<<(std::ostream & os, utf32_view v)
        {
            boost::text::transcode_to_utf8(
                v.begin(), v.end(), std::ostreambuf_iterator<char>(os));
            return os;
        }
#if defined(BOOST_TEXT_DOXYGEN) || defined(_MSC_VER)
        /** Stream inserter; performs unformatted output, in UTF-16 encoding.
            Defined on Windows only. */
        friend std::wostream & operator<<(std::wostream & os, utf32_view v)
        {
            boost::text::transcode_to_utf16(
                v.begin(), v.end(), std::ostreambuf_iterator<wchar_t>(os));
            return os;
        }
#endif

    private:
        using iterator_t = decltype(detail::unpack_iterator_and_sentinel(
                                        std::declval<I>(), std::declval<S>())
                                        .f_);
        using sentinel_t = decltype(detail::unpack_iterator_and_sentinel(
                                        std::declval<I>(), std::declval<S>())
                                        .l_);

        iterator_t first_;
        sentinel_t last_;
    };

}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    /** Returns a `utf8_view` over the data in `[first, last)`.  The view will
        transcode the data if necessary. */
    template<typename Iter, typename Sentinel>
    constexpr auto as_utf8(Iter first, Sentinel last) noexcept
    {
        auto unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto r =
            detail::make_utf8_range_(unpacked.tag_, unpacked.f_, unpacked.l_);
        return utf8_view<decltype(r.f_), decltype(r.l_)>(r.f_, r.l_);
    }

    namespace dtl {
        template<
            typename Range,
            bool Pointer =
                detail::char_ptr<std::remove_reference_t<Range>>::value ||
                detail::_16_ptr<std::remove_reference_t<Range>>::value ||
                detail::cp_ptr<std::remove_reference_t<Range>>::value>
        struct as_utf8_dispatch
        {
            static constexpr auto call(Range r) noexcept
                -> decltype(v1::as_utf8(std::begin(r), std::end(r)))
            {
                return v1::as_utf8(std::begin(r), std::end(r));
            }
        };

        template<typename Ptr>
        struct as_utf8_dispatch<Ptr, true>
        {
            static constexpr auto call(Ptr p) noexcept
                -> decltype(v1::as_utf8(p, null_sentinel{}))
            {
                return v1::as_utf8(p, null_sentinel{});
            }
        };
    }

    /** Returns a `utf8_view` over the data in `r`.  The view will transcode
        the data if necessary.  `R` may be a null-terminated pointer, or a
        reference to such a pointer, of intergral type `T`, as long as
        `sizeof(T) == 1`. */
    template<typename Range>
    constexpr auto as_utf8(Range && r) noexcept->decltype(
        dtl::as_utf8_dispatch<Range &&>::call((Range &&) r))
    {
        return dtl::as_utf8_dispatch<Range &&>::call((Range &&) r);
    }

    /** Returns a `utf16_view` over the data in `[first, last)`.  The view
        will transcode the data if necessary. */
    template<typename Iter, typename Sentinel>
    constexpr auto as_utf16(Iter first, Sentinel last) noexcept
    {
        auto unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto r =
            detail::make_utf16_range_(unpacked.tag_, unpacked.f_, unpacked.l_);
        return utf16_view<decltype(r.f_), decltype(r.l_)>(r.f_, r.l_);
    }

    namespace dtl {
        template<
            typename Range,
            bool Pointer =
                detail::char_ptr<std::remove_reference_t<Range>>::value ||
                detail::_16_ptr<std::remove_reference_t<Range>>::value ||
                detail::cp_ptr<std::remove_reference_t<Range>>::value>
        struct as_utf16_dispatch
        {
            static constexpr auto call(Range r) noexcept
                -> decltype(v1::as_utf16(std::begin(r), std::end(r)))
            {
                return v1::as_utf16(std::begin(r), std::end(r));
            }
        };

        template<typename Ptr>
        struct as_utf16_dispatch<Ptr, true>
        {
            static constexpr auto call(Ptr p) noexcept
                -> decltype(v1::as_utf16(p, null_sentinel{}))
            {
                return v1::as_utf16(p, null_sentinel{});
            }
        };
    }

    /** Returns a `utf16_view` over the data in `r`.  The view will transcode
        the data if necessary.  `R` may be a null-terminated pointer, or a
        reference to such a pointer, of intergral type `T`, as long as
        `sizeof(T) == 2`. */
    template<typename Range>
    constexpr auto as_utf16(Range && r) noexcept->decltype(
        dtl::as_utf16_dispatch<Range &&>::call((Range &&) r))
    {
        return dtl::as_utf16_dispatch<Range &&>::call((Range &&) r);
    }

    /** Returns a `utf32_view` over the data in `[first, last)`.  The view
        will transcode the data if necessary. */
    template<typename Iter, typename Sentinel>
    constexpr auto as_utf32(Iter first, Sentinel last) noexcept
    {
        auto unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto r =
            detail::make_utf32_range_(unpacked.tag_, unpacked.f_, unpacked.l_);
        return utf32_view<decltype(r.f_), decltype(r.l_)>(r.f_, r.l_);
    }

    namespace dtl {
        template<
            typename Range,
            bool Pointer =
                detail::char_ptr<std::remove_reference_t<Range>>::value ||
                detail::_16_ptr<std::remove_reference_t<Range>>::value ||
                detail::cp_ptr<std::remove_reference_t<Range>>::value>
        struct as_utf32_dispatch
        {
            static constexpr auto call(Range r) noexcept
                -> decltype(v1::as_utf32(std::begin(r), std::end(r)))
            {
                return v1::as_utf32(std::begin(r), std::end(r));
            }
        };

        template<typename Ptr>
        struct as_utf32_dispatch<Ptr, true>
        {
            static constexpr auto call(Ptr p) noexcept
                -> decltype(v1::as_utf32(p, null_sentinel{}))
            {
                return v1::as_utf32(p, null_sentinel{});
            }
        };
    }

    /** Returns a `utf32_view` over the data in `r`.  The view will transcode
        the data if necessary.  `R` may be a null-terminated pointer, or a
        reference to such a pointer, of intergral type `T`, as long as
        `sizeof(T) == 4`. */
    template<typename Range>
    constexpr auto as_utf32(Range && r) noexcept->decltype(
        dtl::as_utf32_dispatch<Range &&>::call((Range &&) r))
    {
        return dtl::as_utf32_dispatch<Range &&>::call((Range &&) r);
    }

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    /** Returns a `utf8_view` over the data in `[first, last)`.  The view will
        transcode the data if necessary. */
    template<utf_iter I, std::sentinel_for<I> S>
    constexpr auto as_utf8(I first, S last) noexcept
    {
        auto unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto r =
            detail::make_utf8_range_(unpacked.tag_, unpacked.f_, unpacked.l_);
        return utf8_view<decltype(r.f_), decltype(r.l_)>(r.f_, r.l_);
    }

    /** Returns a `utf8_view` over the data in `r`.  The view will transcode
        the data if necessary.  If `std::remove_reference_t<R>` is not a
        pointer, the result is returned as a `borrowed_view_t`. */
    template<utf_range_like R>
    constexpr auto as_utf8(R && r) noexcept
    {
        if constexpr (std::is_pointer_v<std::remove_reference_t<R>>) {
            return boost::text::as_utf8(r, null_sentinel{});
        } else {
            auto intermediate = boost::text::as_utf8(
                std::ranges::begin(r), std::ranges::end(r));
            using result_type = borrowed_view_t<R, decltype(intermediate)>;
            return result_type{intermediate};
        }
    }

    /** Returns a `utf16_view` over the data in `[first, last)`.  The view
        will transcode the data if necessary. */
    template<utf_iter I, std::sentinel_for<I> S>
    constexpr auto as_utf16(I first, S last) noexcept
    {
        auto unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto r =
            detail::make_utf16_range_(unpacked.tag_, unpacked.f_, unpacked.l_);
        return utf16_view<decltype(r.f_), decltype(r.l_)>(r.f_, r.l_);
    }

    /** Returns a `utf16_view` over the data in `r` the data if necessary.  If
        `std::remove_reference_t<R>` is not a pointer, the result is returned
        as a `borrowed_view_t`. */
    template<utf_range_like R>
    constexpr auto as_utf16(R && r) noexcept
    {
        if constexpr (std::is_pointer_v<std::remove_reference_t<R>>) {
            return boost::text::as_utf16(r, null_sentinel{});
        } else {
            auto intermediate = boost::text::as_utf16(
                std::ranges::begin(r), std::ranges::end(r));
            using result_type = borrowed_view_t<R, decltype(intermediate)>;
            return result_type{intermediate};
        }
    }

    /** Returns a `utf32_view` over the data in `[first, last)`.  The view
        will transcode the data if necessary. */
    template<utf_iter I, std::sentinel_for<I> S>
    constexpr auto as_utf32(I first, S last) noexcept
    {
        auto unpacked = detail::unpack_iterator_and_sentinel(first, last);
        auto r =
            detail::make_utf32_range_(unpacked.tag_, unpacked.f_, unpacked.l_);
        return utf32_view<decltype(r.f_), decltype(r.l_)>(r.f_, r.l_);
    }

    /** Returns a `utf32_view` over the data in `r`.  The view will transcode
        the data if necessary.  If `std::remove_reference_t<R>` is not a
        pointer, the result is returned as a `borrowed_view_t`. */
    template<utf_range_like R>
    constexpr auto as_utf32(R && r) noexcept
    {
        if constexpr (std::is_pointer_v<std::remove_reference_t<R>>) {
            return boost::text::as_utf32(r, null_sentinel{});
        } else {
            auto intermediate = boost::text::as_utf32(
                std::ranges::begin(r), std::ranges::end(r));
            using result_type = borrowed_view_t<R, decltype(intermediate)>;
            return result_type{intermediate};
        }
    }
}}}

namespace std::ranges {
    template<boost::text::utf8_iter I, std::sentinel_for<I> S>
    inline constexpr bool enable_borrowed_range<boost::text::utf8_view<I, S>> =
        true;

    template<boost::text::utf16_iter I, std::sentinel_for<I> S>
    inline constexpr bool enable_borrowed_range<boost::text::utf16_view<I, S>> =
        true;

    template<boost::text::utf32_iter I, std::sentinel_for<I> S>
    inline constexpr bool enable_borrowed_range<boost::text::utf32_view<I, S>> =
        true;
}

#endif

#endif
