// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_ROPE_HPP
#define BOOST_TEXT_ROPE_HPP

#include <boost/text/estimated_width.hpp>
#include <boost/text/grapheme.hpp>
#include <boost/text/grapheme_iterator.hpp>
#include <boost/text/normalize_algorithm.hpp>
#include <boost/text/rope_fwd.hpp>
#include <boost/text/string_view.hpp>
#include <boost/text/text_fwd.hpp>
#include <boost/text/unencoded_rope.hpp>
#include <boost/text/detail/make_string.hpp>
#include <boost/text/detail/norm_collate.hpp>

#include <iterator>


#ifndef BOOST_TEXT_DOXYGEN

#ifdef BOOST_TEXT_TESTING
#define BOOST_TEXT_CHECK_ROPE_NORMALIZATION()                                  \
    do {                                                                       \
        string str(rope_.begin(), rope_.end());                                \
        normalize<normalization>(str);                                         \
        BOOST_ASSERT(rope_ == str);                                            \
    } while (false)
#else
#define BOOST_TEXT_CHECK_ROPE_NORMALIZATION()
#endif

#endif

namespace boost { namespace text {

    template<nf Normalization, typename Char, typename String>
#if BOOST_TEXT_USE_CONCEPTS
        // clang-format off
        requires (utf8_code_unit<Char> || utf16_code_unit<Char>) &&
            std::is_same_v<Char, std::ranges::range_value_t<String>>
#endif
    struct basic_rope
    // clang-format on
    {
        /** The normalization form used in this `basic_rope`. */
        static constexpr nf normalization = Normalization;

        /** The type of code unit used in the underlying storage. */
        using char_type = Char;

        /** The type of the container used in the underlying storage. */
        using string = String;

        /** A specialization of `basic_unencoded_rope` with the same
            `char_type` and `string`. */
        using unencoded_rope = basic_unencoded_rope<char_type, string>;

        /** A specialization of `basic_text` with the same `normalization`,
            `char_type`, and `string`. */
        using text = basic_text<normalization, char_type, string>;

        /** A specialization of `std::basic_string_view` with the same
            `char_type`. */
        using string_view = basic_string_view<char_type>;

        /** A specialization of `basic_rope_view` with the same
            `normalization`, `char_type`, and `string`. */
        using rope_view = basic_rope_view<normalization, char_type, string>;

        /** The UTF format used in the underlying storage. */
        static constexpr format utf_format = detail::format_of<char_type>();

        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();
        static_assert(
            utf_format == format::utf8 || utf_format == format::utf16, "");

        using value_type = grapheme;
        using size_type = std::size_t;
        using iterator = grapheme_iterator<detail::rope_transcode_iterator_t<
            char_type,
            detail::const_vector_iterator<char_type, string>>>;
        using const_iterator = iterator;
        using reverse_iterator = stl_interfaces::reverse_iterator<iterator>;
        using const_reverse_iterator = reverse_iterator;
        using reference = typename const_iterator::reference;
        using const_reference = reference;

        /** Default ctor. */
        basic_rope() {}

        /** Constructs a `basic_rope` from a pair of iterators. */
        basic_rope(const_iterator first, const_iterator last) :
            basic_rope(first.base().base(), last.base().base())
        {}

        /** Constructs a `basic_rope` from a null-terminated string. */
        basic_rope(char_type const * c_str);

        /** Constructs a `basic_rope` from a `rope_view`. */
        explicit basic_rope(rope_view rv);

        /** Constructs a `basic_rope` from a `text`. */
        explicit basic_rope(text t);

#ifdef BOOST_TEXT_DOXYGEN

        /** Constructs a `basic_rope` from a range of `char_type`.

            This function only participates in overload resolution if
            `CURange` models the CURange concept. */
        template<typename CURange>
        explicit basic_rope(CURange const & r);

        /** Constructs a `basic_rope` from a sequence of `char_type`.

            This function only participates in overload resolution if
            `CUIter` models the CUIter concept. */
        template<typename CUIter, typename Sentinel>
        basic_rope(CUIter first, Sentinel last);

        /** Constructs a `basic_rope` from a range of graphemes.

            This function only participates in overload resolution if
            `GraphemeRangeCU` models the GraphemeRangeCU concept. */
        template<typename GraphemeRangeCU>
        explicit basic_rope(GraphemeRangeCU const & r);

        /** Constructs a `basic_rope` from a sequence of graphemes.

            This function only participates in overload resolution if
            `GraphemeIterCU` models the GraphemeIterCU concept. */
        template<typename GraphemeIterCU>
        explicit basic_rope(GraphemeIterCU first, GraphemeIterCU last);

#else

        template<typename CURange>
        explicit basic_rope(
            CURange const & r,
            detail::cu_rng_alg_ret_t<(int)utf_format, int *, CURange> = 0) :
            rope_(std::begin(r), std::end(r))
        {}

        template<typename CUIter, typename Sentinel>
        basic_rope(
            CUIter first,
            Sentinel last,
            detail::cu_iter_ret_t<(int)utf_format, void *, CUIter> = 0);

#if BOOST_TEXT_USE_CONCEPTS
        template<grapheme_range_code_unit<utf_format> R>
        explicit basic_rope(R const & r)
#else
        template<typename R>
        explicit basic_rope(
            R const & r, detail::graph_rng_alg_ret_t<int *, R> = 0)
#endif
        {
            auto str = detail::make_string<string>(
                r.begin().base().base(), r.end().base().base());
            boost::text::normalize<normalization>(str);
            rope_.insert(rope_.begin(), std::move(str));
        }

#if BOOST_TEXT_USE_CONCEPTS
        template<grapheme_iter_code_unit<utf_format> I>
        explicit basic_rope(I first, I last)
#else
        template<typename I>
        explicit basic_rope(
            I first,
            I last,
            detail::graph_iter_alg_cu_ret_t<(int)utf_format, int *, I> = 0)
#endif
        {
            auto str = detail::make_string<string>(
                first.base().base(), last.base().base());
            boost::text::normalize<normalization>(str);
            rope_.insert(rope_.begin(), std::move(str));
        }

#endif

        /** Assignment from a null-terminated string. */
        basic_rope & operator=(char_type const * c_str)
        {
            basic_rope temp(c_str);
            swap(temp);
            return *this;
        }

        /** Assignment from a `rope_view`. */
        basic_rope & operator=(rope_view rv);

        /** Assignment from a `string_view`. */
        basic_rope & operator=(string_view sv)
        {
            basic_rope temp(sv);
            swap(temp);
            return *this;
        }

        /** Assignment from a `string`. */
        basic_rope & operator=(string s)
        {
            basic_rope temp(std::move(s));
            swap(temp);
            return *this;
        }

        /** Move-assignment from a `text`. */
        basic_rope & operator=(text t);

        operator rope_view() const noexcept
        {
            return rope_view(begin(), end());
        }

        const_reference front() const noexcept { return *begin(); }
        const_reference back() const noexcept { return *rbegin(); }

        void push_back(grapheme const & g);

        template<typename CPIter>
        void push_back(grapheme_ref<CPIter> g);

        void pop_back() { erase(std::prev(end())); }


        const_iterator begin() const noexcept
        {
            return make_iter(rope_.begin(), rope_.begin(), rope_.end());
        }
        const_iterator end() const noexcept
        {
            return make_iter(rope_.begin(), rope_.end(), rope_.end());
        }

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

        /** Returns true iff `begin() == end()`. */
        bool empty() const noexcept { return rope_.empty(); }

        /** Returns the number of code units controlled by `*this`, not
            including the null terminator. */
        size_type storage_code_units() const noexcept { return rope_.size(); }

        /** Returns the number of graphemes in `*this`.  This operation is
            O(n). */
        size_type distance() const noexcept
        {
            return std::distance(begin(), end());
        }

        /** Returns the maximum size in code units a `basic_rope` can have. */
        size_type max_code_units() const noexcept { return PTRDIFF_MAX; }

        /** Returns true if `*this` and `rhs` contain the same root node
            pointer.  This is useful when you want to check for equality
            between two `basic_rope`s that are likely to have originated from
            the same initial `basic_rope`, and may have since been mutated. */
        bool equal_root(basic_rope rhs) const noexcept
        {
            return rope_.equal_root(rhs.rope_);
        }

        /** Clear. */
        void clear() noexcept { rope_.clear(); }

        /** Erases the portion of `*this` delimited by `[first, last)`.

            \pre first <= last */
        replace_result<const_iterator>
        erase(const_iterator first, const_iterator last)
        {
            auto const rope_first = first.base().base();
            auto const rope_last = last.base().base();
            auto const retval = detail::erase_impl<true, normalization>(
                rope_, rope_first, rope_last);
            return mutation_result(retval);
        }

        /** Erases the grapheme at position `at`.

            \pre at != end() */
        replace_result<const_iterator> erase(const_iterator at)
        {
            BOOST_ASSERT(at != end());
            return erase(at, std::next(at));
        }

        /** Replaces the portion of *this delimited by `[first1, last1)` with
            the sequence `[first2, last2)`.

            \pre !std::less(first1.base().base(), begin().base().base()) &&
            !std::less(end().base().base(), last1.base().base()) */
        replace_result<const_iterator> replace(
            const_iterator first1,
            const_iterator last1,
            const_iterator first2,
            const_iterator last2)
        {
            return replace_impl(
                first1,
                last1,
                first2.base().base(),
                last2.base().base(),
                insertion_normalized);
        }

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `c_str`.

            \pre !std::less(first.base().base(), begin().base().base()) &&
            !std::less(end().base().base(), last.base().base()) */
        replace_result<const_iterator> replace(
            const_iterator first, const_iterator last, char_type const * c_str)
        {
            return replace(first, last, string_view(c_str));
        }

        /** Replaves the portion of `*this` delimited by `[first, last)` with
            `new_substr`.

            \pre !std::less(first.base().base(), begin().base().base()) &&
            !std::less(end().base().base(), last.base().base()) */
        replace_result<const_iterator> replace(
            const_iterator first, const_iterator last, string_view new_substr)
        {
            return replace_impl(
                first,
                last,
                new_substr.begin(),
                new_substr.end(),
                insertion_not_normalized);
        }

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `new_substr`.

            \pre !std::less(first.base().base(), begin().base().base()) &&
            !std::less(end().base().base(), last.base().base()) */
        replace_result<const_iterator> replace(
            const_iterator first, const_iterator last, rope_view new_substr);

#ifdef BOOST_TEXT_DOXYGEN

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `r`.

            This function only participates in overload resolution if
            `CURange` models the CURange concept.

            \pre !std::less(first.base().base(), begin().base().base()) &&
            !std::less(end().base().base(), last.base().base()) */
        template<typename CURange>
        replace_result<const_iterator>
        replace(const_iterator first, const_iterator last, CURange const & r);

        /** Replaces the portion of `*this` delimited by `[first1, last1)`
            with `[first2, last2)`.

            This function only participates in overload resolution if `CUIter`
            models the CUIter concept.

            \pre !std::less(first1.base().base(), begin().base().base()) &&
            !std::less(end().base().base(), last1.base().base()) */
        template<typename CUIter>
        replace_result<const_iterator> replace(
            const_iterator first1,
            const_iterator last1,
            CUIter first2,
            CUIter last2);

#else

        template<typename CURange>
        auto
        replace(const_iterator first, const_iterator last, CURange const & r)
            -> detail::cu_rng_alg_ret_t<
                (int)utf_format,
                replace_result<const_iterator>,
                CURange>
        {
            return replace(first, last, std::begin(r), std::end(r));
        }

        template<typename CUIter>
        auto replace(
            const_iterator first1,
            const_iterator last1,
            CUIter first2,
            CUIter last2)
            -> detail::cu_iter_ret_t<
                (int)utf_format,
                replace_result<const_iterator>,
                CUIter>
        {
            return replace_impl(
                first1, last1, first2, last2, insertion_not_normalized);
        }

#endif

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `g`. */
        replace_result<const_iterator>
        replace(const_iterator first, const_iterator last, grapheme const & g);

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `g`. */
        template<typename CPIter>
        replace_result<const_iterator> replace(
            const_iterator first, const_iterator last, grapheme_ref<CPIter> g);

        /** Inserts the sequence [first, last) into `*this` starting at position
            `at`. */
        replace_result<const_iterator>
        insert(const_iterator at, const_iterator first, const_iterator last)
        {
            return replace(at, at, first, last);
        }

        /** Inserts the sequence of char_type from `x` into `*this` starting
            at position `at`. */
        template<typename T>
        auto insert(const_iterator at, T const & x)
            -> decltype(replace(at, at, x))
        {
            return replace(at, at, x);
        }

        /** Inserts the sequence `[first, last)` into `*this` starting at
            position `at`. */
        template<typename I>
        auto insert(const_iterator at, I first, I last)
            -> decltype(replace(at, at, first, last))
        {
            return replace(at, at, first, last);
        }
 
        /** Assigns the sequence `[first, last)` to `*this`. */
        void assign(const_iterator first, const_iterator last)
        {
            replace(begin(), end(), first, last);
        }

        /** Assigns the sequence of `char_type` from `x` to `*this`. */
        template<typename T>
        auto assign(T const & x)
            -> decltype(replace(begin(), end(), x), (void)0)
        {
            replace(begin(), end(), x);
        }

        /** Assigns the sequence `[first, last)` to `*this`. */
        template<typename I>
        auto assign(I first, I last)
            -> decltype(replace(begin(), end(), first, last), (void)0)
        {
            replace(begin(), end(), first, last);
        }

        /** Appends the sequence `[first, last)` to `*this`. */
        void append(const_iterator first, const_iterator last)
        {
            replace(end(), end(), first, last);
        }

        /** Appends the sequence of `char_type` from `x` to `*this`. */
        template<typename T>
        auto append(T const & x) -> decltype(replace(end(), end(), x), (void)0)
        {
            replace(end(), end(), x);
        }

        /** Appends the sequence `[first, last)` to `*this`. */
        template<typename I>
        auto append(I first, I last)
            -> decltype(replace(end(), end(), first, last), (void)0)
        {
            replace(end(), end(), first, last);
        }

        /** Swaps `*this` with `rhs`. */
        void swap(basic_rope & rhs) noexcept { rope_.swap(rhs.rope_); }

        /** Removes and returns the underlying `unencoded_rope` from
            `*this`. */
        unencoded_rope extract() && noexcept { return std::move(rope_); }

        /** Replaces the underlying `unencoded_rope` in `*this`.

            \pre ur is in normalization form `normalization`. */
        void replace(unencoded_rope && ur) noexcept { rope_ = std::move(ur); }

        /** Appends `x` to `*this`.  `T` may be any type for which `*this = x`
            is well-formed. */
        template<typename T>
        auto operator+=(T && x) -> decltype(*this = std::forward<T>(x))
        {
            insert(end(), std::forward<T>(x));
            return *this;
        }

        /** Stream inserter; performs formatted output, in UTF-8 encoding. */
        friend std::ostream &
        operator<<(std::ostream & os, basic_rope const & r)
        {
            if (os.good()) {
                auto const size = boost::text::estimated_width_of_graphemes(
                    r.begin().base(), r.end().base());
                detail::pad_width_before(os, size);
                if (os.good())
                    os << boost::text::as_utf8(r.rope_);
                if (os.good())
                    detail::pad_width_after(os, size);
            }
            return os;
        }
#if defined(BOOST_TEXT_DOXYGEN) || defined(_MSC_VER)
        /** Stream inserter; performs formatted output, in UTF-16 encoding.
            Defined on Windows only. */
        friend std::wostream &
        operator<<(std::wostream & os, basic_rope const & r)
        {
            if (os.good()) {
                auto const size = boost::text::estimated_width_of_graphemes(
                    r.begin().base(), r.end().base());
                detail::pad_width_before(os, size);
                if (os.good())
                    os << boost::text::as_utf16(r.rope_);
                if (os.good())
                    detail::pad_width_after(os, size);
            }
            return os;
        }
#endif

        friend bool
        operator==(basic_rope const & lhs, basic_rope const & rhs) noexcept
        {
            return algorithm::equal(
                lhs.begin().base().base(),
                lhs.end().base().base(),
                rhs.begin().base().base(),
                rhs.end().base().base());
        }

        friend bool
        operator!=(basic_rope const & lhs, basic_rope const & rhs) noexcept
        {
            return !(lhs == rhs);
        }


        // Comparisons with rope_view.

        friend bool operator==(basic_rope const & lhs, rope_view rhs) noexcept
        {
            return algorithm::equal(
                lhs.begin().base().base(),
                lhs.end().base().base(),
                rhs.begin().base().base(),
                rhs.end().base().base());
        }

        friend bool operator==(rope_view lhs, basic_rope const & rhs) noexcept
        {
            return rhs == lhs;
        }

        friend bool operator!=(basic_rope const & lhs, rope_view rhs) noexcept
        {
            return !(lhs == rhs);
        }

        friend bool operator!=(rope_view lhs, basic_rope const & rhs) noexcept
        {
            return !(rhs == lhs);
        }


        // Comparisons with char_type const *.

        friend bool
        operator==(basic_rope const & lhs, char_type const * rhs) noexcept
        {
            return boost::text::equal(
                lhs.begin(), lhs.end(), rhs, null_sentinel{});
        }

        friend bool
        operator==(char_type const * lhs, basic_rope const & rhs) noexcept
        {
            return rhs == lhs;
        }

        friend bool
        operator!=(basic_rope const & lhs, char_type const * rhs) noexcept
        {
            return !(lhs == rhs);
        }

        friend bool
        operator!=(char_type const * lhs, basic_rope const & rhs) noexcept
        {
            return !(rhs == lhs);
        }


        // Comparisons with text.

        friend bool operator==(text const & lhs, basic_rope rhs) noexcept
        {
            return algorithm::equal(
                lhs.begin().base().base(),
                lhs.end().base().base(),
                rhs.begin().base().base(),
                rhs.end().base().base());
        }
        friend bool operator==(basic_rope lhs, text const & rhs) noexcept
        {
            return rhs == lhs;
        }

        friend bool operator!=(text const & lhs, basic_rope rhs) noexcept
        {
            return !(lhs == rhs);
        }
        friend bool operator!=(basic_rope lhs, text const & rhs) noexcept
        {
            return !(lhs == rhs);
        }


        // Comparisons with text_view.

#if BOOST_TEXT_USE_CONCEPTS
        template<typename Char2>
        // clang-format off
            requires std::is_same_v<Char2, char_type>
        friend bool
        // clang-format on
#else
        template<typename Char2>
        friend std::enable_if_t<std::is_same<Char2, char_type>::value, bool>
#endif
        operator==(
            basic_text_view<Normalization, Char2> const & lhs,
            basic_rope rhs) noexcept
        {
            return algorithm::equal(
                lhs.begin().base().base(),
                lhs.end().base().base(),
                rhs.begin().base().base(),
                rhs.end().base().base());
        }
#if BOOST_TEXT_USE_CONCEPTS
        template<typename Char2>
        // clang-format off
            requires std::is_same_v<Char2, char_type>
        friend bool
        // clang-format on
#else
        template<typename Char2>
        friend std::enable_if_t<std::is_same<Char2, char_type>::value, bool>
#endif
        operator==(
            basic_rope lhs,
            basic_text_view<Normalization, Char2> const & rhs) noexcept
        {
            return rhs == lhs;
        }

#if BOOST_TEXT_USE_CONCEPTS
        template<typename Char2>
        // clang-format off
            requires std::is_same_v<Char2, char_type>
        friend bool
        // clang-format on
#else
        template<typename Char2>
        friend std::enable_if_t<std::is_same<Char2, char_type>::value, bool>
#endif
        operator!=(
            basic_text_view<Normalization, Char2> const & lhs,
            basic_rope rhs) noexcept
        {
            return !(lhs == rhs);
        }
#if BOOST_TEXT_USE_CONCEPTS
        template<typename Char2>
        // clang-format off
            requires std::is_same_v<Char2, char_type>
        friend bool
        // clang-format on
#else
        template<typename Char2>
        friend std::enable_if_t<std::is_same<Char2, char_type>::value, bool>
#endif
        operator!=(
            basic_rope lhs,
            basic_text_view<Normalization, Char2> const & rhs) noexcept
        {
            return !(lhs == rhs);
        }


        // Generic comparisons.

#if BOOST_TEXT_USE_CONCEPTS

        /** Returns true iff `lhs` == `rhs`, where `rhs` is an object for which
            `lhs = rhs` is well-formed. */
        template<typename T>
        friend bool operator==(basic_rope const & lhs, T const & rhs)
            // clang-format off
            requires requires { lhs = rhs; } &&
                (!std::is_same_v<T, basic_rope>) &&
                (!std::is_same_v<T, text>)
        // clang-format on
        {
            return algorithm::equal(
                std::ranges::begin(lhs),
                std::ranges::end(lhs),
                std::ranges::begin(rhs),
                std::ranges::end(rhs));
        }

        /** Returns true iff `lhs` == `rhs`, where `rhs` is an object for which
            `rhs = lhs` is well-formed. */
        template<typename T>
        friend bool operator==(T const & lhs, basic_rope const & rhs)
            // clang-format off
            requires requires { rhs = lhs; } &&
                (!std::is_same_v<T, basic_rope>) &&
                (!std::is_same_v<T, text>)
        // clang-format on
        {
            return rhs == lhs;
        }

        /** Returns true iff `lhs` != `rhs`, where `rhs` is an object for which
            `lhs = rhs` is well-formed. */
        template<typename T>
        friend bool operator!=(basic_rope const & lhs, T const & rhs)
            // clang-format off
            requires requires { lhs = rhs; } &&
                (!std::is_same_v<T, basic_rope>) &&
                (!std::is_same_v<T, text>)
        // clang-format on
        {
            return !(lhs == rhs);
        }

        /** Returns true iff `lhs` != `rhs`, where `rhs` is an object for which
            `rhs = lhs` is well-formed. */
        template<typename T>
        friend bool operator!=(T const & lhs, basic_rope const & rhs)
            // clang-format off
            requires requires { rhs = lhs; } &&
                (!std::is_same_v<T, basic_rope>) &&
                (!std::is_same_v<T, text>)
        // clang-format on
        {
            return !(lhs == rhs);
        }

#else

        /** Returns true iff `lhs` == `rhs`, where `rhs` is an object for which
            `lhs = rhs` is well-formed. */
        template<typename T>
        friend auto operator==(basic_rope const & lhs, T const & rhs)
            -> std::enable_if_t<
                !std::is_same<T, basic_rope>::value &&
                    !std::is_same<T, text>::value,
                decltype(lhs = rhs, true)>
        {
            return algorithm::equal(
                std::begin(lhs), std::end(lhs), std::begin(rhs), std::end(rhs));
        }

        /** Returns true iff `lhs` == `rhs`, where `rhs` is an object for which
            `rhs = lhs` is well-formed. */
        template<typename T>
        friend auto operator==(T const & lhs, basic_rope const & rhs)
            -> std::enable_if_t<
                !std::is_same<T, basic_rope>::value &&
                    !std::is_same<T, text>::value,
                decltype(rhs = lhs, true)>
        {
            return rhs == lhs;
        }

        /** Returns true iff `lhs` != `rhs`, where `rhs` is an object for which
            `lhs = rhs` is well-formed. */
        template<typename T>
        friend auto operator!=(basic_rope const & lhs, T const & rhs)
            -> std::enable_if_t<
                !std::is_same<T, basic_rope>::value &&
                    !std::is_same<T, text>::value,
                decltype(lhs = rhs, true)>
        {
            return !(lhs == rhs);
        }

        /** Returns true iff `lhs` != `rhs`, where `rhs` is an object for which
            `rhs = lhs` is well-formed. */
        template<typename T>
        friend auto operator!=(T const & lhs, basic_rope const & rhs)
            -> std::enable_if_t<
                !std::is_same<T, basic_rope>::value &&
                    !std::is_same<T, text>::value,
                decltype(rhs = lhs, true)>
        {
            return !(lhs == rhs);
        }

#endif

        friend void swap(basic_rope & lhs, basic_rope & rhs) { lhs.swap(rhs); }

#ifndef BOOST_TEXT_DOXYGEN

    private:
        using ur_iter = detail::const_vector_iterator<char_type, string>;

        static const_iterator
        make_iter(ur_iter first, ur_iter it, ur_iter last) noexcept
        {
            return const_iterator{
                detail::rope_transcode_iterator_t<char_type, ur_iter>{
                    first, first, last},
                detail::rope_transcode_iterator_t<char_type, ur_iter>{
                    first, it, last},
                detail::rope_transcode_iterator_t<char_type, ur_iter>{
                    first, last, last}};
        }

        using utf32_iter =
            detail::rope_transcode_iterator_t<char_type, ur_iter>;

        template<typename Iter>
        replace_result<const_iterator>
        mutation_result(replace_result<Iter> rope_replacement)
        {
            auto const rope_first = rope_.begin();
            auto const rope_lo =
                rope_first + (rope_replacement.begin() - rope_.begin());
            auto const rope_hi =
                rope_first + (rope_replacement.end() - rope_.begin());
            auto const rope_last = rope_.end();

            // The insertion that just happened might be merged into the CP or
            // grapheme ending at the offset of the inserted char_type(s); if
            // so, back up and return an iterator to that.
            auto lo_cp_it = utf32_iter(rope_first, rope_lo, rope_last);
            auto const lo_grapheme_it = boost::text::prev_grapheme_break(
                begin().base(), lo_cp_it, end().base());

            // The insertion that just happened might be merged into the CP or
            // grapheme starting at the offset of the inserted char_type(s); if
            // so, move up and return an iterator to that.
            auto hi_cp_it = utf32_iter(rope_first, rope_hi, rope_last);
            auto hi_grapheme_it = hi_cp_it;
            if (!boost::text::at_grapheme_break(
                    begin().base(), hi_cp_it, end().base())) {
                hi_grapheme_it =
                    boost::text::next_grapheme_break(hi_cp_it, end().base());
            }

            return {
                make_iter(
                    begin().base().base(),
                    lo_grapheme_it.base(),
                    end().base().base()),
                make_iter(
                    begin().base().base(),
                    hi_grapheme_it.base(),
                    end().base().base())};
        }

        template<typename CUIter>
        replace_result<const_iterator> insert_impl(
            const_iterator at,
            CUIter first,
            CUIter last,
            insertion_normalization insertion_norm)
        {
            if (first == last)
                return {at, at};
            auto const rope_at = at.base().base();
            auto const insertion = boost::text::as_utf32(first, last);
            auto const retval = detail::replace_impl<true, normalization>(
                rope_,
                rope_at,
                rope_at,
                insertion.begin(),
                insertion.end(),
                insertion_norm);
            return mutation_result(retval);
        }

        template<typename CUIter>
        replace_result<const_iterator> replace_impl(
            const_iterator first1,
            const_iterator last1,
            CUIter first2,
            CUIter last2,
            insertion_normalization insertion_norm)
        {
            auto const insertion = boost::text::as_utf32(first2, last2);
            auto const retval = detail::replace_impl<true, normalization>(
                rope_,
                first1.base().base(),
                last1.base().base(),
                insertion.begin(),
                insertion.end(),
                insertion_norm);
            return mutation_result(retval);
        }

        unencoded_rope rope_;

        friend rope_view;

#endif // Doxygen
    };

