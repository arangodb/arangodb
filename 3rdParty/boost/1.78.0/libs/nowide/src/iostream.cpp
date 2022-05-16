//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//  Copyright (c) 2020-2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_NOWIDE_SOURCE
#include <boost/nowide/iostream.hpp>

#ifndef BOOST_WINDOWS

namespace boost {
namespace nowide {
    // LCOV_EXCL_START
    /// Avoid empty compilation unit warnings
    /// \internal
    BOOST_NOWIDE_DECL void dummy_exported_function()
    {}
    // LCOV_EXCL_STOP
} // namespace nowide
} // namespace boost

#else

#include "console_buffer.hpp"
#include <cassert>
#include <iostream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace boost {
namespace nowide {
    namespace detail {

        namespace {
            bool is_atty_handle(HANDLE h)
            {
                DWORD dummy;
                return h && GetConsoleMode(h, &dummy) != FALSE;
            }
        } // namespace

        class console_output_buffer final : public console_output_buffer_base
        {
            HANDLE handle_;

        public:
            explicit console_output_buffer(HANDLE handle) : handle_(handle)
            {}

        protected:
            bool
            do_write(const wchar_t* buffer, std::size_t num_chars_to_write, std::size_t& num_chars_written) override
            {
                DWORD size = 0;
                const bool result =
                  WriteConsoleW(handle_, buffer, static_cast<DWORD>(num_chars_to_write), &size, 0) != 0;
                num_chars_written = size;
                return result;
            }
        };

        class console_input_buffer final : public console_input_buffer_base
        {
            HANDLE handle_;

        public:
            explicit console_input_buffer(HANDLE handle) : handle_(handle)
            {}

        protected:
            // Can't test this on CI so exclude
            // LCOV_EXCL_START
            bool do_read(wchar_t* buffer, std::size_t num_chars_to_read, std::size_t& num_chars_read) override
            {
                DWORD size = 0;                                                                 // LCOV_EXCL_LINE
                const auto to_read_size = static_cast<DWORD>(num_chars_to_read);                // LCOV_EXCL_LINE
                const bool result = ReadConsoleW(handle_, buffer, to_read_size, &size, 0) != 0; // LCOV_EXCL_LINE
                num_chars_read = size;                                                          // LCOV_EXCL_LINE
                return result;                                                                  // LCOV_EXCL_LINE
            }
            // LCOV_EXCL_STOP
        };

        winconsole_ostream::winconsole_ostream(int fd, winconsole_ostream* tieStream) : std::ostream(0)
        {
            HANDLE h = 0;
            switch(fd)
            {
            case 1: h = GetStdHandle(STD_OUTPUT_HANDLE); break;
            case 2: h = GetStdHandle(STD_ERROR_HANDLE); break;
            }
            if(is_atty_handle(h))
            {
                d.reset(new console_output_buffer(h));
                std::ostream::rdbuf(d.get());
            } else
            {
                std::ostream::rdbuf(fd == 1 ? std::cout.rdbuf() : std::cerr.rdbuf());
                assert(rdbuf());
            }
            if(tieStream)
            {
                tie(tieStream);
                setf(ios_base::unitbuf); // If tieStream is set, this is cerr -> set unbuffered
            }
        }
        winconsole_ostream::~winconsole_ostream()
        {
            try
            {
                flush();
            } catch(...)
            {} // LCOV_EXCL_LINE
        }

        winconsole_istream::winconsole_istream(winconsole_ostream* tieStream) : std::istream(0)
        {
            HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
            if(is_atty_handle(h))
            {
                d.reset(new console_input_buffer(h));
                std::istream::rdbuf(d.get());
            } else
            {
                std::istream::rdbuf(std::cin.rdbuf());
                assert(rdbuf());
            }
            if(tieStream)
                tie(tieStream);
        }

        winconsole_istream::~winconsole_istream()
        {}

    } // namespace detail

    // Make sure those are initialized as early as possible
#ifdef BOOST_MSVC
#pragma warning(disable : 4073)
#pragma init_seg(lib)
#endif
#ifdef BOOST_NOWIDE_HAS_INIT_PRIORITY
#define BOOST_NOWIDE_INIT_PRIORITY __attribute__((init_priority(101)))
#else
#define BOOST_NOWIDE_INIT_PRIORITY
#endif
    detail::winconsole_ostream cout BOOST_NOWIDE_INIT_PRIORITY(1, nullptr);
    detail::winconsole_istream cin BOOST_NOWIDE_INIT_PRIORITY(&cout);
    detail::winconsole_ostream cerr BOOST_NOWIDE_INIT_PRIORITY(2, &cout);
    detail::winconsole_ostream clog BOOST_NOWIDE_INIT_PRIORITY(2, nullptr);
} // namespace nowide
} // namespace boost

#endif
