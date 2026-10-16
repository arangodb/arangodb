// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_UNENCODED_ROPE_VIEW_HPP
#define BOOST_TEXT_UNENCODED_ROPE_VIEW_HPP

#include <boost/text/segmented_vector.hpp>
#include <boost/text/unencoded_rope_fwd.hpp>
#include <boost/text/rope_fwd.hpp>
#include <boost/text/detail/rope_iterator.hpp>
#include <boost/text/detail/rope.hpp>

#include <boost/stl_interfaces/view_interface.hpp>


namespace boost { namespace text {

    template<typename Char, typename String>
#if BOOST_TEXT_USE_CONCEPTS
    // clang-format off
        requires std::is_same_v<Char, std::ranges::range_value_t<String>>
#endif
    struct basic_unencoded_rope_view
        // clang-format on
        : stl_interfaces::view_interface<
              basic_unencoded_rope_view<Char, String>>
    {
        using value_type = Char;
        using size_type = std::size_t;
        using iterator = detail::const_rope_view_iterator<Char, String>;
        using const_iterator = iterator;
        using reverse_iterator = stl_interfaces::reverse_iterator<iterator>;
        using const_reverse_iterator = reverse_iterator;

        using string = String;
        using string_view = boost::text::basic_string_view<Char>;

        /** Default ctor.

            \post size() == 0 && begin() == end() */
        basic_unencoded_rope_view() noexcept :
            ref_(rope_ref()), which_(which::r)
        {}

        basic_unencoded_rope_view(
            basic_unencoded_rope_view const & other) noexcept :
            ref_(string_view()), which_(other.which_)
        {
            switch (which_) {
            case which::r: ref_.r_ = other.ref_.r_; break;
            case which::tv: ref_.tv_ = other.ref_.tv_; break;
            }
        }

        /** Constructs a substring of `r`, comprising the elements at offsets
            `[lo, hi)`.  If either of `lo` or `hi` is a negative value `x`,
            `x` is taken to be an offset from the end, and so `x + size()` is
            used instead.

            These preconditions apply to the values used after `size()` is
            added to any negative arguments.

            \pre 0 <= lo && lo <= r.size()
            \pre 0 <= hi && lhi <= r.size()
            \pre lo <= hi
            \post size() == r.size() && begin() == r.begin() + lo && end() ==
            r.begin() + hi */
        basic_unencoded_rope_view(
            basic_unencoded_rope<Char, String> const & r,
            size_type lo,
            size_type hi);

        /** Constructs a `basic_unencoded_rope_view` covering the entire given
            string. */
        basic_unencoded_rope_view(string const & s) noexcept :
            ref_(string_view(s.data(), s.size())), which_(which::tv)
        {}

        /** Forbid construction from a temporary `string`. */
        basic_unencoded_rope_view(string && r) noexcept = delete;

        /** Constructs a substring of `s`, comprising the elements at offsets
            `[lo, hi)`.  If either of `lo` or `hi` is a negative value `x`,
            `x` is taken to be an offset from the end, and so `x + size()` is
            used instead.

            These preconditions apply to the values used after `size()` is
            added to any negative arguments.

            \pre 0 <= lo && lo <= s.size()
            \pre 0 <= hi && lhi <= s.size()
            \pre lo <= hi
            \post size() == s.size() && begin() == s.begin() + lo && end() ==
            s.begin() + hi */
        basic_unencoded_rope_view(
            string const & s, size_type lo, size_type hi) :
            ref_(detail::substring(s, lo, hi)), which_(which::tv)
        {}

        /** Constructs a `basic_unencoded_rope_view` from a null-terminated C
            string. */
        basic_unencoded_rope_view(value_type const * c_str) noexcept :
            ref_(string_view(c_str)), which_(which::tv)
        {}

        /** Constructs a `basic_unencoded_rope_view` covering the entire given
            `string_view`. */
        basic_unencoded_rope_view(string_view sv) noexcept :
            ref_(sv), which_(which::tv)
        {}

#ifdef BOOST_TEXT_DOXYGEN

        /** Constructs a `basic_unencoded_rope_view` from a range of
            `value_type`.

            This function only participates in overload resolution if
            `ContigCharRange` models the ContigCharRange concept. */
        template<typename ContigCharRange>
        explicit basic_unencoded_rope_view(ContigCharRange const & r);

#else

#if BOOST_TEXT_USE_CONCEPTS
        template<std::ranges::contiguous_range R>
        // clang-format off
            requires std::is_same_v<std::ranges::range_value_t<R>, value_type>
        explicit basic_unencoded_rope_view(R const & r) :
        // clang-format on
#else
        template<typename R>
        explicit basic_unencoded_rope_view(
            R const & r, detail::contig_rng_alg_ret_t<int *, R> = 0) :
#endif
            ref_(rope_ref())
        {
            if (std::begin(r) == std::end(r)) {
                *this = basic_unencoded_rope_view();
            } else {
                *this = basic_unencoded_rope_view(
                    string_view(&*std::begin(r), std::end(r) - std::begin(r)));
            }
        }

#endif

        const_iterator begin() const noexcept
        {
            switch (which_) {
            case which::r:
                return const_iterator(ref_.r_.r_->begin() + ref_.r_.lo_);
            case which::tv: return const_iterator(ref_.tv_.begin());
            }
            return const_iterator(); // This should never execute.
        }
        const_iterator end() const noexcept
        {
            switch (which_) {
            case which::r:
                return const_iterator(ref_.r_.r_->begin() + ref_.r_.hi_);
            case which::tv: return const_iterator(ref_.tv_.end());
            }
            return const_iterator(); // This should never execute.
        }

        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator cend() const noexcept { return end(); }

        const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator{end()};
        }
        const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator{begin()};
        }

        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator crend() const noexcept { return rend(); }

