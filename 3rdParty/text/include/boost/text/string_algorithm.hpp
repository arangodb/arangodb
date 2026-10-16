// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_STRING_ALGORITHM_HPP
#define BOOST_TEXT_STRING_ALGORITHM_HPP

#include <boost/text/algorithm.hpp>
#include <boost/text/concepts.hpp>
#include <boost/text/grapheme_view.hpp>
#include <boost/text/string_view.hpp>
#include <boost/text/transcode_view.hpp>
#include <boost/text/detail/algorithm.hpp>

#include <algorithm>


namespace boost { namespace text { namespace detail {

    template<typename T>
    using ptr_arr_value_t =
        std::remove_pointer_t<std::remove_extent_t<std::remove_reference_t<T>>>;

    template<typename I, typename S>
    utf32_view<I, S> remove_utf32_terminator(utf32_view<I, S> view)
    {
        return view;
    }
    template<typename I>
    utf32_view<I> remove_utf32_terminator(utf32_view<I> view)
    {
        if (!view.empty() && view.back() == 0)
            return utf32_view<I>(view.begin(), std::prev(view.end()));
        return view;
    }

    template<typename R>
    auto as_utf32_no_terminator(R & r)
        -> decltype(detail::remove_utf32_terminator(boost::text::as_utf32(r)))
    {
        return detail::remove_utf32_terminator(boost::text::as_utf32(r));
    }

    template<typename I>
    auto as_utf32_no_sentinel_or_terminator(I first, I last) -> decltype(
        detail::remove_utf32_terminator(boost::text::as_utf32(first, last)))
    {
        return detail::remove_utf32_terminator(
            boost::text::as_utf32(first, last));
    }

    template<typename I, typename S>
    auto as_utf32_no_sentinel_or_terminator(I first, S last)
    {
        auto it = first;
        while (it != last) {
            ++it;
        }
        return boost::text::as_utf32(first, it);
    }

    template<
        typename R,
        bool Pointer = std::is_pointer<std::remove_reference_t<R>>::value>
    struct as_utf32_common_view_dispatch
    {
        static constexpr auto call(R & r) noexcept
            -> decltype(detail::as_utf32_no_sentinel_or_terminator(
                std::begin(r), std::end(r)))
        {
            return detail::as_utf32_no_sentinel_or_terminator(
                std::begin(r), std::end(r));
        }
    };

    template<typename Ptr>
    struct as_utf32_common_view_dispatch<Ptr, true>
    {
        using string_view_t = basic_string_view<std::remove_pointer_t<Ptr>>;
        static constexpr auto call(Ptr p) noexcept
            -> decltype(boost::text::as_utf32(string_view_t(p)))
        {
            return boost::text::as_utf32(string_view_t(p));
        }
    };

    template<typename R>
    auto as_utf32_common_view_no_terminator(R & r)
        -> decltype(as_utf32_common_view_dispatch<R>::call(r))
    {
        return as_utf32_common_view_dispatch<R>::call(r);
    }

}}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    namespace dtl {
        template<typename I1, typename S1, typename I2, typename S2>
        search_result<I1> find(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return text::search(first1, last1, first2, last2);
        }

        template<typename I1, typename I2>
        search_result<I1> rfind(I1 first1, I1 last1, I2 first2, I2 last2)
        {
            auto const result = text::search(
                std::make_reverse_iterator(last1),
                std::make_reverse_iterator(first1),
                std::make_reverse_iterator(last2),
                std::make_reverse_iterator(first2));
            if (result.empty())
                return {last1, last1};
            return {result.end().base(), result.begin().base()};
        }

        template<typename I1, typename S1, typename I2, typename S2>
        I1 find_first_of(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return text::find_first_of(
                first1, last1, first2, last2, std::equal_to<>{});
        }

        template<typename I1, typename I2>
        I1 find_last_of(I1 first1, I1 last1, I2 first2, I2 last2)
        {
            auto const last1_ = std::make_reverse_iterator(first1);
            auto const result = text::find_first_of(
                std::make_reverse_iterator(last1),
                last1_,
                std::make_reverse_iterator(last2),
                std::make_reverse_iterator(first2),
                std::equal_to<>{});
            if (result == last1_)
                return last1;
            return std::prev(result.base());
        }

        template<typename I1, typename S1, typename I2, typename S2>
        I1 find_first_not_of(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return text::find_if(
                first1, last1, [first2, last2](auto const & x) {
                    return text::find(first2, last2, x) == last2;
                });
        }

        template<typename I1, typename I2>
        I1 find_last_not_of(I1 first1_, I1 last1_, I2 first2_, I2 last2_)
        {
            auto const first1 = std::make_reverse_iterator(last1_);
            auto const last1 = std::make_reverse_iterator(first1_);
            auto const first2 = std::make_reverse_iterator(last2_);
            auto const last2 = std::make_reverse_iterator(first2_);
            auto const result =
                text::find_if(first1, last1, [first2, last2](auto const & x) {
                    return text::find(first2, last2, x) == last2;
                });
            if (result == last1)
                return last1_;
            return std::prev(result.base());
        }

        template<typename I1, typename S1, typename I2, typename S2>
        bool starts_with(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return text::mismatch(first1, last1, first2, last2).second == last2;
        }

        template<typename I1, typename I2>
        bool ends_with(I1 first1, I1 last1, I2 first2, I2 last2)
        {
            auto const target = std::make_reverse_iterator(first2);
            return text::mismatch(
                       std::make_reverse_iterator(last1),
                       std::make_reverse_iterator(first1),
                       std::make_reverse_iterator(last2),
                       target)
                       .second == target;
        }

        template<typename I1, typename S1, typename I2, typename S2>
        bool contains(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return dtl::find(first1, last1, first2, last2).begin() != last1;
        }
    }