    /** Returns a collation sort key for `str`, using the given collation
        table.  Any optional settings flags will be honored, so long as they
        do not conflict with the settings on the given table.

        \note The contents of `str` will be normalized into temporary storage
        before collation if it is not normalized NFD or FCC; this is required
        by the Unicode collation algorithm. */
    template<nf Normalization, typename Char, typename String>
    text_sort_key collation_sort_key(
        basic_rope<Normalization, Char, String> const & str,
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

        \note The contents of each `basic_rope` will be normalized into
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
        basic_rope<Normalization1, Char1, String1> const & str1,
        basic_rope<Normalization2, Char2, String2> const & str2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return detail::
            norm_collate<Normalization1, Char1, Normalization2, Char2>(
                str1, str2, table, flags);
    }

}}

#include <boost/text/text.hpp>
#include <boost/text/rope_view.hpp>
#include <boost/text/normalize_string.hpp>
#include <boost/text/detail/rope_iterator.hpp>

#ifndef BOOST_TEXT_DOXYGEN

namespace boost { namespace text {

    template<nf Normalization, typename Char, typename String>
    basic_rope<Normalization, Char, String>::basic_rope(
        char_type const * c_str) :
        rope_(text(c_str).extract())
    {}

    template<nf Normalization, typename Char, typename String>
    basic_rope<Normalization, Char, String>::basic_rope(rope_view rv) :
        rope_(rv.begin().base().base(), rv.end().base().base())
    {}

