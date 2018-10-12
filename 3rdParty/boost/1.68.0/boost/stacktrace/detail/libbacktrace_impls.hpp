// Copyright Antony Polukhin, 2016-2018.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_DETAIL_LIBBACKTRACE_IMPLS_HPP
#define BOOST_STACKTRACE_DETAIL_LIBBACKTRACE_IMPLS_HPP

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#include <boost/stacktrace/detail/to_hex_array.hpp>
#include <boost/stacktrace/detail/to_dec_array.hpp>
#include <boost/stacktrace/detail/location_from_symbol.hpp>
#include <boost/core/demangle.hpp>

#include <backtrace.h>

namespace boost { namespace stacktrace { namespace detail {


struct pc_data {
    std::string* function;
    std::string* filename;
    std::size_t line;
};

inline int libbacktrace_full_callback(void *data, uintptr_t /*pc*/, const char *filename, int lineno, const char *function) {
    pc_data& d = *static_cast<pc_data*>(data);
    if (d.filename && filename) {
        *d.filename = filename;
    }
    if (d.function && function) {
        *d.function = function;
    }
    d.line = lineno;
    return 0;
}

inline void libbacktrace_error_callback(void* /*data*/, const char* /*msg*/, int /*errnum*/) BOOST_NOEXCEPT {
    // Do nothing, just return.
}


inline ::backtrace_state* construct_state(const program_location& prog_location) BOOST_NOEXCEPT {
    // Currently `backtrace_create_state` can not detect file name on Windows https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82543
    // That's why we provide a `prog_location` here.
    return ::backtrace_create_state(
        prog_location.name(), 0 /*thread-safe*/, boost::stacktrace::detail::libbacktrace_error_callback, 0
    );

    // TODO: this does not seem to work well when this function is in .so:
    // Not async-signal-safe, so this method is not called from async-safe functions.
    //
    // This function is not async signal safe because:
    // * Dynamic initialization of a block-scope variable with static storage duration could lock a mutex
    // * No guarantees on `backtrace_create_state` function.

    // [dcl.inline]: A static local variable in an inline function with external linkage always refers to the same object.

    /*  
    static ::backtrace_state* state = ::backtrace_create_state(
        0, 1 , boost::stacktrace::detail::libbacktrace_error_callback, 0
    );

    return state;
    */
}

struct to_string_using_backtrace {
    std::string res;
    boost::stacktrace::detail::program_location prog_location;
    ::backtrace_state* state;
    std::string filename;
    std::size_t line;

    void prepare_function_name(const void* addr) {
        boost::stacktrace::detail::pc_data data = {&res, &filename, 0};
        if (state) {
            ::backtrace_pcinfo(
                state,
                reinterpret_cast<uintptr_t>(addr),
                boost::stacktrace::detail::libbacktrace_full_callback,
                boost::stacktrace::detail::libbacktrace_error_callback,
                &data
            );
        }
        line = data.line;
    }

    bool prepare_source_location(const void* /*addr*/) {
        if (filename.empty() || !line) {
            return false;
        }

        res += " at ";
        res += filename;
        res += ':';
        res += boost::stacktrace::detail::to_dec_array(line).data();
        return true;
    }

    to_string_using_backtrace() BOOST_NOEXCEPT {
        state = boost::stacktrace::detail::construct_state(prog_location);
    }
};

template <class Base> class to_string_impl_base;
typedef to_string_impl_base<to_string_using_backtrace> to_string_impl;

inline std::string name_impl(const void* addr) {
    std::string res;

    boost::stacktrace::detail::program_location prog_location;
    ::backtrace_state* state = boost::stacktrace::detail::construct_state(prog_location);

    boost::stacktrace::detail::pc_data data = {&res, 0, 0};
    if (state) {
        ::backtrace_pcinfo(
            state,
            reinterpret_cast<uintptr_t>(addr),
            boost::stacktrace::detail::libbacktrace_full_callback,
            boost::stacktrace::detail::libbacktrace_error_callback,
            &data
        );
    }
    if (!res.empty()) {
        res = boost::core::demangle(res.c_str());
    }

    return res;
}

} // namespace detail

std::string frame::source_file() const {
    std::string res;

    boost::stacktrace::detail::program_location prog_location;
    ::backtrace_state* state = boost::stacktrace::detail::construct_state(prog_location);

    boost::stacktrace::detail::pc_data data = {0, &res, 0};
    if (state) {
        ::backtrace_pcinfo(
            state,
            reinterpret_cast<uintptr_t>(addr_),
            boost::stacktrace::detail::libbacktrace_full_callback,
            boost::stacktrace::detail::libbacktrace_error_callback,
            &data
        );
    }

    return res;
}

std::size_t frame::source_line() const {
    boost::stacktrace::detail::program_location prog_location;
    ::backtrace_state* state = boost::stacktrace::detail::construct_state(prog_location);

    boost::stacktrace::detail::pc_data data = {0, 0, 0};
    if (state) {
        ::backtrace_pcinfo(
            state,
            reinterpret_cast<uintptr_t>(addr_),
            boost::stacktrace::detail::libbacktrace_full_callback,
            boost::stacktrace::detail::libbacktrace_error_callback,
            &data
        );
    }

    return data.line;
}


}} // namespace boost::stacktrace

#endif // BOOST_STACKTRACE_DETAIL_LIBBACKTRACE_IMPLS_HPP
