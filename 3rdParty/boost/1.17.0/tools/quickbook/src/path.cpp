/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2005 Thomas Guest
    Copyright (c) 2013, 2017 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "path.hpp"
#include <cassert>
#include <boost/filesystem/operations.hpp>
#include <boost/range/algorithm/replace.hpp>
#include "for.hpp"
#include "glob.hpp"
#include "include_paths.hpp"
#include "state.hpp"
#include "utils.hpp"

#if QUICKBOOK_CYGWIN_PATHS
#include <sys/cygwin.h>
#include <boost/scoped_array.hpp>
#endif

namespace quickbook
{
    // Not a general purpose normalization function, just
    // from paths from the root directory. It strips the excess
    // ".." parts from a path like: "x/../../y", leaving "y".
    std::vector<fs::path> remove_dots_from_path(fs::path const& path)
    {
        assert(!path.has_root_directory() && !path.has_root_name());

        std::vector<fs::path> parts;

        QUICKBOOK_FOR (fs::path const& part, path) {
            if (part.empty() || part == ".") {
            }
            else if (part == "..") {
                if (!parts.empty()) parts.pop_back();
            }
            else {
                parts.push_back(part);
            }
        }

        return parts;
    }

    // The relative path from base to path
    fs::path path_difference(
        fs::path const& base, fs::path const& path, bool is_file)
    {
        fs::path absolute_base = fs::absolute(base),
                 absolute_path = fs::absolute(path);

        // Remove '.', '..' and empty parts from the remaining path
        std::vector<fs::path> base_parts = remove_dots_from_path(
                                  absolute_base.relative_path()),
                              path_parts = remove_dots_from_path(
                                  absolute_path.relative_path());

        std::vector<fs::path>::iterator base_it = base_parts.begin(),
                                        base_end = base_parts.end(),
                                        path_it = path_parts.begin(),
                                        path_end = path_parts.end();

        // Build up the two paths in these variables, checking for the first
        // difference.
        fs::path base_tmp = absolute_base.root_path(),
                 path_tmp = absolute_path.root_path();

        fs::path result;

        // If they have different roots then there's no relative path so
        // just build an absolute path.
        if (!fs::equivalent(base_tmp, path_tmp)) {
            result = path_tmp;
        }
        else {
            // Find the point at which the paths differ
            for (; base_it != base_end && path_it != path_end;
                 ++base_it, ++path_it) {
                base_tmp /= *base_it;
                path_tmp /= *path_it;
                if (*base_it != *path_it) {
                    if (!fs::exists(base_tmp) || !fs::exists(path_tmp) ||
                        !fs::equivalent(base_tmp, path_tmp)) {
                        break;
                    }
                }
            }

            if (is_file && path_it == path_end &&
                path_it != path_parts.begin()) {
                --path_it;
                result = "..";
            }
            else if (base_it == base_end && path_it == path_end) {
                result = ".";
            }

            // Build a relative path to that point
            for (; base_it != base_end; ++base_it)
                result /= "..";
        }

        // Build the rest of our path
        for (; path_it != path_end; ++path_it)
            result /= *path_it;

        return result;
    }

    // Convert a Boost.Filesystem path to a URL.
    //
    // I'm really not sure about this, as the meaning of root_name and
    // root_directory are only clear for windows.
    //
    // Some info on file URLs at:
    // https://en.wikipedia.org/wiki/File_URI_scheme
    std::string file_path_to_url_impl(fs::path const& x, bool is_dir)
    {
        fs::path::const_iterator it = x.begin(), end = x.end();
        if (it == end) {
            return is_dir ? "./" : "";
        }

        std::string result;
        bool sep = false;
        std::string part;
        if (x.has_root_name()) {
            // Handle network address (e.g. \\example.com)
            part = detail::path_to_generic(*it);
            if (part.size() >= 2 && part[0] == '/' && part[1] == '/') {
                result = "file:" + detail::escape_uri(part);
                sep = true;
                ++it;
                if (it != end && *it == "/") {
                    result += "/";
                    sep = false;
                    ++it;
                }
            }
            else {
                result = "file:///";
            }

            // Handle windows root (e.g. c:)
            if (it != end) {
                part = detail::path_to_generic(*it);
                if (part.size() >= 2 && part[part.size() - 1] == ':') {
                    result +=
                        detail::escape_uri(part.substr(0, part.size() - 1));
                    result += ':';
                    sep = false;
                    ++it;
                }
            }
        }
        else if (x.has_root_directory()) {
            result = "file://";
            sep = true;
        }
        else if (*it == ".") {
            result = ".";
            sep = true;
            ++it;
        }

        for (; it != end; ++it) {
            part = detail::path_to_generic(*it);
            if (part == "/") {
                result += "/";
                sep = false;
            }
            else if (part == ".") {
                // If the path has a trailing slash, write it out,
                // even if is_dir is false.
                if (sep) {
                    result += "/";
                    sep = false;
                }
            }
            else {
                if (sep) {
                    result += "/";
                }
                result += detail::escape_uri(detail::path_to_generic(*it));
                sep = true;
            }
        }

        if (is_dir && sep) {
            result += "/";
        }

        return result;
    }

    std::string file_path_to_url(fs::path const& x)
    {
        return file_path_to_url_impl(x, false);
    }

    std::string dir_path_to_url(fs::path const& x)
    {
        return file_path_to_url_impl(x, true);
    }