        /** Returns a substring of `*this`, comprising the elements at offsets
            `[lo, hi)`.  If either of `lo` or `hi` is a negative value `x`,
            `x` is taken to be an offset from the end, and so `x + size()` is
            used instead.

            These preconditions apply to the values used after `size()` is
            added to any negative arguments.

            \pre 0 <= lo && lo <= size()
            \pre 0 <= hi && lhi <= size()
            \pre lo <= hi */
        basic_unencoded_rope_view
        operator()(std::ptrdiff_t lo, std::ptrdiff_t hi) const
        {
            if (lo < 0)
                lo += this->size();
            if (hi < 0)
                hi += this->size();
            BOOST_ASSERT(0 <= lo && lo <= (std::ptrdiff_t)this->size());
            BOOST_ASSERT(0 <= hi && hi <= (std::ptrdiff_t)this->size());
            BOOST_ASSERT(lo <= hi);
            switch (which_) {
            case which::r:
                return basic_unencoded_rope_view(
                    ref_.r_.r_, ref_.r_.lo_ + lo, ref_.r_.lo_ + hi);
            case which::tv:
                return basic_unencoded_rope_view(
                    detail::substring(ref_.tv_, lo, hi));
            }
            return *this; // This should never execute.
        }

        /** Lexicographical compare.  Returns a value `< 0` when `*this` is
            lexicographically less than `rhs`, `0` if `*this == rhs`, and a
            value `> 0` if `*this` is lexicographically greater than `rhs`. */
        int compare(basic_unencoded_rope_view rhs) const noexcept
        {
            if (which_ == which::tv && rhs.which_ == which::tv)
                return ref_.tv_.compare(rhs.ref_.tv_);
            if (this->empty())
                return rhs.empty() ? 0 : -1;
            return boost::text::lexicographical_compare_three_way(
                begin(), end(), rhs.begin(), rhs.end());
        }

        basic_unencoded_rope_view &
        operator=(basic_unencoded_rope_view const & other) noexcept
        {
            which_ = other.which_;
            switch (which_) {
            case which::r: ref_.r_ = other.ref_.r_; break;
            case which::tv: ref_.tv_ = other.ref_.tv_; break;
            }
            return *this;
        }

        /** Assignment from a `basic_unencoded_rope`. */
        basic_unencoded_rope_view &
        operator=(basic_unencoded_rope<Char, String> const & r) noexcept
        {
            return *this = basic_unencoded_rope_view(r);
        }

        /** Assignment from a `string`. */
        basic_unencoded_rope_view & operator=(string const & s) noexcept
        {
            return *this = basic_unencoded_rope_view(s);
        }

        /** Forbid assignment from a `basic_unencoded_rope`. */
        basic_unencoded_rope_view &
        operator=(basic_unencoded_rope<Char, String> && r) noexcept = delete;

        /** Forbid assignment from a `string`. */
        basic_unencoded_rope_view & operator=(string && s) noexcept = delete;

        /** Assignment from a null-terminated C string. */
        basic_unencoded_rope_view & operator=(value_type const * c_str) noexcept
        {
            return *this = basic_unencoded_rope_view(c_str);
        }

        /** Assignment from a `string_view`. */
        basic_unencoded_rope_view & operator=(string_view sv) noexcept
        {
            return *this = basic_unencoded_rope_view(sv);
        }

#ifdef BOOST_TEXT_DOXYGEN

