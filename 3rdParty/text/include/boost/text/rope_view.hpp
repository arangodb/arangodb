// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_ROPE_VIEW_HPP
#define BOOST_TEXT_ROPE_VIEW_HPP

#include <boost/text/grapheme_iterator.hpp>
#include <boost/text/rope_fwd.hpp>
#include <boost/text/text_fwd.hpp>
#include <boost/text/unencoded_rope_view.hpp>
#include <boost/text/detail/rope_iterator.hpp>
#include <boost/text/detail/norm_collate.hpp>
#include <boost/text/detail/vector_iterator.hpp>

#include <iterator>


namespace boost { namespace text {

    template<nf Normalization, typename Char, typename String>
#if BOOST_TEXT_USE_CONCEPTS
        // clang-format off
        requires utf8_code_unit<Char> || utf16_code_unit<Char>
#endif
    struct basic_rope_view
    // clang-format on
    {
        /** The normalization form used in this `basic_rope_view`. */
        static constexpr nf normalization = Normalization;

        /** The type of code unit used in the underlying storage to which this
            view refers. */
        using char_type = Char;

        /** The type of the container used in the underlying storage of
            `basic_rope<Normalization, Char, String>`. */
        using string = String;

        /** A specialization of `basic_unencoded_rope_view` with the same
            `char_type` and `string`. */
        using unencoded_rope_view =
            basic_unencoded_rope_view<char_type, string>;

        /** A specialization of `basic_text_view` with the same
            `normalization`, `char_type`, and `string`. */
        using text_view = basic_text_view<normalization, char_type>;

        /** A specialization of `basic_text` with the same `normalization`,
            `char_type`, and `string`. */
        using text = basic_text<normalization, char_type, string>;

        /** The UTF format used in the underlying storage. */
        static constexpr format utf_format = detail::format_of<char_type>();

        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();
        static_assert(
            utf_format == format::utf8 || utf_format == format::utf16, "");

        using value_type = utf32_view<detail::rope_transcode_iterator_t<
            char_type,
            detail::const_rope_view_iterator<char_type, string>>>;
        using size_type = std::size_t;
        using iterator = grapheme_iterator<detail::rope_transcode_iterator_t<
            char_type,
            detail::const_rope_view_iterator<char_type, string>>>;
        using const_iterator = iterator;
        using reverse_iterator = stl_interfaces::reverse_iterator<iterator>;
        using const_reverse_iterator = reverse_iterator;

        using rope_iterator =
            grapheme_iterator<detail::rope_transcode_iterator_t<
                char_type,
                detail::const_vector_iterator<char_type, string>>>;
        using const_rope_iterator =
            grapheme_iterator<detail::rope_transcode_iterator_t<
                char_type,
                detail::const_vector_iterator<char_type, string>>>;

        /** Default ctor. */
        basic_rope_view() noexcept {}

        /** Constructs a `basic_rope_view` from a `text`. */
        basic_rope_view(text const & t) noexcept;

        /** Disable construction from a temporary `text`. */
        basic_rope_view(text &&) noexcept = delete;

        /** Constructs a `basic_rope_view` from a `text_view`. */
        basic_rope_view(text_view tv) noexcept;

        /** Constructs a `basic_rope_view` from a pair of
            `const_rope_iterators`.

             \pre boost::text::normalized<normalization>(first.base(),
             last.base()) */
        basic_rope_view(
            const_rope_iterator first, const_rope_iterator last) noexcept;

        /** Assignment from a `text`. */
        basic_rope_view & operator=(text const & t) noexcept;

        /** Disallow assignment from a `text` rvalue. */
        basic_rope_view & operator=(text &&) noexcept = delete;

        /** Assignment from a `text_view`. */
        basic_rope_view & operator=(text_view tv) noexcept;

        const_iterator begin() const noexcept;
        const_iterator end() const noexcept;

        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator cend() const noexcept { return end(); }

        const_reverse_iterator rbegin() const noexcept
        {
            return reverse_iterator(end());
        }
        const_reverse_iterator rend() const noexcept
        {
            return reverse_iterator(begin());
        }

        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator crend() const noexcept { return rend(); }

        bool empty() const noexcept { return begin() == end(); }

        /** Returns the number of code units controlled by `*this`, not
            including the null terminator. */
        size_type storage_code_units() const noexcept;

        /** Returns the number of graphemes in `*this`.  This operation is
            O(n). */
        size_type distance() const noexcept
        {
            return std::distance(begin(), end());
        }

        /** Swaps `*this` with `rhs`. */
        void swap(basic_rope_view & rhs) noexcept;

        /** Stream inserter; performs formatted output, in UTF-8 encoding. */
        friend std::ostream & operator<<(std::ostream & os, basic_rope_view rv)
        {
            if (os.good()) {
                auto const size = boost::text::estimated_width_of_graphemes(
                    rv.begin().base(), rv.end().base());
                detail::pad_width_before(os, size);
                if (os.good())
                    os << boost::text::as_utf8(
                        rv.begin().base().base(), rv.end().base().base());
                if (os.good())
                    detail::pad_width_after(os, size);
            }
            return os;
        }
#if defined(BOOST_TEXT_DOXYGEN) || defined(_MSC_VER)
        /** Stream inserter; performs formatted output, in UTF-16 encoding.
            Defined on Windows only. */
        friend std::wostream &
        operator<<(std::wostream & os, basic_rope_view rv)
        {
            if (os.good()) {
                auto const size = boost::text::estimated_width_of_graphemes(
                    rv.begin().base(), rv.end().base());
                detail::pad_width_before(os, size);
                if (os.good())
                    os << boost::text::as_utf16(
                        rv.begin().base().base(), rv.end().base().base());
                if (os.good())
                    detail::pad_width_after(os, size);
            }
            return os;
        }
#endif

        friend bool
        operator==(basic_rope_view lhs, basic_rope_view rhs) noexcept
        {
            return lhs.begin() == rhs.begin() && lhs.end() == rhs.end();
        }

        friend bool
        operator!=(basic_rope_view lhs, basic_rope_view rhs) noexcept
        {
            return !(lhs == rhs);
        }


        // Comparisons with text.

        friend bool operator==(basic_rope_view const & lhs, text rhs) noexcept
        {
            return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
        }

        friend bool operator==(text lhs, basic_rope_view const & rhs) noexcept
        {
            return rhs == lhs;
        }

        friend bool operator!=(basic_rope_view const & lhs, text rhs) noexcept
        {
            return !(lhs == rhs);
        }

        friend bool operator!=(text lhs, basic_rope_view const & rhs) noexcept
        {
            return !(rhs == lhs);
        }


        // Comparisons with text_view.

        friend bool
        operator==(basic_rope_view const & lhs, text_view rhs) noexcept
        {
            return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
        }

        friend bool
        operator==(text_view lhs, basic_rope_view const & rhs) noexcept
        {
            return rhs == lhs;
        }

        friend bool
        operator!=(basic_rope_view const & lhs, text_view rhs) noexcept
        {
            return !(lhs == rhs);
        }

        friend bool
        operator!=(text_view lhs, basic_rope_view const & rhs) noexcept
        {
            return !(rhs == lhs);
        }

        friend void swap(basic_rope_view & lhs, basic_rope_view & rhs)
        {
            lhs.swap(rhs);
        }

#ifndef BOOST_TEXT_DOXYGEN

    private:
        using urv_iter = detail::const_rope_view_iterator<char_type, string>;
        static iterator
        make_iter(urv_iter first, urv_iter it, urv_iter last) noexcept
        {
            return iterator{
                detail::rope_transcode_iterator_t<char_type, urv_iter>{
                    first, first, last},
                detail::rope_transcode_iterator_t<char_type, urv_iter>{
                    first, it, last},
                detail::rope_transcode_iterator_t<char_type, urv_iter>{
                    first, last, last}};
        }

        unencoded_rope_view view_;

#endif
    };