    // Code point iterator overloads.

    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    auto find(CPIter1 first1, Sentinel1 last1, CPIter2 first2, Sentinel2 last2)
        ->detail::cp_iters_ret_t<search_result<CPIter1>, CPIter1, CPIter2>
    {
        return dtl::find(first1, last1, first2, last2);
    }

    template<typename CPIter1, typename CPIter2>
    auto rfind(CPIter1 first1, CPIter1 last1, CPIter2 first2, CPIter2 last2)
        ->detail::cp_iters_ret_t<search_result<CPIter1>, CPIter1, CPIter2>
    {
        return dtl::rfind(first1, last1, first2, last2);
    }

    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    auto find_first_of(
        CPIter1 first1, Sentinel1 last1, CPIter2 first2, Sentinel2 last2)
        ->detail::cp_iters_ret_t<CPIter1, CPIter1, CPIter2>
    {
        return dtl::find_first_of(first1, last1, first2, last2);
    }

    template<typename CPIter1, typename CPIter2>
    auto find_last_of(
        CPIter1 first1, CPIter1 last1, CPIter2 first2, CPIter2 last2)
        ->detail::cp_iters_ret_t<CPIter1, CPIter1, CPIter2>
    {
        return dtl::find_last_of(first1, last1, first2, last2);
    }

    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    auto find_first_not_of(
        CPIter1 first1, Sentinel1 last1, CPIter2 first2, Sentinel2 last2)
        ->detail::cp_iters_ret_t<CPIter1, CPIter1, CPIter2>
    {
        return dtl::find_first_not_of(first1, last1, first2, last2);
    }

    template<typename CPIter1, typename CPIter2>
    auto find_last_not_of(
        CPIter1 first1, CPIter1 last1, CPIter2 first2, CPIter2 last2)
        ->detail::cp_iters_ret_t<CPIter1, CPIter1, CPIter2>
    {
        return dtl::find_last_not_of(first1, last1, first2, last2);
    }

    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    auto starts_with(
        CPIter1 first1, Sentinel1 last1, CPIter2 first2, Sentinel2 last2)
        ->detail::cp_iters_ret_t<bool, CPIter1, CPIter2>
    {
        return dtl::starts_with(first1, last1, first2, last2);
    }

    template<typename CPIter1, typename CPIter2>
    auto ends_with(CPIter1 first1, CPIter1 last1, CPIter2 first2, CPIter2 last2)
        ->detail::cp_iters_ret_t<bool, CPIter1, CPIter2>
    {
        return dtl::ends_with(first1, last1, first2, last2);
    }

    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    auto contains(
        CPIter1 first1, Sentinel1 last1, CPIter2 first2, Sentinel2 last2)
        ->detail::cp_iters_ret_t<bool, CPIter1, CPIter2>
    {
        return dtl::contains(first1, last1, first2, last2);
    }

    // Code point range overloads.