    namespace detail
    {
#if QUICKBOOK_WIDE_PATHS
        std::string command_line_to_utf8(command_line_string const& x)
        {
            return to_utf8(x);
        }
#else
        std::string command_line_to_utf8(command_line_string const& x)
        {
            return x;
        }
#endif

#if QUICKBOOK_WIDE_PATHS
        fs::path generic_to_path(quickbook::string_view x)
        {
            return fs::path(from_utf8(x));
        }

        std::string path_to_generic(fs::path const& x)
        {
            return to_utf8(x.generic_wstring());
        }
#else
        fs::path generic_to_path(quickbook::string_view x)
        {
            return fs::path(x.begin(), x.end());
        }

        std::string path_to_generic(fs::path const& x)
        {
            return x.generic_string();
        }
#endif

#if QUICKBOOK_CYGWIN_PATHS
        fs::path command_line_to_path(command_line_string const& path)
        {
            cygwin_conv_path_t flags = CCP_POSIX_TO_WIN_W | CCP_RELATIVE;

            ssize_t size = cygwin_conv_path(flags, path.c_str(), NULL, 0);

            if (size < 0)
                throw conversion_error(
                    "Error converting cygwin path to windows.");

            boost::scoped_array<char> result(new char[size]);
            void* ptr = result.get();

            if (cygwin_conv_path(flags, path.c_str(), ptr, size))
                throw conversion_error(
                    "Error converting cygwin path to windows.");

            return fs::path(static_cast<wchar_t*>(ptr));
        }

        stream_string path_to_stream(fs::path const& path)
        {
            cygwin_conv_path_t flags = CCP_WIN_W_TO_POSIX | CCP_RELATIVE;

            ssize_t size =
                cygwin_conv_path(flags, path.native().c_str(), NULL, 0);

            if (size < 0)
                throw conversion_error(
                    "Error converting windows path to cygwin.");

            boost::scoped_array<char> result(new char[size]);

            if (cygwin_conv_path(
                    flags, path.native().c_str(), result.get(), size))
                throw conversion_error(
                    "Error converting windows path to cygwin.");

            return std::string(result.get());
        }
#else
        fs::path command_line_to_path(command_line_string const& path)
        {
            return fs::path(path);
        }

#if QUICKBOOK_WIDE_PATHS && !QUICKBOOK_WIDE_STREAMS
        stream_string path_to_stream(fs::path const& path)
        {
            return path.string();
        }
#else
        stream_string path_to_stream(fs::path const& path)
        {
            return path.native();
        }
#endif

#endif // QUICKBOOK_CYGWIN_PATHS

        enum path_or_url_type
        {
            path_or_url_empty = 0,
            path_or_url_path,
            path_or_url_url
        };

        path_or_url::path_or_url() : type_(path_or_url_empty) {}

        path_or_url::path_or_url(path_or_url const& x)
            : type_(x.type_), path_(x.path_), url_(x.url_)
        {
        }

        path_or_url::path_or_url(command_line_string const& x)
        {
            auto rep = command_line_to_utf8(x);
            auto it = rep.begin(), end = rep.end();
            std::size_t count = 0;
            while (it != end &&
                   ((*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z') ||
                    *it == '+' || *it == '-' || *it == '.')) {
                ++it;
                ++count;
            }

            if (it != end && *it == ':' && count > 1) {
                type_ = path_or_url_url;
            }
            else {
                type_ = path_or_url_path;
            }

            switch (type_) {
            case path_or_url_empty:
                break;
            case path_or_url_path:
                path_ = command_line_to_path(x);
                break;
            case path_or_url_url:
                url_ = rep;
                break;
            default:
                assert(false);
            }
        }

        path_or_url& path_or_url::operator=(path_or_url const& x)
        {
            type_ = x.type_;
            path_ = x.path_;
            url_ = x.url_;
            return *this;
        }

        path_or_url& path_or_url::operator=(command_line_string const& x)
        {
            path_or_url tmp(x);
            swap(tmp);
            return *this;
        }

        void path_or_url::swap(path_or_url& x)
        {
            std::swap(type_, x.type_);
            std::swap(path_, x.path_);
            std::swap(url_, x.url_);
        }

        path_or_url path_or_url::url(string_view x)
        {
            path_or_url r;
            r.type_ = path_or_url_url;
            r.url_.assign(x.begin(), x.end());
            return r;
        }

        path_or_url path_or_url::path(boost::filesystem::path const& x)
        {
            path_or_url r;
            r.type_ = path_or_url_path;
            r.path_ = x;
            return r;
        }

        path_or_url::operator bool() const
        {
            return type_ != path_or_url_empty;
        }

        bool path_or_url::is_path() const { return type_ == path_or_url_path; }

        bool path_or_url::is_url() const { return type_ == path_or_url_url; }

        boost::filesystem::path const& path_or_url::get_path() const
        {
            assert(is_path());
            return path_;
        }

        std::string const& path_or_url::get_url() const
        {
            assert(is_url());
            return url_;
        }

        path_or_url path_or_url::operator/(string_view x) const
        {
            path_or_url r;
            r.type_ = type_;

            switch (type_) {
            case path_or_url_empty:
                assert(false);
                break;
            case path_or_url_path:
                r.path_ = path_ / x.to_s();
                break;
            case path_or_url_url: {
                r.url_ = url_;
                auto pos = r.url_.rfind('/');
                if (pos == std::string::npos) {
                    pos = r.url_.rfind(':');
                }
                if (pos != std::string::npos) {
                    r.url_.resize(pos + 1);
                }
                else {
                    // Error? Empty string?
                    r.url_ = "/";
                }
                r.url_ += x;
                break;
            }
            default:
                assert(false);
            }
            return r;
        }
    }
}
