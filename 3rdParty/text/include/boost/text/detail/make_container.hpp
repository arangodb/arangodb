// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_MAKE_CONTAINER_HPP
#define BOOST_TEXT_DETAIL_MAKE_CONTAINER_HPP

#include <boost/text/algorithm.hpp>


namespace boost { namespace text { namespace detail {

    template<typename Container, typename Iter>
    auto make_container(Iter first, Iter last)
    {
        return Container(first, last);
    }
    template<typename Container, typename Iter, typename Sentinel>
    auto make_container(Iter first, Sentinel last)
    {
        return Container(
            first, std::next(first, boost::text::distance(first, last)));
    }

}}}

#endif
