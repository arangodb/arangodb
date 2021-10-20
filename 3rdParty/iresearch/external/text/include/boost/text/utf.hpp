// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_UTF_HPP
#define BOOST_TEXT_UTF_HPP

#include <boost/text/config.hpp>

#include <type_traits>


namespace boost { namespace text {

    /** The Unicode Transformation Formats. */
    enum class format { utf8 = 1, utf16 = 2, utf32 = 4 };

    namespace detail {
        template<typename T>
        constexpr format format_of()
        {
            constexpr uint32_t size = sizeof(T);
            static_assert(std::is_integral<T>::value, "");
            static_assert(size == 1 || size == 2 || size == 4, "");
            constexpr format formats[] = {
                format::utf8,
                format::utf8,
                format::utf16,
                format::utf32,
                format::utf32};
            return formats[size];
        }
    }

}}

#endif