    template<nf Normalization, typename Char, typename String>
    basic_rope<Normalization, Char, String>::basic_rope(text t) :
        rope_(std::move(t).extract())
    {}

    template<nf Normalization, typename Char, typename String>
    template<typename CUIter, typename Sentinel>
    basic_rope<Normalization, Char, String>::basic_rope(
        CUIter first,
        Sentinel last,
        detail::cu_iter_ret_t<(int)utf_format, void *, CUIter>) :
        rope_(text(first, last).extract())
    {}

    template<nf Normalization, typename Char, typename String>
    basic_rope<Normalization, Char, String> &
    basic_rope<Normalization, Char, String>::operator=(rope_view rv)
    {
        basic_rope temp(rv);
        swap(temp);
        return *this;
    }

    template<nf Normalization, typename Char, typename String>
    basic_rope<Normalization, Char, String> &
    basic_rope<Normalization, Char, String>::operator=(text t)
    {
        basic_rope temp(std::move(t));
        swap(temp);
        return *this;
    }

    template<nf Normalization, typename Char, typename String>
    void basic_rope<Normalization, Char, String>::push_back(grapheme const & g)
    {
        replace(end(), end(), g);
    }

    template<nf Normalization, typename Char, typename String>
    template<typename CPIter>
    void
    basic_rope<Normalization, Char, String>::push_back(grapheme_ref<CPIter> g)
    {
        replace(end(), end(), g);
    }

