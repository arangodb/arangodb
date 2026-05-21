// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_MAKE_STRING_HPP
#define BOOST_TEXT_DETAIL_MAKE_STRING_HPP

#include <boost/text/detail/sentinel_tag.hpp>


namespace boost { namespace text { namespace detail {

    template<typename String, typename CharIter>
    auto make_string(CharIter first, CharIter last)
    {
        return String(first, last);
    }
    template<typename String, typename Char>
    auto make_string(Char const * first, boost::text::null_sentinel)
    {
        basic_string_view<Char> sv(first);
        return String(sv.begin(), sv.end());
    }
    template<typename String, typename CharIter, typename Sentinel>
    auto make_string(CharIter first, Sentinel last)
    {
        return String(
            first, std::next(first, boost::text::distance(first, last)));
    }

}}}

#endif