    /** Returns a collation sort key for `str`, using the given collation
        table.  Any optional settings flags will be honored, so long as they
        do not conflict with the settings on the given table.

        \note The contents of `str` will be normalized into temporary storage
        before collation if it is not normalized NFD or FCC; this is required
        by the Unicode collation algorithm. */
    template<nf Normalization, typename Char, typename String>
    text_sort_key collation_sort_key(
        basic_rope_view<Normalization, Char, String> const & str,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return detail::norm_collation_sort_key<Normalization, Char>(
            str, table, flags);
    }

    /** Creates sort keys for `str1` and `str2`, then returns the result of
        calling `compare()` on the keys.  Any optional settings flags will be
        honored, so long as they do not conflict with the settings on the
        given table.

        \note The contents of each `basic_rope_view` will be normalized into
        temporary storage before collation if it is not normalized NFD or FCC;
        this is required by the Unicode collation algorithm. */
    template<
        nf Normalization1,
        typename Char1,
        typename String1,
        nf Normalization2,
        typename Char2,
        typename String2>
    int collate(
        basic_rope_view<Normalization1, Char1, String1> const & str1,
        basic_rope_view<Normalization2, Char2, String2> const & str2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return detail::
            norm_collate<Normalization1, Char1, Normalization2, Char2>(
                str1, str2, table, flags);
    }

}}

