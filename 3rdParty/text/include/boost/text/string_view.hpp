// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_STRING_VIEW_HPP
#define BOOST_TEXT_STRING_VIEW_HPP

#include <boost/text/config.hpp>

#if !defined(__cpp_lib_string_view)
#include <boost/utility/string_view.hpp>
#else
#include <string_view>
#endif

#include <string>


namespace boost { namespace text {

#if defined(__cpp_lib_string_view)
    using string_view = std::string_view;
    template<typename T>
    using basic_string_view = std::basic_string_view<T>;
#else
    using string_view = boost::string_view;
    template<typename T>
    using basic_string_view = boost::basic_string_view<T>;
#endif

    namespace detail {
        template<typename Char>
        basic_string_view<Char>
        substring(basic_string_view<Char> sv, int lo, int hi)
        {
            if (lo < 0)
                lo = sv.size() + lo;
            if (hi < 0)
                hi = sv.size() + hi;
            return basic_string_view<Char>(sv.data() + lo, hi - lo);
        }
        template<typename Char, typename Traits, typename Allocator>
        basic_string_view<Char> substring(
            std::basic_string<Char, Traits, Allocator> const & s,
            int lo,
            int hi)
        {
            if (lo < 0)
                lo = s.size() + lo;
            if (hi < 0)
                hi = s.size() + hi;
            return basic_string_view<Char>(s.data() + lo, hi - lo);
        }
    }

}}

#endif
