// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_ALGORITHM_HPP
#define BOOST_TEXT_ALGORITHM_HPP

#include <boost/text/detail/sentinel_tag.hpp>

#include <boost/stl_interfaces/view_interface.hpp>

#include <cstddef>
#include <iterator>
#include <utility>


namespace boost { namespace text {

    namespace detail {
        template<typename Iter>
        std::ptrdiff_t distance(Iter first, Iter last, non_sentinel_tag)
        {
            return std::distance(first, last);
        }

        template<typename Iter, typename Sentinel>
        std::ptrdiff_t distance(Iter first, Sentinel last, sentinel_tag)
        {
            std::ptrdiff_t retval = 0;
            while (first != last) {
                ++retval;
                ++first;
            }
            return retval;
        }
    }

    /** Range-friendly version of `std::distance()`, taking an iterator and a
        sentinel. */
    template<typename Iter, typename Sentinel>
    std::ptrdiff_t distance(Iter first, Sentinel last)
    {
        return detail::distance(
            first,
            last,
            typename std::conditional<
                std::is_same<Iter, Sentinel>::value,
                detail::non_sentinel_tag,
                detail::sentinel_tag>::type());
    }

    /** Range-friendly version of `std::find()`, taking an iterator and a
        sentinel. */
    template<typename BidiIter, typename Sentinel, typename T>
    BidiIter find(BidiIter first, Sentinel last, T const & x)
    {
        while (first != last) {
            if (*first == x)
                return first;
            ++first;
        }
        return first;
    }

    /** A range-friendly compliment to `std::find()`; returns an iterator to
        the first element not equal to `x`. */
    template<typename BidiIter, typename Sentinel, typename T>
    BidiIter find_not(BidiIter first, Sentinel last, T const & x)
    {
        while (first != last) {
            if (*first != x)
                return first;
            ++first;
        }
        return first;
    }

    /** Range-friendly version of `std::find_if()`, taking an iterator and a
        sentinel. */
    template<typename BidiIter, typename Sentinel, typename Pred>
    BidiIter find_if(BidiIter first, Sentinel last, Pred p)
    {
        while (first != last) {
            if (p(*first))
                return first;
            ++first;
        }
        return first;
    }

    /** Range-friendly version of `std::find_if_not()`, taking an iterator and
        a sentinel. */
    template<typename BidiIter, typename Sentinel, typename Pred>
    BidiIter find_if_not(BidiIter first, Sentinel last, Pred p)
    {
        while (first != last) {
            if (!p(*first))
                return first;
            ++first;
        }
        return first;
    }

    /** Analogue of `std::find()` that finds the last value in `[first, last)`
        equal to `x`. */
    template<typename BidiIter, typename T>
    BidiIter find_backward(BidiIter first, BidiIter last, T const & x)
    {
        auto it = last;
        while (it != first) {
            if (*--it == x)
                return it;
        }
        return last;
    }

    /** Analogue of `std::find()` that finds the last value in `[first, last)`
        not equal to `x`. */
    template<typename BidiIter, typename T>
    BidiIter find_not_backward(BidiIter first, BidiIter last, T const & x)
    {
        auto it = last;
        while (it != first) {
            if (*--it != x)
                return it;
        }
        return last;
    }

    /** Analogue of `std::find()` that finds the last value `v` in `[first,
        last)` for which `p(v)` is true. */
    template<typename BidiIter, typename Pred>
    BidiIter find_if_backward(BidiIter first, BidiIter last, Pred p)
    {
        auto it = last;
        while (it != first) {
            if (p(*--it))
                return it;
        }
        return last;
    }

    /** Analogue of `std::find()` that finds the last value `v` in `[first,
        last)` for which `p(v)` is false. */
    template<typename BidiIter, typename Pred>
    BidiIter find_if_not_backward(BidiIter first, BidiIter last, Pred p)
    {
        auto it = last;
        while (it != first) {
            if (!p(*--it))
                return it;
        }
        return last;
    }

    /** A utility range type returned by `foreach_subrange*()`. */
    template<typename Iter, typename Sentinel = Iter>
    struct foreach_subrange_range
    {
        using iterator = Iter;
        using sentinel = Sentinel;

        foreach_subrange_range() {}
        foreach_subrange_range(iterator first, sentinel last) :
            first_(first),
            last_(last)
        {}

        iterator begin() const noexcept { return first_; }
        sentinel end() const noexcept { return last_; }

    private:
        iterator first_;
        sentinel last_;
    };

    /** Calls `f(sub)` for each subrange `sub` in `[first, last)`.  A subrange
        is a contiguous subsequence of elements that each compares equal to
        the first element of the subsequence.  Subranges passed to `f` are
        non-overlapping. */
    template<typename FwdIter, typename Sentinel, typename Func>
    Func foreach_subrange(FwdIter first, Sentinel last, Func f)
    {
        while (first != last) {
            auto const & x = *first;
            auto const next = boost::text::find_not(first, last, x);
            if (first != next) {
                f(boost::text::foreach_subrange_range<FwdIter, Sentinel>(
                    first, next));
            }
            first = next;
        }
        return f;
    }

    /** Calls `f(sub)` for each subrange `sub` in `[first, last)`.  A subrange
        is a contiguous subsequence of elements that for each element `e`,
        `proj(e)` each compares equal to `proj()` of the first element of the
        subsequence.  Subranges passed to `f` are non-overlapping. */
    template<typename FwdIter, typename Sentinel, typename Func, typename Proj>
    Func foreach_subrange(FwdIter first, Sentinel last, Func f, Proj proj)
    {
        using value_type = typename std::iterator_traits<FwdIter>::value_type;
        while (first != last) {
            auto const & x = proj(*first);
            auto const next = boost::text::find_if_not(
                first, last, [&x, proj](const value_type & element) {
                    return proj(element) == x;
                });
            if (first != next) {
                f(boost::text::foreach_subrange_range<FwdIter, Sentinel>(
                    first, next));
            }
            first = next;
        }
        return f;
    }