        /** Assignment from a contiguous range of `value_type` elements.

            This function only participates in overload resolution if
            `ContigCharRange` models the ContigCharRange concept. */
        template<typename ContigCharRange>
        basic_unencoded_rope_view & operator=(ContigCharRange const & r);

#else

#if BOOST_TEXT_USE_CONCEPTS
        template<std::ranges::contiguous_range R>
        // clang-format off
            requires std::is_same_v<std::ranges::range_value_t<R>, value_type>
        basic_unencoded_rope_view & operator=(R const & r)
        // clang-format on
#else
        template<typename R>
        auto operator=(R const & r)
            -> detail::contig_rng_alg_ret_t<basic_unencoded_rope_view &, R>
#endif
        {
            return *this = basic_unencoded_rope_view(r);
        }

#endif

        /** Stream inserter; performs unformatted output. */
        friend std::ostream &
        operator<<(std::ostream & os, basic_unencoded_rope_view rv)
        {
            for (auto c : rv) {
                os << c;
            }
            return os;
        }

        friend bool operator==(
            basic_unencoded_rope_view lhs,
            basic_unencoded_rope_view rhs) noexcept
        {
            return lhs.compare(rhs) == 0;
        }

        friend bool operator!=(
            basic_unencoded_rope_view lhs,
            basic_unencoded_rope_view rhs) noexcept
        {
            return lhs.compare(rhs) != 0;
        }

        friend bool operator<(
            basic_unencoded_rope_view lhs,
            basic_unencoded_rope_view rhs) noexcept
        {
            return lhs.compare(rhs) < 0;
        }

        friend bool operator<=(
            basic_unencoded_rope_view lhs,
            basic_unencoded_rope_view rhs) noexcept
        {
            return lhs.compare(rhs) <= 0;
        }

        friend bool operator>(
            basic_unencoded_rope_view lhs,
            basic_unencoded_rope_view rhs) noexcept
        {
            return lhs.compare(rhs) > 0;
        }

        friend bool operator>=(
            basic_unencoded_rope_view lhs,
            basic_unencoded_rope_view rhs) noexcept
        {
            return lhs.compare(rhs) >= 0;
        }

        friend void
        swap(basic_unencoded_rope_view & lhs, basic_unencoded_rope_view & rhs)
        {
            lhs.swap(rhs);
        }

#ifndef BOOST_TEXT_DOXYGEN

    private:
        enum class which{r, tv};

        struct rope_ref
        {
            rope_ref() : r_(nullptr), lo_(0), hi_(0) {}
            rope_ref(
                segmented_vector<value_type, string> const * r,
                std::ptrdiff_t lo,
                std::ptrdiff_t hi) :
                r_(r), lo_(lo), hi_(hi)
            {}

            segmented_vector<value_type, string> const * r_;
            size_type lo_;
            size_type hi_;
        };

        union ref
        {
            ref(rope_ref r) : r_(r) {}
            ref(string_view tv) : tv_(tv) {}

            rope_ref r_;
            string_view tv_;
        };

        basic_unencoded_rope_view(
            segmented_vector<value_type, string> const * r,
            std::size_t lo,
            std::size_t hi) :
            ref_(rope_ref(r, lo, hi)), which_(which::r)
        {}

        ref ref_;
        which which_;

        friend basic_unencoded_rope<Char, String>;
        friend basic_rope_view<nf::c, Char, String>;
        friend basic_rope_view<nf::kc, Char, String>;
        friend basic_rope_view<nf::d, Char, String>;
        friend basic_rope_view<nf::kd, Char, String>;
        friend basic_rope_view<nf::fcc, Char, String>;
#endif
    };

}}

#include <boost/text/unencoded_rope.hpp>

namespace boost { namespace text {

    template<typename Char, typename String>
    int operator+(
        basic_unencoded_rope_view<Char, String> lhs,
        basic_unencoded_rope_view<Char, String> rhs) = delete;

}}

#ifndef BOOST_TEXT_DOXYGEN

namespace std {
    template<typename Char, typename String>
    struct hash<boost::text::basic_unencoded_rope_view<Char, String>>
    {
        using argument_type =
            boost::text::basic_unencoded_rope_view<Char, String>;
        using result_type = std::size_t;
        result_type operator()(argument_type const & urv) const noexcept
        {
            return boost::text::detail::hash_char_range(urv);
        }
    };
}

#endif

#if BOOST_TEXT_USE_CONCEPTS

namespace std::ranges {
    template<typename Char, typename String>
    inline constexpr bool enable_borrowed_range<
        boost::text::basic_unencoded_rope_view<Char, String>> = true;
}

#endif

#endif