#include <boost/text/text.hpp>
#include <boost/text/rope.hpp>
#include <boost/text/unencoded_rope_view.hpp>

namespace boost { namespace text {

    template<nf Normalization, typename Char, typename String>
    basic_rope_view<Normalization, Char, String>::basic_rope_view(
        text const & t) noexcept :
        view_(basic_string_view<char_type>(
            t.begin().base().base(), t.storage_code_units()))
    {}

    template<nf Normalization, typename Char, typename String>
    basic_rope_view<Normalization, Char, String>::basic_rope_view(
        text_view tv) noexcept :
        view_(basic_string_view<char_type>(
            tv.begin().base().base(), tv.storage_code_units()))
    {}

    template<nf Normalization, typename Char, typename String>
    basic_rope_view<Normalization, Char, String>::basic_rope_view(
        const_rope_iterator first, const_rope_iterator last) noexcept
    {
        BOOST_ASSERT(
            boost::text::normalized<normalization>(first.base(), last.base()));
        auto const lo = first.base().base() - first.base().base().vec_->begin();
        auto const hi = last.base().base() - last.base().base().vec_->begin();
        view_ = unencoded_rope_view(first.base().base().vec_, lo, hi);
    }

    template<nf Normalization, typename Char, typename String>
    basic_rope_view<Normalization, Char, String> &
    basic_rope_view<Normalization, Char, String>::operator=(
        text const & t) noexcept
    {
        view_ = basic_string_view<char_type>(
            t.begin().base().base(), t.storage_code_units());
        return *this;
    }

    template<nf Normalization, typename Char, typename String>
    basic_rope_view<Normalization, Char, String> &
    basic_rope_view<Normalization, Char, String>::operator=(
        text_view tv) noexcept
    {
        view_ = basic_string_view<char_type>(
            tv.begin().base().base(), tv.storage_code_units());
        return *this;
    }

    template<nf Normalization, typename Char, typename String>
    typename basic_rope_view<Normalization, Char, String>::const_iterator
    basic_rope_view<Normalization, Char, String>::begin() const noexcept
    {
        return make_iter(view_.begin(), view_.begin(), view_.end());
    }
    template<nf Normalization, typename Char, typename String>
    typename basic_rope_view<Normalization, Char, String>::const_iterator
    basic_rope_view<Normalization, Char, String>::end() const noexcept
    {
        return make_iter(view_.begin(), view_.end(), view_.end());
    }

    template<nf Normalization, typename Char, typename String>
    typename basic_rope_view<Normalization, Char, String>::size_type
    basic_rope_view<Normalization, Char, String>::storage_code_units()
        const noexcept
    {
        return view_.size();
    }

}}

#ifndef BOOST_TEXT_DOXYGEN

namespace std {
    template<boost::text::nf Normalization, typename Char, typename String>
    struct hash<boost::text::basic_rope_view<Normalization, Char, String>>
    {
        using argument_type =
            boost::text::basic_rope_view<Normalization, Char, String>;
        using result_type = std::size_t;
        result_type operator()(argument_type const & rv) const noexcept
        {
            return boost::text::detail::hash_grapheme_range(rv);
        }
    };
}

#endif

#if BOOST_TEXT_USE_CONCEPTS

namespace std::ranges {
    template<boost::text::nf Normalization, typename Char, typename String>
    inline constexpr bool enable_borrowed_range<
        boost::text::basic_rope_view<Normalization, Char, String>> = true;
}

#endif

#endif