    template<nf Normalization, typename Char, typename String>
    replace_result<
        typename basic_rope<Normalization, Char, String>::const_iterator>
    basic_rope<Normalization, Char, String>::replace(
        const_iterator first, const_iterator last, rope_view new_substr)
    {
        return replace_impl(
            first,
            last,
            new_substr.begin().base().base(),
            new_substr.end().base().base(),
            insertion_normalized);
    }

    template<nf Normalization, typename Char, typename String>
    replace_result<
        typename basic_rope<Normalization, Char, String>::const_iterator>
    basic_rope<Normalization, Char, String>::replace(
        const_iterator first, const_iterator last, grapheme const & g)
    {
        return replace(first, last, grapheme_ref<grapheme::const_iterator>(g));
    }

    template<nf Normalization, typename Char, typename String>
    template<typename CPIter>
    replace_result<
        typename basic_rope<Normalization, Char, String>::const_iterator>
    basic_rope<Normalization, Char, String>::replace(
        const_iterator first, const_iterator last, grapheme_ref<CPIter> g)
    {
        if (g.empty() && first == last)
            return replace_result<const_iterator>{first, first};
        std::array<char_type, 128> buf;
        auto out =
            boost::text::transcode_to_utf8(g.begin(), g.end(), buf.data());
        return replace_impl(
            first, last, buf.data(), out, insertion_not_normalized);
    }

}}