    /** Calls `f(sub)` for each subrange `sub` in `[first, last)`.  A subrange
        is a contiguous subsequence of elements, each of which is equal to
        `x`.  Subranges passed to `f` are non-overlapping. */
    template<typename FwdIter, typename Sentinel, typename T, typename Func>
    Func foreach_subrange_of(FwdIter first, Sentinel last, T const & x, Func f)
    {
        while (first != last) {
            first = boost::text::find(first, last, x);
            auto const next = boost::text::find_not(first, last, x);
            if (first != next) {
                f(boost::text::foreach_subrange_range<FwdIter, Sentinel>(
                    first, next));
            }
            first = next;
        }
        return f;
    }

    /** Calls `f(sub)` for each subrange `sub` in `[first, last)`.  A subrange
        is a contiguous subsequence of elements `ei` for which `p(ei)` is
        true.  Subranges passed to `f` are non-overlapping. */
    template<typename FwdIter, typename Sentinel, typename Pred, typename Func>
    Func foreach_subrange_if(FwdIter first, Sentinel last, Pred p, Func f)
    {
        while (first != last) {
            first = boost::text::find_if(first, last, p);
            auto const next = boost::text::find_if_not(first, last, p);
            if (first != next) {
                f(boost::text::foreach_subrange_range<FwdIter, Sentinel>(
                    first, next));
            }
            first = next;
        }
        return f;
    }

    /** Sentinel-friendly version of `std::all_of()`. */
    template<typename Iter, typename Sentinel, typename Pred>
    bool all_of(Iter first, Sentinel last, Pred p)
    {
        for (; first != last; ++first) {
            if (!p(*first))
                return false;
        }
        return true;
    }

    /** Sentinel-friendly version of `std::equal()`. */
    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2>
    bool equal(Iter1 first1, Sentinel1 last1, Iter2 first2, Sentinel2 last2)
    {
        for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
            if (*first1 != *first2)
                return false;
        }
        return first1 == last1 && first2 == last2;
    }

    /** Sentinel-friendly version of `std::mismatch()`. */
    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2>
    std::pair<Iter1, Iter2>
    mismatch(Iter1 first1, Sentinel1 last1, Iter2 first2, Sentinel2 last2)
    {
        for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
            if (*first1 != *first2)
                break;
        }
        return {first1, first2};
    }

    /** Sentinel-friendly version of
        `std::lexicographical_compare_three_way()`, except that it returns an
        `int` instead of a `std::strong_ordering`. */
    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2>
    int lexicographical_compare_three_way(
        Iter1 first1, Sentinel1 last1, Iter2 first2, Sentinel2 last2)
    {
        auto const iters = boost::text::mismatch(first1, last1, first2, last2);
        if (iters.first == last1) {
            if (iters.second == last2)
                return 0;
            else
                return -1;
        } else if (iters.second == last2) {
            return 1;
        } else if (*iters.first < *iters.second) {
            return -1;
        } else {
            return 1;
        }
    }

    /** The view type returned by `boost::text::search()`. */
    template<typename Iter>
    struct search_result : stl_interfaces::view_interface<search_result<Iter>>
    {
        using iterator = Iter;

        constexpr search_result() noexcept {}
        constexpr search_result(iterator first, iterator last) noexcept :
            first_(first), last_(last)
        {}

        constexpr iterator begin() const noexcept { return first_; }
        constexpr iterator end() const noexcept { return last_; }

    private:
        iterator first_;
        iterator last_;
    };

    /** Sentinel-friendly version of `std::search()`. */
    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2>
    search_result<Iter1>
    search(Iter1 first1, Sentinel1 last1, Iter2 first2, Sentinel2 last2)
    {
        if (first1 == last1 || first2 == last2)
            return {first1, first1};

        if (std::next(first2) == last2) {
            auto const it = text::find(first1, last1, *first2);
            return {it, std::next(it)};
        }

        auto it = first1;
        for (;;) {
            first1 = text::find(first1, last1, *first2);

            if (first1 == last1)
                return {first1, first1};

            auto it2 = std::next(first2);
            it = first1;
            if (++it == last1)
                return {it, it};

            while (*it == *it2) {
                if (++it2 == last2)
                    return {first1, ++it};
                if (++it == last1)
                    return {it, it};
            }

            ++first1;
        }

        return {first1, first1};
    }

    /** Sentinel-friendly version of `std::find_first_of()`. */
    template<
        typename Iter1,
        typename Sentinel1,
        typename Iter2,
        typename Sentinel2,
        typename Pred>
    Iter1 find_first_of(
        Iter1 first1, Sentinel1 last1, Iter2 first2, Sentinel2 last2, Pred pred)
    {
        for (; first1 != last1; ++first1) {
            for (auto it = first2; it != last2; ++it) {
                if (pred(*first1, *it))
                    return first1;
            }
        }
        return first1;
    }

}}

#if BOOST_TEXT_USE_CONCEPTS

namespace std::ranges {
    template<typename Iter, typename Sentinel>
    inline constexpr bool enable_borrowed_range<
        boost::text::foreach_subrange_range<Iter, Sentinel>> = true;
}

#endif

#endif
