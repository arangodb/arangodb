// Copyright Antony Polukhin, 2016-2017.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


//[getting_started_user_config
#ifndef USER_CONFIG_HPP
#define USER_CONFIG_HPP

#include <boost/stacktrace/stacktrace_fwd.hpp>

#include <iosfwd>

namespace boost { namespace stacktrace {

template <class CharT, class TraitsT, class Allocator>
std::basic_ostream<CharT, TraitsT>& do_stream_st(std::basic_ostream<CharT, TraitsT>& os, const basic_stacktrace<Allocator>& bt);

template <class CharT, class TraitsT>
std::basic_ostream<CharT, TraitsT>& operator<<(std::basic_ostream<CharT, TraitsT>& os, const stacktrace& bt) {
    return do_stream_st(os, bt);
}

}}  // namespace boost::stacktrace
#endif // USER_CONFIG_HPP
//]

#ifndef USER_CONFIG2_HPP
#define USER_CONFIG2_HPP
//[getting_started_user_config_impl
namespace boost { namespace stacktrace {

template <class CharT, class TraitsT, class Allocator>
std::basic_ostream<CharT, TraitsT>& do_stream_st(std::basic_ostream<CharT, TraitsT>& os, const basic_stacktrace<Allocator>& bt) {
    const std::streamsize w = os.width();
    const std::size_t frames = bt.size();
    for (std::size_t i = 0; i < frames; ++i) {
        os.width(2);
        os << i;
        os.width(w);
        os << "# ";
        os << bt[i].name();
        os << '\n';
    }

    return os;
}

}}  // namespace boost::stacktrace
//]

#endif // USER_CONFIG2_HPP