#endif

namespace boost { namespace text {

#if BOOST_TEXT_USE_CONCEPTS

    /** Creates a new `basic_rope` object that is the concatenation of `t` and
       some object `x` for which `r = x` is well-formed. */
    template<nf Normalization, typename Char, typename String, typename T>
    basic_rope<Normalization, Char, String>
    operator+(basic_rope<Normalization, Char, String> r, T const & x)
        // clang-format off
        requires requires { r = x; }
    // clang-format on
    {
        r.insert(r.end(), x);
        return r;
    }

    /** Creates a new `basic_rope` object that is the concatenation
        of `x` and `t`, where `x` is an object for which `r = x` is
        well-formed. */
    template<nf Normalization, typename Char, typename String, typename T>
    basic_rope<Normalization, Char, String>
    operator+(T const & x, basic_rope<Normalization, Char, String> r)
        // clang-format off
        requires requires { r = x; } &&
            (!std::is_same_v<T, basic_rope<Normalization, Char, String>>)
    // clang-format on
    {
        r.insert(r.begin(), x);
        return r;
    }

#else

    /** Creates a new `basic_rope` object that is the concatenation
        of `t` and some object `x` for which `r = x` is well-formed. */
    template<nf Normalization, typename Char, typename String, typename T>
    auto operator+(basic_rope<Normalization, Char, String> r, T const & x)
        -> decltype(r = x, basic_rope<Normalization, Char, String>{})
    {
        r.insert(r.end(), x);
        return r;
    }

