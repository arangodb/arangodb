// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_UNENCODED_ROPE_HPP
#define BOOST_TEXT_UNENCODED_ROPE_HPP

#include <boost/text/algorithm.hpp>
#include <boost/text/estimated_width.hpp>
#include <boost/text/segmented_vector.hpp>
#include <boost/text/transcode_view.hpp>
#include <boost/text/unencoded_rope_fwd.hpp>
#include <boost/text/detail/iterator.hpp>
#include <boost/text/detail/rope.hpp>
#include <boost/text/detail/rope_iterator.hpp>

#include <boost/algorithm/cxx14/equal.hpp>

#ifdef BOOST_TEXT_TESTING
#include <iostream>
#else
#include <iosfwd>
#endif

#include <string>


namespace boost { namespace text {

    template<typename Char, typename String>
#if BOOST_TEXT_USE_CONCEPTS
    // clang-format off
        requires std::is_same_v<Char, std::ranges::range_value_t<String>>
#endif
    struct basic_unencoded_rope
        // clang-format on
        : boost::stl_interfaces::sequence_container_interface<
              basic_unencoded_rope<Char, String>,
              boost::stl_interfaces::element_layout::discontiguous>
    {
        using value_type = Char;
        using pointer = value_type *;
        using const_pointer = value_type const *;
        using reference = value_type const &;
        using const_reference = reference;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using iterator = detail::const_vector_iterator<value_type, String>;
        using const_iterator = iterator;
        using reverse_iterator = stl_interfaces::reverse_iterator<iterator>;
        using const_reverse_iterator = reverse_iterator;

        using string = String;
        using string_view = basic_string_view<Char>;

        /** The specialization of `unencoded_rope_view` with the same
            `value_type` and `string`. */
        using unencoded_rope_view =
            basic_unencoded_rope_view<value_type, string>;

        /** Default ctor.

            \post size() == 0 && begin() == end() */
        basic_unencoded_rope() noexcept {}

        basic_unencoded_rope(basic_unencoded_rope const &) = default;
        basic_unencoded_rope(basic_unencoded_rope &&) noexcept = default;

        /** Constructs a basic_unencoded_rope from a null-terminated
            string. */
        basic_unencoded_rope(value_type const * c_str)
        {
            seg_vec_.insert(begin(), string(c_str));
        }

        /** Constructs a `basic_unencoded_rope` from a
            `basic_unencoded_rope_view`. */
        explicit basic_unencoded_rope(unencoded_rope_view rv);

        /** Move-constructs a `basic_unencoded_rope` from a `string`. */
        explicit basic_unencoded_rope(string && s)
        {
            seg_vec_.insert(begin(), std::move(s));
        }

        /** Constructs a `basic_unencoded_rope` from a range of
            `value_type` elements. */
#if BOOST_TEXT_USE_CONCEPTS
        template<std::ranges::range R>
        // clang-format off
            requires
            std::convertible_to<std::ranges::range_reference_t<R>, value_type>
#else
        template<typename R>
#endif
        explicit basic_unencoded_rope(R const & r)
        // clang-format on
        {
            insert(begin(), r);
        }

        /** Constructs a `basic_unencoded_rope` from a sequence of
            `value_type`. */
#if BOOST_TEXT_USE_CONCEPTS
        template<std::input_iterator I, std::sentinel_for<I> S>
        // clang-format off
            requires std::convertible_to<std::iter_reference_t<I>, value_type>
#else
        template<typename I, typename S>
#endif
        basic_unencoded_rope(I first, S last)
        // clang-format on
        {
            insert(begin(), first, last);
        }

        basic_unencoded_rope &
        operator=(basic_unencoded_rope const &) = default;
        basic_unencoded_rope &
        operator=(basic_unencoded_rope &&) noexcept = default;

        /** Assignment from a `basic_unencoded_rope_view`. */
        basic_unencoded_rope & operator=(unencoded_rope_view rv);

        /** Move-assignment from a `string`. */
        basic_unencoded_rope & operator=(string s)
        {
            basic_unencoded_rope temp(std::move(s));
            swap(temp);
            return *this;
        }

        /** Assignment from a null-terminated string. */
        basic_unencoded_rope & operator=(value_type const * c_str)
        {
            basic_unencoded_rope temp(c_str);
            swap(temp);
            return *this;
        }

        const_iterator begin() noexcept { return seg_vec_.begin(); }
        const_iterator end() noexcept { return seg_vec_.end(); }

        size_type max_size() const noexcept { return seg_vec_.max_size(); }

        template<typename... Args>
        const_reference emplace_front(Args &&... args)
        {
            return *emplace(begin(), (Args &&) args...);
        }
        template<typename... Args>
        const_reference emplace_back(Args &&... args)
        {
            return *emplace(end(), (Args &&) args...);
        }
        template<typename... Args>
        const_iterator emplace(const_iterator at, Args &&... args)
        {
            value_type input[1] = {value_type{(Args &&) args...}};
            return insert(at, input, input + 1);
        }

        /** Returns a substring of `*this` as an `unencoded_rope_view`,
            comprising the elements at offsets `[lo, hi)`.  If either of `lo`
            or `hi` is a negative value `x`, `x` is taken to be an offset from
            the end, and so `x + size()` is used instead.

            These preconditions apply to the values used after `size()` is
            added to any negative arguments.

            \pre 0 <= lo && lo <= size()
            \pre 0 <= hi && lhi <= size()
            \pre lo <= hi */
        unencoded_rope_view
        operator()(std::ptrdiff_t lo, std::ptrdiff_t hi) const;

        /** Lexicographical compare.  Returns a value `< 0` when `*this` is
            lexicographically less than `rhs, `0` if `*this == rhs`, and a
            value `> 0` if `*this` is lexicographically greater than `rhs`. */
        int compare(basic_unencoded_rope rhs) const noexcept
        {
            if (this->empty())
                return rhs.empty() ? 0 : -1;
            return boost::text::lexicographical_compare_three_way(
                begin(), end(), rhs.begin(), rhs.end());
        }

        operator unencoded_rope_view() const noexcept;

        /** Erases the portion of `*this` delimited by `[first, last)`.

            \pre rv.begin() <= rv.begin() && rv.end() <= end() */
        basic_unencoded_rope & erase(const_iterator first, const_iterator last)
        {
            seg_vec_.erase(first, last);
            return *this;
        }

        /** Erases the portion of `*this` delimited by `rv`.

            \pre rv.begin() <= rv.begin() && rv.end() <= end() */
        basic_unencoded_rope & erase(unencoded_rope_view rv);

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `c_str`.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
        basic_unencoded_rope & replace(
            const_iterator first, const_iterator last, value_type const * c_str)
        {
            seg_vec_.replace(first, last, string(c_str));
            return *this;
        }

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `rv`.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
        basic_unencoded_rope & replace(
            const_iterator first, const_iterator last, unencoded_rope_view rv);

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `s`.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
        basic_unencoded_rope &
        replace(const_iterator first, const_iterator last, string && s)
        {
            seg_vec_.replace(first, last, std::move(s));
            return *this;
        }

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            `r`.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
#if BOOST_TEXT_USE_CONCEPTS
        template<std::ranges::range R>
        // clang-format off
            requires
            std::convertible_to<std::ranges::range_reference_t<R>, value_type>
#else
        template<typename R>
#endif
        basic_unencoded_rope &
        replace(const_iterator first, const_iterator last, R const & r)
        // clang-format on
        {
            seg_vec_.replace(first, last, r.begin(), r.end());
            return *this;
        }

        /** Replaces the portion of `*this` delimited by `[first1, last1)`
            with `[first2, last2)`.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
#if BOOST_TEXT_USE_CONCEPTS
        template<std::input_iterator I, std::sentinel_for<I> S>
        // clang-format off
            requires std::convertible_to<std::iter_reference_t<I>, value_type>
#else
        template<typename I, typename S>
#endif
        basic_unencoded_rope & replace(
            const_iterator first1,
            const_iterator last1,
            I first2,
            S last2)
        // clang-format on
        {
            seg_vec_.replace(first1, last1, first2, last2);
            return *this;
        }

#ifdef BOOST_TEXT_DOXYGEN

        /** Replaces the portion of `*this` delimited by `old_substr` with
            `r`.

            This function only participates in overload resolution if
            `replace(old_substr.begin().as_rope_iter(),
            old_substr.end().as_rope_iter(), std::forward<Range>(r))` is
            well-formed.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
        template<typename Range>
        basic_unencoded_rope &
        replace(unencoded_rope_view old_substr, Range && r);

        /** Replaces the portion of `*this` delimited by `old_substr` with
            `[first, last)`.

            This function only participates in overload resolution if
            `replace(old_substr.begin().as_rope_iter(),
            old_substr.end().as_rope_iter(), first, last)` is well-formed.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
        template<typename Iter, typename Sentinel>
        basic_unencoded_rope &
        replace(unencoded_rope_view old_substr, Iter first, Sentinel last);

#else

#if BOOST_TEXT_USE_CONCEPTS
        template<typename R>
            // clang-format off
        requires std::ranges::range<R> &&
            std::convertible_to<
                std::ranges::range_reference_t<R>, value_type> ||
            std::convertible_to<R, value_type const *>
#else
        template<typename R>
#endif
        auto replace(unencoded_rope_view const & old_substr, R && r)
            // clang-format on
            -> decltype(replace(const_iterator{}, const_iterator{}, r))
        {
            return replace_shim<R>(old_substr, (R &&) r);
        }

#if BOOST_TEXT_USE_CONCEPTS
        template<std::input_iterator I, std::sentinel_for<I> S>
        // clang-format off
            requires std::convertible_to<std::iter_reference_t<I>, value_type>
#else
        template<typename I, typename S>
#endif
        auto replace(unencoded_rope_view const & old_substr, I first, S last)
            // clang-format on
            -> decltype(
                replace(const_iterator{}, const_iterator{}, first, last))
        {
            return replace_shim<I, S>(old_substr, first, last);
        }

#endif

#ifdef BOOST_TEXT_DOXYGEN

        /** Inserts `r` into `*this` at position `at`.

            This function only participates in overload resolution if
            `replace(at, at, std::forward<Range>(r))` is well-formed.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
        template<typename Range>
        const_iterator insert(const_iterator at, Range && r);

        /** Inserts `[first, last)` into `*this` at position `at`.

            This function only participates in overload resolution if
            `replace(at, at, first, last)` is well-formed.

            \pre begin() <= old_substr.begin() && old_substr.end() <= end() */
        template<typename Iter, typename Sentinel>
        const_iterator insert(const_iterator at, Iter first, Sentinel last);

        /** Appends `x` to `*this`.

            This function only participates in overload resolution if
            `*this = std::forward<T>(x)` is well-formed. */
        template<typename T>
        basic_unencoded_rope & operator+=(T && x);

#else

#if BOOST_TEXT_USE_CONCEPTS
        template<typename R>
            // clang-format off
            requires std::ranges::range<R> &&
                std::convertible_to<
                    std::ranges::range_reference_t<R>, value_type> ||
                std::convertible_to<R, value_type const *>
#else
        template<typename R>
#endif
        auto insert(const_iterator at, R && r)
            // clang-format on
            -> decltype(replace(at, at, std::forward<R>(r)), const_iterator{})
        {
            auto const at_offset = at - begin();
            replace(at, at, std::forward<R>(r));
            return begin() + at_offset;
        }

#if BOOST_TEXT_USE_CONCEPTS
        template<std::input_iterator I, std::sentinel_for<I> S>
        // clang-format off
            requires std::convertible_to<std::iter_reference_t<I>, value_type>
#else
        template<typename I, typename S>
#endif
        auto insert(const_iterator at, I first, S last)
            // clang-format on
            -> decltype(replace(at, at, first, last), const_iterator{})
        {
            auto const at_offset = at - begin();
            replace(at, at, first, last);
            return begin() + at_offset;
        }

        template<typename T>
        auto operator+=(T && x) -> decltype(*this = std::forward<T>(x))
        {
            insert(end(), std::forward<T>(x));
            return *this;
        }

#endif

        void swap(basic_unencoded_rope & other)
        {
            seg_vec_.swap(other.seg_vec_);
        }

        using base_type = boost::stl_interfaces::sequence_container_interface<
            basic_unencoded_rope<Char, String>,
            boost::stl_interfaces::element_layout::discontiguous>;
        using base_type::begin;
        using base_type::end;
        using base_type::insert;
        using base_type::erase;

        /** Returns true if `*this` and `other` contain the same root node
            pointer.  This is useful when you want to check for equality
            between two `basic_unencoded_rope`s that are likely to have
            originated from the same initial `basic_unencoded_rope`, and may
            have since been mutated. */
        bool equal_root(basic_unencoded_rope other) const noexcept
        {
            return seg_vec_.equal_root(other.seg_vec_);
        }

        /** Stream inserter; performs unformatted output. */
        friend std::ostream &
        operator<<(std::ostream & os, basic_unencoded_rope r)
        {
            for (auto c : r) {
                os << c;
            }
            return os;
        }

        friend bool
        operator==(value_type const * lhs, basic_unencoded_rope rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs, null_sentinel{}, rhs.begin(), rhs.end()) == 0;
        }

