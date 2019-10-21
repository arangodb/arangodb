/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2005 Thomas Guest
    Copyright (c) 2013, 2017 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_DETAIL_PATH_HPP)
#define BOOST_QUICKBOOK_DETAIL_PATH_HPP

#include <boost/filesystem/path.hpp>
#include "native_text.hpp"

namespace quickbook
{
    namespace fs = boost::filesystem;

    // The relative path from base to path
    fs::path path_difference(
        fs::path const& base, fs::path const& path, bool is_file = false);

    // Convert a Boost.Filesystem path to a URL.
    std::string file_path_to_url(fs::path const&);
    std::string dir_path_to_url(fs::path const&);

    namespace detail
    {
// 'generic':   Paths in quickbook source and the generated boostbook.
//              Always UTF-8.
// 'command_line':
//              Paths (or other parameters) from the command line and
//              possibly other sources in the future. Wide strings on
//              normal windows, UTF-8 for cygwin and other platforms
//              (hopefully).
// 'path':      Stored as a boost::filesystem::path. Since
//              Boost.Filesystem doesn't support cygwin, this
//              is always wide on windows. UTF-8 on other
//              platforms (again, hopefully).

#if QUICKBOOK_WIDE_PATHS
        typedef std::wstring command_line_string;
#else
        typedef std::string command_line_string;
#endif

        std::string command_line_to_utf8(command_line_string const&);
        fs::path command_line_to_path(command_line_string const&);

        std::string path_to_generic(fs::path const&);
        fs::path generic_to_path(quickbook::string_view);

        stream_string path_to_stream(fs::path const& path);

        // Command line parameters that might be a path, a url, or empty.
        // Not very efficient, but won't be used much.
        class path_or_url
        {
            int type_;
            boost::filesystem::path path_;
            std::string url_;

          public:
            // Creates an empty path_or_url.
            path_or_url();

            path_or_url(path_or_url const&);

            // Stores a parameter as either a path or a URL depending
            // on whether it looks like an absolute URL (i.e. starts with
            // 'scheme:')
            explicit path_or_url(command_line_string const&);

            path_or_url& operator=(path_or_url const&);

            path_or_url& operator=(command_line_string const&);

            void swap(path_or_url&);

            // Explicity create a URL
            static path_or_url url(string_view);

            // Explicitly create a path
            static path_or_url path(boost::filesystem::path const&);

            // Returns true if this isn't empty.
            operator bool() const;

            // Returns true if contains a path.
            bool is_path() const;

            // Returns true is contains a URL.
            bool is_url() const;

            // Returns the stored path.
            // pre: is_path()
            boost::filesystem::path const& get_path() const;

            // Returns the stored URL.
            // pre: is_url()
            std::string const& get_url() const;

            // Appends the value, either by path concatenation or URL
            // concatenation.
            // Note: a URL will strip text after the last '/', a path won't.
            //       Maybe should only work when the path is known to be a
            //       directory?
            path_or_url operator/(string_view) const;
        };
    }
}

#endif