    /** Creates a new `basic_rope` object that is the concatenation
        of `x` and `t`, where `x` is an object for which `r = x` is
        well-formed. */
    template<nf Normalization, typename Char, typename String, typename T>
    auto operator+(T const & x, basic_rope<Normalization, Char, String> r)
        -> std::enable_if_t<
            !std::is_same<T, basic_rope<Normalization, Char, String>>::value,
            decltype(r = x, basic_rope<Normalization, Char, String>{})>
    {
        r.insert(r.begin(), x);
        return r;
    }

#endif


    template<nf Normalization, typename Char, typename String>
    replace_result<typename basic_text<Normalization, Char, String>::iterator>
    basic_text<Normalization, Char, String>::replace(
        const_iterator first, const_iterator last, rope_view new_substr)
    {
        return replace_impl(
            first,
            last,
            new_substr.begin().base().base(),
            new_substr.end().base().base(),
            insertion_normalized);
    }

}}

#ifndef BOOST_TEXT_DOXYGEN

namespace std {
    template<boost::text::nf Normalization, typename Char, typename String>
    struct hash<boost::text::basic_rope<Normalization, Char, String>>
    {
        using argument_type =
            boost::text::basic_rope<Normalization, Char, String>;
        using result_type = std::size_t;
        result_type operator()(argument_type const & r) const noexcept
        {
            return boost::text::detail::hash_grapheme_range(r);
        }
    };
}

#endif

#endif