        friend bool
        operator==(basic_unencoded_rope lhs, value_type const * rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs.begin(), lhs.end(), rhs, null_sentinel{}) == 0;
        }

        friend bool
        operator!=(value_type const * lhs, basic_unencoded_rope rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs, null_sentinel{}, rhs.begin(), rhs.end()) != 0;
        }

        friend bool
        operator!=(basic_unencoded_rope lhs, value_type const * rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs.begin(), lhs.end(), rhs, null_sentinel{}) != 0;
        }

        friend bool
        operator<(value_type const * lhs, basic_unencoded_rope rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs, null_sentinel{}, rhs.begin(), rhs.end()) < 0;
        }

        friend bool
        operator<(basic_unencoded_rope lhs, value_type const * rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs.begin(), lhs.end(), rhs, null_sentinel{}) < 0;
        }

        friend bool
        operator<=(value_type const * lhs, basic_unencoded_rope rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs, null_sentinel{}, rhs.begin(), rhs.end()) <= 0;
        }

        friend bool
        operator<=(basic_unencoded_rope lhs, value_type const * rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs.begin(), lhs.end(), rhs, null_sentinel{}) <= 0;
        }

        friend bool
        operator>(value_type const * lhs, basic_unencoded_rope rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs, null_sentinel{}, rhs.begin(), rhs.end()) > 0;
        }

        friend bool
        operator>(basic_unencoded_rope lhs, value_type const * rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs.begin(), lhs.end(), rhs, null_sentinel{}) > 0;
        }

        friend bool
        operator>=(value_type const * lhs, basic_unencoded_rope rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs, null_sentinel{}, rhs.begin(), rhs.end()) >= 0;
        }

        friend bool
        operator>=(basic_unencoded_rope lhs, value_type const * rhs) noexcept
        {
            return boost::text::lexicographical_compare_three_way(
                       lhs.begin(), lhs.end(), rhs, null_sentinel{}) >= 0;
        }

        friend void swap(basic_unencoded_rope & lhs, basic_unencoded_rope & rhs)
        {
            lhs.swap(rhs);
        }

