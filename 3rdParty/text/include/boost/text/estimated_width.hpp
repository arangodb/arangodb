// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_ESTIMATED_WIDTH_HPP
#define BOOST_TEXT_ESTIMATED_WIDTH_HPP

#include <boost/text/concepts.hpp>
#include <boost/text/grapheme_view.hpp>


namespace boost { namespace text { namespace detail {
    inline int width_implied_by_cp(uint32_t cp)
    {
        using pair_type = std::pair<uint32_t, uint32_t>;
        constexpr pair_type double_wides[] = {
            {0x1100, 0x115f},
            {0x2329, 0x232a},
            {0x2e80, 0x303e},
            {0x3040, 0xa4cf},
            {0xac00, 0xd7a3},
            {0xf900, 0xfaff},
            {0xfe10, 0xfe19},
            {0xfe30, 0xfe6f},
            {0xff00, 0xff60},
            {0xffe0, 0xffe6},
            {0x1f300, 0x1f64f},
            {0x1f900, 0x1f9ff},
            {0x20000, 0x2fffd},
            {0x30000, 0x3fffd},
        };
        auto const last = std::end(double_wides);
        auto const it = std::lower_bound(
            std::begin(double_wides), last, cp, [](pair_type y, uint32_t x) {
                return y.second < x;
            });
        if (it != last && it->first <= cp && cp <= it->second)
            return 2;
        return 1;
    }

    template<typename I, typename S>
    std::size_t estimated_width_of_graphemes_impl(I first, S last)
    {
        std::size_t retval = 0;
        for (auto grapheme : boost::text::as_graphemes(first, last)) {
            retval += detail::width_implied_by_cp(grapheme.front());
        }
        return retval;
    }
}}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    /** Returns the estimated width of the graphemes within `[first, last)`,
        using the same technique that used by `std::format()`.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        \see [format.string.std] for a full description */
    template<typename CPIter, typename Sentinel>
    auto estimated_width_of_graphemes(CPIter first, Sentinel last)
        ->detail::cp_iter_ret_t<std::size_t, CPIter>
    {
        return detail::estimated_width_of_graphemes_impl(first, last);
    }

    /** Returns the estimated width of the graphemes within `r`, using the
        same technique that used by `std::format()`.

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept.

        \see [format.string.std] for a full description */
    template<typename CPRange>
    auto estimated_width_of_graphemes(CPRange && r)
        ->decltype(detail::estimated_width_of_graphemes_impl(
            std::begin(r), std::end(r)))
    {
        return detail::estimated_width_of_graphemes_impl(
            std::begin(r), std::end(r));
    }

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    /** Returns the estimated width of the graphemes within `[first, last)`,
        using the same technique that used by `std::format()`.

        \see [format.string.std] for a full description */
    template<code_point_iter I, std::sentinel_for<I> S>
    std::size_t estimated_width_of_graphemes(I first, S last)
    {
        return detail::estimated_width_of_graphemes_impl(first, last);
    }

    /** Returns the estimated width of the graphemes within `r`, using the
        same technique that used by `std::format()`.

        \see [format.string.std] for a full description */
    template<code_point_range R>
    std::size_t estimated_width_of_graphemes(R && r)
    {
        return detail::estimated_width_of_graphemes_impl(
            std::ranges::begin(r), std::ranges::end(r));
    }

}}}

#endif

#endif
