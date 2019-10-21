#ifndef BOOST_SAFE_FORMAT_HPP
#define BOOST_SAFE_FORMAT_HPP

//  Copyright (c) 2015 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <ostream>
#include <typeinfo>

#include <boost/core/demangle.hpp>
#include <boost/safe_numerics/safe_common.hpp>

namespace {

// create an output manipulator which prints variable type and limits
// as well as value
template<typename T>
struct safe_format_impl {
    const T & m_t;
    safe_format_impl(const T & t) :
        m_t(t)
    {}
    template <class charT, class Traits>
    friend std::basic_ostream<charT,Traits> &
    operator<<(
        std::basic_ostream<charT,Traits> & os,
        const safe_format_impl<T> & f
    ){
        return os
            << "<"
            << boost::core::demangle(typeid(
                typename boost::safe_numerics::base_type<T>::type
                ).name()
            )
            << ">["
            << std::numeric_limits<T>::min() << ","
            << std::numeric_limits<T>::max() << "] = "
            << f.m_t;
    }
};

} // anonymous namespace

template<typename T>
auto safe_format(const T & t){
    return safe_format_impl<T>(t);
}

#endif // BOOST_SAFE_FORMAT_HPP
