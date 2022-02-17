// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_ROPE_HPP
#define BOOST_TEXT_DETAIL_ROPE_HPP

#include <boost/text/string_view.hpp>
#include <boost/text/transcode_iterator.hpp>

#include <boost/text/detail/btree.hpp>


namespace boost { namespace text { namespace detail {

    constexpr std::size_t string_insert_max = BOOST_TEXT_STRING_INSERT_MAX;

    static_assert(sizeof(node_ptr<char, std::string>) * 8 <= 64, "");

    struct segment_inserter
    {
        template<typename Iter, typename Sentinel>
        void operator()(Iter first, Sentinel last) const
        {
            for (; os_.good() && first != last; ++first) {
                os_ << *first;
            }
        }

        std::ostream & os_;
    };

}}}

#endif
