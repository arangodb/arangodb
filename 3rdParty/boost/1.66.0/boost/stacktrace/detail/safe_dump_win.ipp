// Copyright Antony Polukhin, 2016-2017.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_DETAIL_SAFE_DUMP_WIN_IPP
#define BOOST_STACKTRACE_DETAIL_SAFE_DUMP_WIN_IPP

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#include <boost/stacktrace/safe_dump_to.hpp>

#include <boost/core/noncopyable.hpp>

#include <boost/detail/winapi/get_current_process.hpp>
#include <boost/detail/winapi/file_management.hpp>
#include <boost/detail/winapi/handles.hpp>
#include <boost/detail/winapi/access_rights.hpp>

namespace boost { namespace stacktrace { namespace detail {

std::size_t dump(void* fd, const native_frame_ptr_t* frames, std::size_t frames_count) BOOST_NOEXCEPT {
    boost::detail::winapi::DWORD_ written;
    const boost::detail::winapi::DWORD_ bytes_to_write = static_cast<boost::detail::winapi::DWORD_>(
        sizeof(native_frame_ptr_t) * frames_count
    );
    if (!boost::detail::winapi::WriteFile(fd, frames, bytes_to_write, &written, 0)) {
        return 0;
    }

    return frames_count;
}

std::size_t dump(const char* file, const native_frame_ptr_t* frames, std::size_t frames_count) BOOST_NOEXCEPT {
    void* const fd = boost::detail::winapi::CreateFileA(
        file,
        boost::detail::winapi::GENERIC_WRITE_,
        0,
        0,
        boost::detail::winapi::CREATE_ALWAYS_,
        boost::detail::winapi::FILE_ATTRIBUTE_NORMAL_,
        0
    );

    if (fd == boost::detail::winapi::invalid_handle_value) {
        return 0;
    }

    const std::size_t size = boost::stacktrace::detail::dump(fd, frames, frames_count);
    boost::detail::winapi::CloseHandle(fd);
    return size;
}

}}} // namespace boost::stacktrace::detail

#endif // BOOST_STACKTRACE_DETAIL_SAFE_DUMP_WIN_IPP
