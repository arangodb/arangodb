/*=============================================================================
    Copyright (c) 2009 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// For handling native strings and streams.

#if !defined(BOOST_QUICKBOOK_DETAIL_STREAM_HPP)
#define BOOST_QUICKBOOK_DETAIL_STREAM_HPP

#include <iostream>
#include <boost/filesystem/path.hpp>
#include "native_text.hpp"

namespace quickbook
{
    namespace fs = boost::filesystem;

    namespace detail
    {
        // A light wrapper around C++'s streams that gets things right
        // in the quickbook context.
        //
        // This is far from perfect but it fixes some issues.
        struct ostream
        {
            typedef stream_string string;
#if QUICKBOOK_WIDE_STREAMS
            typedef std::wostream base_ostream;
            typedef std::wios base_ios;
#else
            typedef std::ostream base_ostream;
            typedef std::ios base_ios;
#endif
            base_ostream& base;

            explicit ostream(base_ostream& x) : base(x) {}

            // C strings should always be ascii.
            ostream& operator<<(char);
            ostream& operator<<(char const*);

            // std::string should be UTF-8 (what a mess!)
            ostream& operator<<(std::string const&);
            ostream& operator<<(quickbook::string_view);

            // Other value types.
            ostream& operator<<(int x);
            ostream& operator<<(unsigned int x);
            ostream& operator<<(long x);
            ostream& operator<<(unsigned long x);

#if !defined(BOOST_NO_LONG_LONG)
            ostream& operator<<(boost::long_long_type x);
            ostream& operator<<(boost::ulong_long_type x);
#endif

            ostream& operator<<(fs::path const&);

            // Modifiers
            ostream& operator<<(base_ostream& (*)(base_ostream&));
            ostream& operator<<(base_ios& (*)(base_ios&));
        };

        void initialise_output();

        ostream& out();

        // Preformats an error/warning message so that it can be parsed by
        // common IDEs. Set 'ms_errors' to determine if VS format
        // or GCC format. Returns the stream to continue ouput of the verbose
        // error message.
        void set_ms_errors(bool);
        ostream& outerr();
        ostream& outerr(fs::path const& file, std::ptrdiff_t line = -1);
        ostream& outwarn(fs::path const& file, std::ptrdiff_t line = -1);
        ostream& outerr(file_ptr const&, string_iterator);
        ostream& outwarn(file_ptr const&, string_iterator);
    }
}

#endif