#ifndef BOOST_TEXT_DOXYGEN

    private:
        template<typename R>
        basic_unencoded_rope &
        replace_shim(unencoded_rope_view const & old_substr, R && r);

        template<typename I, typename S>
        basic_unencoded_rope &
        replace_shim(unencoded_rope_view const & old_substr, I first, S last);

        segmented_vector<value_type, string> seg_vec_;

        friend unencoded_rope_view;

#endif
    };

#if BOOST_TEXT_USE_CONCEPTS

    /** Returns true iff `lhs` == `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator==(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        // clang-format off
        requires requires { lhs = rhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
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
    template<typename Char, typename String, typename T>
    bool operator==(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        // clang-format off
        requires requires { rhs = lhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return algorithm::equal(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs));
    }

    /** Returns true iff `lhs` != `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator!=(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        // clang-format off
        requires requires { lhs = rhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return !(lhs == rhs);
    }

    /** Returns true iff `lhs` != `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator!=(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        // clang-format off
        requires requires { rhs = lhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return !(lhs == rhs);
    }

    /** Returns true iff `lhs` < `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator<(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        // clang-format off
        requires requires { lhs = rhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return boost::text::lexicographical_compare_three_way(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs)) < 0;
    }

    /** Returns true iff `lhs` < `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator<(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        // clang-format off
        requires requires { rhs = lhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return boost::text::lexicographical_compare_three_way(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs)) < 0;
    }

    /** Returns true iff `lhs` <= `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator<=(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        // clang-format off
        requires requires { lhs = rhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return boost::text::lexicographical_compare_three_way(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs)) <= 0;
    }

    /** Returns true iff `lhs` <= `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator<=(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        // clang-format off
        requires requires { rhs = lhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return boost::text::lexicographical_compare_three_way(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs)) <= 0;
    }

    /** Returns true iff `lhs` > `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator>(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        // clang-format off
        requires requires { lhs = rhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return boost::text::lexicographical_compare_three_way(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs)) > 0;
    }

    /** Returns true iff `lhs` > `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator>(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        // clang-format off
        requires requires { rhs = lhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return boost::text::lexicographical_compare_three_way(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs)) > 0;
    }

    /** Returns true iff `lhs` >= `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator>=(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        // clang-format off
        requires requires { lhs = rhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return boost::text::lexicographical_compare_three_way(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs)) >= 0;
    }

    /** Returns true iff `lhs` >= `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    bool operator>=(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        // clang-format off
        requires requires { rhs = lhs; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        return boost::text::lexicographical_compare_three_way(
                   std::ranges::begin(lhs),
                   std::ranges::end(lhs),
                   std::ranges::begin(rhs),
                   std::ranges::end(rhs)) >= 0;
    }

#else

    /** Returns true iff `lhs` == `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator==(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(lhs = lhs, true)>
    {
        return algorithm::equal(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs));
    }

    /** Returns true iff `lhs` == `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator==(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(rhs = lhs, true)>
    {
        return algorithm::equal(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs));
    }

    /** Returns true iff `lhs` != `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator!=(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(lhs = rhs, true)>
    {
        return !(lhs == rhs);
    }

    /** Returns true iff `lhs` != `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator!=(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(rhs = lhs, true)>
    {
        return !(lhs == rhs);
    }

    /** Returns true iff `lhs` < `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator<(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(lhs = rhs, true)>
    {
        return boost::text::lexicographical_compare_three_way(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs)) < 0;
    }

    /** Returns true iff `lhs` < `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator<(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(rhs = lhs, true)>
    {
        return boost::text::lexicographical_compare_three_way(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs)) < 0;
    }

    /** Returns true iff `lhs` <= `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator<=(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(lhs = rhs, true)>
    {
        return boost::text::lexicographical_compare_three_way(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs)) <= 0;
    }

    /** Returns true iff `lhs` <= `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator<=(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(rhs = lhs, true)>
    {
        return boost::text::lexicographical_compare_three_way(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs)) <= 0;
    }

    /** Returns true iff `lhs` > `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator>(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(lhs = rhs, true)>
    {
        return boost::text::lexicographical_compare_three_way(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs)) > 0;
    }

    /** Returns true iff `lhs` > `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator>(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(rhs = lhs, true)>
    {
        return boost::text::lexicographical_compare_three_way(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs)) > 0;
    }

    /** Returns true iff `lhs` >= `rhs`, where `rhs` is an object for which
        `lhs = rhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator>=(basic_unencoded_rope<Char, String> lhs, T const & rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(lhs = rhs, true)>
    {
        return boost::text::lexicographical_compare_three_way(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs)) >= 0;
    }

    /** Returns true iff `lhs` >= `rhs`, where `rhs` is an object for which
        `rhs = lhs` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator>=(T const & lhs, basic_unencoded_rope<Char, String> rhs)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(rhs = lhs, true)>
    {
        return boost::text::lexicographical_compare_three_way(
                   std::begin(lhs),
                   std::end(lhs),
                   std::begin(rhs),
                   std::end(rhs)) >= 0;
    }

#endif

}}

#include <boost/text/unencoded_rope_view.hpp>

#ifndef BOOST_TEXT_DOXYGEN

namespace boost { namespace text {

    template<typename Char, typename String>
    basic_unencoded_rope<Char, String>::basic_unencoded_rope(
        unencoded_rope_view rv)
    {
        insert(begin(), rv);
    }

    template<typename Char, typename String>
    basic_unencoded_rope<Char, String> &
    basic_unencoded_rope<Char, String>::operator=(unencoded_rope_view rv)
    {
        basic_unencoded_rope temp(rv);
        swap(temp);
        return *this;
    }

    template<typename Char, typename String>
    basic_unencoded_rope_view<Char, String>
    basic_unencoded_rope<Char, String>::operator()(
        std::ptrdiff_t lo, std::ptrdiff_t hi) const
    {
        if (lo < 0)
            lo += this->size();
        if (hi < 0)
            hi += this->size();
        BOOST_ASSERT(0 <= lo && lo <= (std::ptrdiff_t)this->size());
        BOOST_ASSERT(0 <= hi && hi <= (std::ptrdiff_t)this->size());
        BOOST_ASSERT(lo <= hi);
        return unencoded_rope_view(*this, lo, hi);
    }

    template<typename Char, typename String>
    basic_unencoded_rope<Char, String> &
    basic_unencoded_rope<Char, String>::erase(unencoded_rope_view rv)
    {
        seg_vec_.erase(rv.begin().as_rope_iter(), rv.end().as_rope_iter());
        return *this;
    }

    template<typename Char, typename String>
    basic_unencoded_rope<Char, String> &
    basic_unencoded_rope<Char, String>::replace(
        const_iterator first, const_iterator last, unencoded_rope_view rv)
    {
        seg_vec_.replace(first, last, String(rv.begin(), rv.end()));
        return *this;
    }

    template<typename Char, typename String>
    basic_unencoded_rope<Char, String>::
    operator basic_unencoded_rope_view<Char, String>() const noexcept
    {
        return unencoded_rope_view(*this, 0, this->size());
    }

    template<typename Char, typename String>
    template<typename R>
    basic_unencoded_rope<Char, String> &
    basic_unencoded_rope<Char, String>::replace_shim(
        unencoded_rope_view const & old_substr, R && r)
    {
        replace(
            old_substr.begin().as_rope_iter(),
            old_substr.end().as_rope_iter(),
            (R &&) r);
        return *this;
    }

    template<typename Char, typename String>
    template<typename I, typename S>
    basic_unencoded_rope<Char, String> &
    basic_unencoded_rope<Char, String>::replace_shim(
        unencoded_rope_view const & old_substr, I first, S last)
    {
        return replace(
            old_substr.begin().as_rope_iter(),
            old_substr.end().as_rope_iter(),
            first,
            last);
    }

}}

#endif

namespace boost { namespace text {

#if BOOST_TEXT_USE_CONCEPTS

    /** Creates a new `basic_unencoded_rope` object that is the concatenation
        of `t` and some object `x` for which `ur = x` is well-formed. */
    template<typename Char, typename String, typename T>
    basic_unencoded_rope<Char, String>
    operator+(basic_unencoded_rope<Char, String> ur, T const & x)
        // clang-format off
        requires requires { ur = x; }
    // clang-format on
    {
        ur.insert(ur.end(), x);
        return ur;
    }

    /** Creates a new `basic_unencoded_rope` object that is the concatenation
        of `x` and `t`, where `x` is an object for which `ur = x` is
        well-formed. */
    template<typename Char, typename String, typename T>
    basic_unencoded_rope<Char, String>
    operator+(T const & x, basic_unencoded_rope<Char, String> ur)
        // clang-format off
        requires requires { ur = x; } &&
            (!std::is_same_v<T, basic_unencoded_rope<Char, String>>)
    // clang-format on
    {
        ur.insert(ur.begin(), x);
        return ur;
    }