    namespace dtl {
        template<typename R>
        struct str_alg_fwd_search_result
        {
            using type = search_result<detail::iterator_t<decltype(
                detail::as_utf32_no_terminator(std::declval<R>()))>>;
        };
        template<typename R>
        struct str_alg_rev_search_result
        {
            using type = search_result<detail::iterator_t<decltype(
                detail::as_utf32_common_view_no_terminator(
                    std::declval<R>()))>>;
        };
        template<typename R>
        struct str_alg_fwd_iter
        {
            using type = detail::iterator_t<decltype(
                detail::as_utf32_no_terminator(std::declval<R>()))>;
        };
        template<typename R>
        struct str_alg_rev_iter
        {
            using type = detail::iterator_t<decltype(
                detail::as_utf32_common_view_no_terminator(std::declval<R>()))>;
        };
    }

    template<typename Range1, typename Range2>
    auto find(Range1 && r1, Range2 && r2)
        ->detail::lazy_non_graph_rngs_alg_ret_t<
            dtl::str_alg_fwd_search_result<Range1>,
            Range1,
            Range2>
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::find(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    template<typename Range1, typename Range2>
    auto rfind(Range1 && r1, Range2 && r2)
        ->detail::lazy_non_graph_rngs_alg_ret_t<
            dtl::str_alg_rev_search_result<Range1>,
            Range1,
            Range2>
    {
        auto r1_ = detail::as_utf32_common_view_no_terminator(r1);
        auto r2_ = detail::as_utf32_common_view_no_terminator(r2);
        return dtl::rfind(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    template<typename Range1, typename Range2>
    auto find_first_of(Range1 && r1, Range2 && r2)
        ->detail::lazy_non_graph_rngs_alg_ret_t<
            dtl::str_alg_fwd_iter<Range1>,
            Range1,
            Range2>
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::find_first_of(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    template<typename Range1, typename Range2>
    auto find_last_of(Range1 && r1, Range2 && r2)
        ->detail::lazy_non_graph_rngs_alg_ret_t<
            dtl::str_alg_rev_iter<Range1>,
            Range1,
            Range2>
    {
        auto r1_ = detail::as_utf32_common_view_no_terminator(r1);
        auto r2_ = detail::as_utf32_common_view_no_terminator(r2);
        return dtl::find_last_of(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    template<typename Range1, typename Range2>
    auto find_first_not_of(Range1 && r1, Range2 && r2)
        ->detail::lazy_non_graph_rngs_alg_ret_t<
            dtl::str_alg_fwd_iter<Range1>,
            Range1,
            Range2>
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::find_first_not_of(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    template<typename Range1, typename Range2>
    auto find_last_not_of(Range1 && r1, Range2 && r2)
        ->detail::lazy_non_graph_rngs_alg_ret_t<
            dtl::str_alg_rev_iter<Range1>,
            Range1,
            Range2>
    {
        auto r1_ = detail::as_utf32_common_view_no_terminator(r1);
        auto r2_ = detail::as_utf32_common_view_no_terminator(r2);
        return dtl::find_last_not_of(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    template<typename Range1, typename Range2>
    auto starts_with(Range1 && r1, Range2 && r2)
        ->detail::non_graph_rngs_alg_ret_t<bool, Range1, Range2>
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::starts_with(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    template<typename Range1, typename Range2>
    auto ends_with(Range1 && r1, Range2 && r2)
        ->detail::non_graph_rngs_alg_ret_t<bool, Range1, Range2>
    {
        auto r1_ = detail::as_utf32_common_view_no_terminator(r1);
        auto r2_ = detail::as_utf32_common_view_no_terminator(r2);
        return dtl::ends_with(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    template<typename Range1, typename Range2>
    auto contains(Range1 && r1, Range2 && r2)
        ->detail::non_graph_rngs_alg_ret_t<bool, Range1, Range2>
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::contains(
            std::begin(r1_), std::end(r1_), std::begin(r2_), std::end(r2_));
    }

    // Grapheme iterator overloads.

    template<
        typename GraphemeIter1,
        typename Sentinel1,
        typename GraphemeIter2,
        typename Sentinel2>
    auto find(
        GraphemeIter1 first1,
        Sentinel1 last1,
        GraphemeIter2 first2,
        Sentinel2 last2)
        ->detail::graph_iters_alg_ret_t<
            search_result<GraphemeIter1>,
            GraphemeIter1,
            GraphemeIter2>
    {
        return dtl::find(first1, last1, first2, last2);
    }

    template<typename GraphemeIter1, typename GraphemeIter2>
    auto rfind(
        GraphemeIter1 first1,
        GraphemeIter1 last1,
        GraphemeIter2 first2,
        GraphemeIter2 last2)
        ->detail::graph_iters_alg_ret_t<
            search_result<GraphemeIter1>,
            GraphemeIter1,
            GraphemeIter2>
    {
        return dtl::rfind(first1, last1, first2, last2);
    }

    template<
        typename GraphemeIter1,
        typename Sentinel1,
        typename GraphemeIter2,
        typename Sentinel2>
    auto find_first_of(
        GraphemeIter1 first1,
        Sentinel1 last1,
        GraphemeIter2 first2,
        Sentinel2 last2)
        ->detail::
            graph_iters_alg_ret_t<GraphemeIter1, GraphemeIter1, GraphemeIter2>
    {
        return dtl::find_first_of(first1, last1, first2, last2);
    }

    template<typename GraphemeIter1, typename GraphemeIter2>
    auto find_last_of(
        GraphemeIter1 first1,
        GraphemeIter1 last1,
        GraphemeIter2 first2,
        GraphemeIter2 last2)
        ->detail::
            graph_iters_alg_ret_t<GraphemeIter1, GraphemeIter1, GraphemeIter2>
    {
        return dtl::find_last_of(first1, last1, first2, last2);
    }

    template<
        typename GraphemeIter1,
        typename Sentinel1,
        typename GraphemeIter2,
        typename Sentinel2>
    auto find_first_not_of(
        GraphemeIter1 first1,
        Sentinel1 last1,
        GraphemeIter2 first2,
        Sentinel2 last2)
        ->detail::
            graph_iters_alg_ret_t<GraphemeIter1, GraphemeIter1, GraphemeIter2>
    {
        return dtl::find_first_not_of(first1, last1, first2, last2);
    }

    template<typename GraphemeIter1, typename GraphemeIter2>
    auto find_last_not_of(
        GraphemeIter1 first1,
        GraphemeIter1 last1,
        GraphemeIter2 first2,
        GraphemeIter2 last2)
        ->detail::
            graph_iters_alg_ret_t<GraphemeIter1, GraphemeIter1, GraphemeIter2>
    {
        return dtl::find_last_not_of(first1, last1, first2, last2);
    }

    template<
        typename GraphemeIter1,
        typename Sentinel1,
        typename GraphemeIter2,
        typename Sentinel2>
    auto starts_with(
        GraphemeIter1 first1,
        Sentinel1 last1,
        GraphemeIter2 first2,
        Sentinel2 last2)
        ->detail::graph_iters_alg_ret_t<bool, GraphemeIter1, GraphemeIter2>
    {
        return dtl::starts_with(first1, last1, first2, last2);
    }

    template<typename GraphemeIter1, typename GraphemeIter2>
    auto ends_with(
        GraphemeIter1 first1,
        GraphemeIter1 last1,
        GraphemeIter2 first2,
        GraphemeIter2 last2)
        ->detail::graph_iters_alg_ret_t<bool, GraphemeIter1, GraphemeIter2>
    {
        return dtl::ends_with(first1, last1, first2, last2);
    }

    template<
        typename GraphemeIter1,
        typename Sentinel1,
        typename GraphemeIter2,
        typename Sentinel2>
    auto contains(
        GraphemeIter1 first1,
        Sentinel1 last1,
        GraphemeIter2 first2,
        Sentinel2 last2)
        ->detail::graph_iters_alg_ret_t<bool, GraphemeIter1, GraphemeIter2>
    {
        return dtl::contains(first1, last1, first2, last2);
    }

    // Grapheme range overloads.

    namespace dtl {
        template<typename R>
        struct str_alg_gr_fwd_search_result
        {
            using type = search_result<
                detail::iterator_t<decltype(boost::text::as_graphemes(
                    detail::as_utf32_no_terminator(std::declval<R>())))>>;
        };
        template<typename R>
        struct str_alg_gr_rev_search_result
        {
            using type = search_result<
                detail::iterator_t<decltype(boost::text::as_graphemes(
                    detail::as_utf32_common_view_no_terminator(
                        std::declval<R>())))>>;
        };
        template<typename R>
        struct str_alg_gr_fwd_iter
        {
            using type = detail::iterator_t<decltype(boost::text::as_graphemes(
                detail::as_utf32_no_terminator(std::declval<R>())))>;
        };
        template<typename R>
        struct str_alg_gr_rev_iter
        {
            using type = detail::iterator_t<decltype(boost::text::as_graphemes(
                detail::as_utf32_common_view_no_terminator(
                    std::declval<R>())))>;
        };
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto find(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<
            search_result<detail::iterator_t<GraphemeRange1>>,
            GraphemeRange1,
            GraphemeRange2>
    {
        return dtl::find(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto find(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<
            search_result<detail::iterator_t<GraphemeRange>>,
            GraphemeRange,
            Range>
    {
        auto const r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::find(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto find(Range && r1, GraphemeRange && r2)
        ->detail::lazy_graph_rng_and_non_alg_ret_t<
            dtl::str_alg_gr_fwd_search_result<Range>,
            GraphemeRange,
            Range>
    {
        auto const r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::find(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto rfind(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<
            search_result<detail::iterator_t<GraphemeRange1>>,
            GraphemeRange1,
            GraphemeRange2>
    {
        return dtl::rfind(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto rfind(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<
            search_result<detail::iterator_t<GraphemeRange>>,
            GraphemeRange,
            Range>
    {
        auto const r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::rfind(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto rfind(Range && r1, GraphemeRange && r2)
        ->detail::lazy_graph_rng_and_non_alg_ret_t<
            dtl::str_alg_gr_rev_search_result<Range>,
            GraphemeRange,
            Range>
    {
        auto const r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::rfind(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto find_first_of(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<
            detail::iterator_t<GraphemeRange1>,
            GraphemeRange1,
            GraphemeRange2>
    {
        return dtl::find_first_of(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto find_first_of(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<
            detail::iterator_t<GraphemeRange>,
            GraphemeRange,
            Range>
    {
        auto const r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::find_first_of(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto find_first_of(Range && r1, GraphemeRange && r2)
        ->detail::lazy_graph_rng_and_non_alg_ret_t<
            dtl::str_alg_gr_fwd_iter<Range>,
            GraphemeRange,
            Range>
    {
        auto const r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::find_first_of(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto find_last_of(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<
            detail::iterator_t<GraphemeRange1>,
            GraphemeRange1,
            GraphemeRange2>
    {
        return dtl::find_last_of(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto find_last_of(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<
            detail::iterator_t<GraphemeRange>,
            GraphemeRange,
            Range>
    {
        auto const r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::find_last_of(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto find_last_of(Range && r1, GraphemeRange && r2)
        ->detail::lazy_graph_rng_and_non_alg_ret_t<
            dtl::str_alg_gr_rev_iter<Range>,
            GraphemeRange,
            Range>
    {
        auto const r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::find_last_of(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto find_first_not_of(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<
            detail::iterator_t<GraphemeRange1>,
            GraphemeRange1,
            GraphemeRange2>
    {
        return dtl::find_first_not_of(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto find_first_not_of(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<
            detail::iterator_t<GraphemeRange>,
            GraphemeRange,
            Range>
    {
        auto const r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::find_first_not_of(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto find_first_not_of(Range && r1, GraphemeRange && r2)
        ->detail::lazy_graph_rng_and_non_alg_ret_t<
            dtl::str_alg_gr_fwd_iter<Range>,
            GraphemeRange,
            Range>
    {
        auto const r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::find_first_not_of(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto find_last_not_of(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<
            detail::iterator_t<GraphemeRange1>,
            GraphemeRange1,
            GraphemeRange2>
    {
        return dtl::find_last_not_of(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto find_last_not_of(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<
            detail::iterator_t<GraphemeRange>,
            GraphemeRange,
            Range>
    {
        auto const r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::find_last_not_of(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto find_last_not_of(Range && r1, GraphemeRange && r2)
        ->detail::lazy_graph_rng_and_non_alg_ret_t<
            dtl::str_alg_gr_rev_iter<Range>,
            GraphemeRange,
            Range>
    {
        auto const r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::find_last_not_of(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto starts_with(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<bool, GraphemeRange1, GraphemeRange2>
    {
        return dtl::starts_with(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto starts_with(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<bool, GraphemeRange, Range>
    {
        auto const r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::starts_with(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto starts_with(Range && r1, GraphemeRange && r2)
        ->detail::graph_rng_and_non_alg_ret_t<bool, GraphemeRange, Range>
    {
        auto const r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::starts_with(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto ends_with(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<bool, GraphemeRange1, GraphemeRange2>
    {
        return dtl::ends_with(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto ends_with(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<bool, GraphemeRange, Range>
    {
        auto const r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::ends_with(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto ends_with(Range && r1, GraphemeRange && r2)
        ->detail::graph_rng_and_non_alg_ret_t<bool, GraphemeRange, Range>
    {
        auto const r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::ends_with(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto contains(GraphemeRange1 && r1, GraphemeRange2 && r2)
        ->detail::graph_rngs_alg_ret_t<bool, GraphemeRange1, GraphemeRange2>
    {
        return dtl::contains(
            std::begin(r1), std::end(r1), std::begin(r2), std::end(r2));
    }
    template<typename GraphemeRange, typename Range>
    auto contains(GraphemeRange && r1, Range && r2)
        ->detail::graph_rng_and_non_alg_ret_t<bool, GraphemeRange, Range>
    {
        auto const r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::contains(
            std::begin(r1), std::end(r1), std::begin(r2_), std::end(r2_));
    }
    template<typename Range, typename GraphemeRange>
    auto contains(Range && r1, GraphemeRange && r2)
        ->detail::graph_rng_and_non_alg_ret_t<bool, GraphemeRange, Range>
    {
        auto const r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::contains(
            std::begin(r1_), std::end(r1_), std::begin(r2), std::end(r2));
    }

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    namespace dtl {
        template<typename I1, typename S1, typename I2, typename S2>
        std::ranges::subrange<I1> find(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return std::ranges::search(
                first1, last1, first2, last2, std::equal_to{});
        }

        template<typename I1, typename I2>
        std::ranges::subrange<I1>
        rfind(I1 first1, I1 last1, I2 first2, I2 last2)
        {
            auto const result = std::ranges::search(
                std::make_reverse_iterator(last1),
                std::make_reverse_iterator(first1),
                std::make_reverse_iterator(last2),
                std::make_reverse_iterator(first2),
                std::equal_to{});
            if (result.empty())
                return {last1, last1};
            return {result.end().base(), result.begin().base()};
        }

        template<typename I1, typename S1, typename I2, typename S2>
        I1 find_first_of(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return std::ranges::find_first_of(
                first1, last1, first2, last2, std::equal_to{});
        }

        template<typename I1, typename I2>
        I1 find_last_of(I1 first1, I1 last1, I2 first2, I2 last2)
        {
            auto const last1_ = std::make_reverse_iterator(first1);
            auto const result = std::ranges::find_first_of(
                std::make_reverse_iterator(last1),
                last1_,
                std::make_reverse_iterator(last2),
                std::make_reverse_iterator(first2),
                std::equal_to{});
            if (result == last1_)
                return last1;
            return std::prev(result.base());
        }

        template<typename I1, typename S1, typename I2, typename S2>
        I1 find_first_not_of(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return std::ranges::find_if(
                first1, last1, [first2, last2](auto const & x) {
                    return std::ranges::find_if(
                               first2, last2, [&x](auto const & y) {
                                   return x == y;
                               }) == last2;
                });
        }

        template<typename I1, typename I2>
        I1 find_last_not_of(I1 first1_, I1 last1_, I2 first2_, I2 last2_)
        {
            auto const first1 = std::make_reverse_iterator(last1_);
            auto const last1 = std::make_reverse_iterator(first1_);
            auto const first2 = std::make_reverse_iterator(last2_);
            auto const last2 = std::make_reverse_iterator(first2_);
            auto const result = std::ranges::find_if(
                first1, last1, [first2, last2](auto const & x) {
                    return std::ranges::find_if(
                               first2, last2, [&x](auto const & y) {
                                   return x == y;
                               }) == last2;
                });
            if (result == last1)
                return last1_;
            return std::prev(result.base());
        }

        template<typename I1, typename S1, typename I2, typename S2>
        bool starts_with(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return std::ranges::mismatch(
                       first1, last1, first2, last2, std::equal_to{})
                       .in2 == last2;
        }

        template<typename I1, typename I2>
        bool ends_with(I1 first1, I1 last1, I2 first2, I2 last2)
        {
            auto const target = std::make_reverse_iterator(first2);
            return std::ranges::mismatch(
                       std::make_reverse_iterator(last1),
                       std::make_reverse_iterator(first1),
                       std::make_reverse_iterator(last2),
                       target,
                       std::equal_to{})
                       .in2 == target;
        }

        template<typename I1, typename S1, typename I2, typename S2>
        bool contains(I1 first1, S1 last1, I2 first2, S2 last2)
        {
            return dtl::find(first1, last1, first2, last2).begin() != last1;
        }
    }

    // Code point iterator overloads.

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2>
    std::ranges::subrange<I1> find(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::find(first1, last1, first2, last2);
    }

    template<code_point_iter I1, code_point_iter I2>
    std::ranges::subrange<I1> rfind(I1 first1, I1 last1, I2 first2, I2 last2)
    {
        return dtl::rfind(first1, last1, first2, last2);
    }

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2>
    I1 find_first_of(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::find_first_of(first1, last1, first2, last2);
    }

    template<code_point_iter I1, code_point_iter I2>
    I1 find_last_of(I1 first1, I1 last1, I2 first2, I2 last2)
    {
        return dtl::find_last_of(first1, last1, first2, last2);
    }

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2>
    I1 find_first_not_of(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::find_first_not_of(first1, last1, first2, last2);
    }

    template<code_point_iter I1, code_point_iter I2>
    I1 find_last_not_of(I1 first1, I1 last1, I2 first2, I2 last2)
    {
        return dtl::find_last_not_of(first1, last1, first2, last2);
    }

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2>
    bool starts_with(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::starts_with(first1, last1, first2, last2);
    }

    template<code_point_iter I1, code_point_iter I2>
    bool ends_with(I1 first1, I1 last1, I2 first2, I2 last2)
    {
        return dtl::ends_with(first1, last1, first2, last2);
    }

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2>
    bool contains(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::contains(first1, last1, first2, last2);
    }

    // Code point range overloads.

    template<utf_range_like R1, utf_range_like R2>
    auto find(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::find(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    template<utf_range_like R1, utf_range_like R2>
    auto rfind(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_common_view_no_terminator(r1);
        auto r2_ = detail::as_utf32_common_view_no_terminator(r2);
        return dtl::rfind(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    template<utf_range_like R1, utf_range_like R2>
    auto find_first_of(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::find_first_of(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    template<utf_range_like R1, utf_range_like R2>
    auto find_last_of(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_common_view_no_terminator(r1);
        auto r2_ = detail::as_utf32_common_view_no_terminator(r2);
        return dtl::find_last_of(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    template<utf_range_like R1, utf_range_like R2>
    auto find_first_not_of(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::find_first_not_of(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    template<utf_range_like R1, utf_range_like R2>
    auto find_last_not_of(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_common_view_no_terminator(r1);
        auto r2_ = detail::as_utf32_common_view_no_terminator(r2);
        return dtl::find_last_not_of(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    template<utf_range_like R1, utf_range_like R2>
    bool starts_with(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::starts_with(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    template<utf_range_like R1, utf_range_like R2>
    bool ends_with(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_common_view_no_terminator(r1);
        auto r2_ = detail::as_utf32_common_view_no_terminator(r2);
        return dtl::ends_with(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    template<utf_range_like R1, utf_range_like R2>
    bool contains(R1 && r1, R2 && r2)
    {
        auto r1_ = detail::as_utf32_no_terminator(r1);
        auto r2_ = detail::as_utf32_no_terminator(r2);
        return dtl::contains(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }

    // Grapheme iterator overloads.

    template<
        grapheme_iter I1,
        std::sentinel_for<I1> S1,
        grapheme_iter I2,
        std::sentinel_for<I2> S2>
    std::ranges::subrange<I1> find(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::find(first1, last1, first2, last2);
    }

    template<grapheme_iter I1, grapheme_iter I2>
    std::ranges::subrange<I1> rfind(I1 first1, I1 last1, I2 first2, I2 last2)
    {
        return dtl::rfind(first1, last1, first2, last2);
    }

    template<
        grapheme_iter I1,
        std::sentinel_for<I1> S1,
        grapheme_iter I2,
        std::sentinel_for<I2> S2>
    I1 find_first_of(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::find_first_of(first1, last1, first2, last2);
    }

    template<grapheme_iter I1, grapheme_iter I2>
    I1 find_last_of(I1 first1, I1 last1, I2 first2, I2 last2)
    {
        return dtl::find_last_of(first1, last1, first2, last2);
    }

    template<
        grapheme_iter I1,
        std::sentinel_for<I1> S1,
        grapheme_iter I2,
        std::sentinel_for<I2> S2>
    I1 find_first_not_of(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::find_first_not_of(first1, last1, first2, last2);
    }

    template<grapheme_iter I1, grapheme_iter I2>
    I1 find_last_not_of(I1 first1, I1 last1, I2 first2, I2 last2)
    {
        return dtl::find_last_not_of(first1, last1, first2, last2);
    }

    template<
        grapheme_iter I1,
        std::sentinel_for<I1> S1,
        grapheme_iter I2,
        std::sentinel_for<I2> S2>
    bool starts_with(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::starts_with(first1, last1, first2, last2);
    }

    template<grapheme_iter I1, grapheme_iter I2>
    bool ends_with(I1 first1, I1 last1, I2 first2, I2 last2)
    {
        return dtl::ends_with(first1, last1, first2, last2);
    }

    template<
        grapheme_iter I1,
        std::sentinel_for<I1> S1,
        grapheme_iter I2,
        std::sentinel_for<I2> S2>
    bool contains(I1 first1, S1 last1, I2 first2, S2 last2)
    {
        return dtl::contains(first1, last1, first2, last2);
    }

    // Grapheme range overloads.

    template<grapheme_range R1, grapheme_range R2>
    auto find(R1 && r1, R2 && r2)
    {
        return dtl::find(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    auto find(R1 && r1, R2 && r2)
    {
        auto r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::find(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    auto find(R1 && r1, R2 && r2)
    {
        auto r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::find(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

    template<grapheme_range R1, grapheme_range R2>
    auto rfind(R1 && r1, R2 && r2)
    {
        return dtl::rfind(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    auto rfind(R1 && r1, R2 && r2)
    {
        auto r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::rfind(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    auto rfind(R1 && r1, R2 && r2)
    {
        auto r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::rfind(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

    template<grapheme_range R1, grapheme_range R2>
    auto find_first_of(R1 && r1, R2 && r2)
    {
        return dtl::find_first_of(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    auto find_first_of(R1 && r1, R2 && r2)
    {
        auto r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::find_first_of(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    auto find_first_of(R1 && r1, R2 && r2)
    {
        auto r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::find_first_of(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

    template<grapheme_range R1, grapheme_range R2>
    auto find_last_of(R1 && r1, R2 && r2)
    {
        return dtl::find_last_of(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    auto find_last_of(R1 && r1, R2 && r2)
    {
        auto r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::find_last_of(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    auto find_last_of(R1 && r1, R2 && r2)
    {
        auto r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::find_last_of(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

    template<grapheme_range R1, grapheme_range R2>
    auto find_first_not_of(R1 && r1, R2 && r2)
    {
        return dtl::find_first_not_of(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    auto find_first_not_of(R1 && r1, R2 && r2)
    {
        auto r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::find_first_not_of(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    auto find_first_not_of(R1 && r1, R2 && r2)
    {
        auto r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::find_first_not_of(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

    template<grapheme_range R1, grapheme_range R2>
    auto find_last_not_of(R1 && r1, R2 && r2)
    {
        return dtl::find_last_not_of(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    auto find_last_not_of(R1 && r1, R2 && r2)
    {
        auto r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::find_last_not_of(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    auto find_last_not_of(R1 && r1, R2 && r2)
    {
        auto r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::find_last_not_of(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

    template<grapheme_range R1, grapheme_range R2>
    bool starts_with(R1 && r1, R2 && r2)
    {
        return dtl::starts_with(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    bool starts_with(R1 && r1, R2 && r2)
    {
        auto r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::starts_with(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    bool starts_with(R1 && r1, R2 && r2)
    {
        auto r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::starts_with(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

    template<grapheme_range R1, grapheme_range R2>
    bool ends_with(R1 && r1, R2 && r2)
    {
        return dtl::ends_with(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    bool ends_with(R1 && r1, R2 && r2)
    {
        auto r2_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r2));
        return dtl::ends_with(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    bool ends_with(R1 && r1, R2 && r2)
    {
        auto r1_ = boost::text::as_graphemes(
            detail::as_utf32_common_view_no_terminator(r1));
        return dtl::ends_with(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

    template<grapheme_range R1, grapheme_range R2>
    bool contains(R1 && r1, R2 && r2)
    {
        return dtl::contains(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }
    template<grapheme_range R1, utf_range_like R2>
    bool contains(R1 && r1, R2 && r2)
    {
        auto r2_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r2));
        return dtl::contains(
            std::ranges::begin(r1),
            std::ranges::end(r1),
            std::ranges::begin(r2_),
            std::ranges::end(r2_));
    }
    template<utf_range_like R1, grapheme_range R2>
    bool contains(R1 && r1, R2 && r2)
    {
        auto r1_ =
            boost::text::as_graphemes(detail::as_utf32_no_terminator(r1));
        return dtl::contains(
            std::ranges::begin(r1_),
            std::ranges::end(r1_),
            std::ranges::begin(r2),
            std::ranges::end(r2));
    }

}}}

#endif

#endif
