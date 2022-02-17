// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_STRING_UTILITY_HPP
#define BOOST_TEXT_STRING_UTILITY_HPP

#include <boost/text/transcode_view.hpp>
#include <boost/text/detail/sentinel_tag.hpp>


namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

#ifdef BOOST_TEXT_DOXYGEN

    /** Returns a UTF-8-encoded string constructed from [first, last).

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept. */
    template<typename CPIter, typename Sentinel>
    std::string to_string(CPIter first, Sentinel last);

    /** Returns a UTF-8-encoded string constructed from range.

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept. */
    template<typename CPRange>
    std::string to_string(CPRange const & range);

#else

    template<typename CPIter, typename Sentinel>
    auto to_string(CPIter first, Sentinel last)
        -> detail::cp_iter_ret_t<std::string, CPIter>
    {
        auto const r = boost::text::as_utf8(first, last);
        return std::string(r.begin(), r.end());
    }

    template<typename CPRange>
    auto to_string(CPRange const & range)
        -> detail::cp_rng_alg_ret_t<std::string, CPRange>
    {
        return v1::to_string(std::begin(range), std::end(range));
    }

#endif

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    template<code_point_iter I, std::sentinel_for<I> S>
    std::string to_string(I first, S last)
    {
        auto const r = boost::text::as_utf8(first, last);
        return std::string(r.begin(), r.end());
    }

    template<code_point_range R>
    std::string to_string(R const & r)
    {
        return boost::text::to_string(
            std::ranges::begin(r), std::ranges::end(r));
    }

}}}

#endif

#endif