#else

    /** Creates a new `basic_unencoded_rope` object that is the concatenation
        of `t` and some object `x` for which `ur = x` is well-formed. */
    template<typename Char, typename String, typename T>
    auto operator+(basic_unencoded_rope<Char, String> ur, T const & x)
        -> decltype(ur = x, basic_unencoded_rope<Char, String>{})
    {
        ur.insert(ur.end(), x);
        return ur;
    }

    /** Creates a new `basic_unencoded_rope` object that is the concatenation
        of `x` and `t`, where `x` is an object for which `ur = x` is
        well-formed. */
    template<typename Char, typename String, typename T>
    auto operator+(T const & x, basic_unencoded_rope<Char, String> ur)
        -> std::enable_if_t<
            !std::is_same<T, basic_unencoded_rope<Char, String>>::value,
            decltype(ur = x, basic_unencoded_rope<Char, String>{})>
    {
        ur.insert(ur.begin(), x);
        return ur;
    }

#endif

    template<typename Char, typename String>
    basic_unencoded_rope_view<Char, String>::basic_unencoded_rope_view(
        basic_unencoded_rope<Char, String> const & r,
        size_type lo,
        size_type hi) :
        ref_(rope_ref(&r.seg_vec_, lo, hi)), which_(which::r)
    {}

    namespace detail {

#ifdef BOOST_TEXT_TESTING
        template<typename T, typename Segment>
        void dump_tree(
            std::ostream & os,
            node_ptr<T, Segment> const & root,
            int key,
            int indent)
        {
            os << std::string(indent * 4, ' ')
               << (root->leaf_ ? "LEAF" : "INTR") << " @0x" << std::hex
               << root.get();
            if (key != -1)
                os << " < " << std::dec << key;
            os << " (" << root->refs_ << " refs)\n";
            if (!root->leaf_) {
                int i = 0;
                for (auto const & child : children(root)) {
                    dump_tree(os, child, keys(root)[i++], indent + 1);
                }
            }
        }
#endif
    }
}}

#ifndef BOOST_TEXT_DOXYGEN

namespace std {
    template<typename Char, typename String>
    struct hash<boost::text::basic_unencoded_rope<Char, String>>
    {
        using argument_type = boost::text::basic_unencoded_rope<Char, String>;
        using result_type = std::size_t;
        result_type operator()(argument_type const & ur) const noexcept
        {
            return boost::text::detail::hash_char_range(ur);
        }
    };
}

#endif

#endif
