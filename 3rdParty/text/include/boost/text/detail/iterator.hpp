// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_ITERATOR_HPP
#define BOOST_TEXT_DETAIL_ITERATOR_HPP

#include <boost/text/config.hpp>
#include <boost/stl_interfaces/reverse_iterator.hpp>

#include <iterator>


namespace boost { namespace text { namespace detail {

    using reverse_char_iterator = stl_interfaces::reverse_iterator<char *>;
    using const_reverse_char_iterator =
        stl_interfaces::reverse_iterator<char const *>;

}}}

#endif
