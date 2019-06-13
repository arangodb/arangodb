/*=============================================================================
    Copyright (c) 2009 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "stream.hpp"
#include "files.hpp"
#include "path.hpp"

#if QUICKBOOK_WIDE_PATHS || QUICKBOOK_WIDE_STREAMS
#include <fcntl.h>
#include <io.h>
#endif

namespace quickbook
{
    namespace detail
    {
        namespace
        {
            bool ms_errors = false;
        }

        void set_ms_errors(bool x) { ms_errors = x; }

#if QUICKBOOK_WIDE_STREAMS

        void initialise_output()
        {
            if (_isatty(_fileno(stdout))) _setmode(_fileno(stdout), _O_U16TEXT);
            if (_isatty(_fileno(stderr))) _setmode(_fileno(stderr), _O_U16TEXT);
        }

        void write_utf8(ostream::base_ostream& out, quickbook::string_view x)
        {
            out << from_utf8(x);
        }

        ostream& out()
        {
            static ostream x(std::wcout);
            return x;
        }

        namespace
        {
            inline ostream& error_stream()
            {
                static ostream x(std::wcerr);
                return x;
            }
        }

#else

        void initialise_output() {}

        void write_utf8(ostream::base_ostream& out, quickbook::string_view x)
        {
            out << x;
        }

        ostream& out()
        {
            static ostream x(std::cout);
            return x;
        }

        namespace
        {
            inline ostream& error_stream()
            {
                static ostream x(std::clog);
                return x;
            }
        }

#endif

        ostream& outerr() { return error_stream() << "Error: "; }

        ostream& outerr(fs::path const& file, std::ptrdiff_t line)
        {
            if (line >= 0) {
                if (ms_errors)
                    return error_stream() << path_to_stream(file) << "(" << line
                                          << "): error: ";
                else
                    return error_stream() << path_to_stream(file) << ":" << line
                                          << ": error: ";
            }
            else {
                return error_stream() << path_to_stream(file) << ": error: ";
            }
        }

        ostream& outerr(file_ptr const& f, string_iterator pos)
        {
            return outerr(f->path, f->position_of(pos).line);
        }

        ostream& outwarn(fs::path const& file, std::ptrdiff_t line)
        {
            if (line >= 0) {
                if (ms_errors)
                    return error_stream() << path_to_stream(file) << "(" << line
                                          << "): warning: ";
                else
                    return error_stream() << path_to_stream(file) << ":" << line
                                          << ": warning: ";
            }
            else {
                return error_stream() << path_to_stream(file) << ": warning: ";
            }
        }

        ostream& outwarn(file_ptr const& f, string_iterator pos)
        {
            return outwarn(f->path, f->position_of(pos).line);
        }

        ostream& ostream::operator<<(char c)
        {
            assert(c && !(c & 0x80));
            base << c;
            return *this;
        }

        inline bool check_ascii(char const* x)
        {
            for (; *x; ++x)
                if (*x & 0x80) return false;
            return true;
        }

        ostream& ostream::operator<<(char const* x)
        {
            assert(check_ascii(x));
            base << x;
            return *this;
        }

        ostream& ostream::operator<<(std::string const& x)
        {
            write_utf8(base, x);
            return *this;
        }

        ostream& ostream::operator<<(quickbook::string_view x)
        {
            write_utf8(base, x);
            return *this;
        }

        ostream& ostream::operator<<(int x)
        {
            base << x;
            return *this;
        }

        ostream& ostream::operator<<(unsigned int x)
        {
            base << x;
            return *this;
        }

        ostream& ostream::operator<<(long x)
        {
            base << x;
            return *this;
        }

        ostream& ostream::operator<<(unsigned long x)
        {
            base << x;
            return *this;
        }

#if !defined(BOOST_NO_LONG_LONG)
        ostream& ostream::operator<<(boost::long_long_type x)
        {
            base << x;
            return *this;
        }

        ostream& ostream::operator<<(boost::ulong_long_type x)
        {
            base << x;
            return *this;
        }
#endif

        ostream& ostream::operator<<(fs::path const& x)
        {
            base << path_to_stream(x);
            return *this;
        }

        ostream& ostream::operator<<(base_ostream& (*x)(base_ostream&))
        {
            base << x;
            return *this;
        }

        ostream& ostream::operator<<(base_ios& (*x)(base_ios&))
        {
            base << x;
            return *this;
        }
    }
}
