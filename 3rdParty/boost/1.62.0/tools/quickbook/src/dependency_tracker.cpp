/*=============================================================================
    Copyright (c) 2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "dependency_tracker.hpp"
#include "native_text.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>

namespace quickbook
{
    // Convert the path to its canonical representation if it exists.
    // Or something close if it doesn't.
    static fs::path normalize_path(fs::path const& path)
    {
        fs::path p = fs::absolute(path); // The base of the path.
        fs::path extra; // The non-existant part of the path.
        int parent_count = 0; // Number of active '..' sections

        // Invariant: path is equivalent to: p / ('..' * parent_count) / extra
        // i.e. if parent_count == 0: p/extra
        // if parent_count == 2: p/../../extra

        // Pop path sections from path until we find an existing
        // path, adjusting for any dot path sections.
        while (!fs::exists(fs::status(p))) {
            fs::path name = p.filename();
            p = p.parent_path();
            if (name == "..") {
                ++parent_count;
            }
            else if (name == ".") {
            }
            else if (parent_count) {
                --parent_count;
            }
            else {
                extra = name / extra;
            }
        }

        // If there are any left over ".." sections, then add them
        // on to the end of the real path, and trust Boost.Filesystem
        // to sort them out.
        while (parent_count) {
            p = p / "..";
            --parent_count;
        }

        // Cannoicalize the existing part of the path, and add 'extra' back to
        // the end.
        return fs::canonical(p) / extra;
    }

    static char const* control_escapes[16] = {
        "\\000", "\\001", "\\002", "\\003",
        "\\004", "\\005", "\\006", "\\a",
        "\\b",   "\\t",   "\\n",   "\\v",
        "\\f",   "\\r",   "\\016", "\\017"
    };

    static std::string escaped_path(std::string const& generic)
    {
        std::string result;
        result.reserve(generic.size());

        BOOST_FOREACH(char c, generic)
        {
            if (c >= 0 && c < 16) {
                result += control_escapes[(unsigned int) c];
            }
            else if (c == '\\') {
                result += "\\\\";
            }
            else if (c == 127) {
                result += "\\177";
            }
            else {
                result += c;
            }
        }

        return result;
    }

    static std::string get_path(fs::path const& path,
            dependency_tracker::flags f)
    {
        std::string generic = quickbook::detail::path_to_generic(path);

        if (f & dependency_tracker::escaped) {
            generic = escaped_path(generic);
        }

        return generic;
    }

    dependency_tracker::dependency_tracker() :
        dependencies(), glob_dependencies(),
        last_glob(glob_dependencies.end()) {}

    bool dependency_tracker::add_dependency(fs::path const& f) {
        bool found = fs::exists(fs::status(f));
        dependencies[normalize_path(f)] |= found;
        return found;
    }

    void dependency_tracker::add_glob(fs::path const& f) {
        std::pair<glob_list::iterator, bool> r = glob_dependencies.insert(
                std::make_pair(normalize_path(f), glob_list::mapped_type()));
        last_glob = r.first;
    }

    void dependency_tracker::add_glob_match(fs::path const& f) {
        assert(last_glob != glob_dependencies.end());
        last_glob->second.insert(normalize_path(f));
    }

    void dependency_tracker::write_dependencies(fs::path const& file_out,
            flags f)
    {
        fs::ofstream out(file_out);

        if (out.fail()) {
            throw std::runtime_error(
                "Error opening dependency file " +
                quickbook::detail::path_to_generic(file_out));
        }

        out.exceptions(std::ios::badbit);
        write_dependencies(out, f);
    }

    void dependency_tracker::write_dependencies(std::ostream& out,
            flags f)
    {
        if (f & checked) {
            BOOST_FOREACH(dependency_list::value_type const& d, dependencies)
            {
                out << (d.second ? "+ " : "- ")
                    << get_path(d.first, f) << std::endl;
            }

            BOOST_FOREACH(glob_list::value_type const& g, glob_dependencies)
            {
                out << "g "
                    << get_path(g.first, f) << std::endl;

                BOOST_FOREACH(fs::path const& p, g.second)
                {
                    out << "+ " << get_path(p, f) << std::endl;
                }
            }
        }
        else {
            std::set<std::string> paths;

            BOOST_FOREACH(dependency_list::value_type const& d, dependencies)
            {
                if (d.second) {
                    paths.insert(get_path(d.first, f));
                }
            }

            BOOST_FOREACH(glob_list::value_type const& g, glob_dependencies)
            {
                BOOST_FOREACH(fs::path const& p, g.second)
                {
                    paths.insert(get_path(p, f));
                }
            }

            BOOST_FOREACH(std::string const& p, paths)
            {
                out << p << std::endl;
            }
        }
    }
}
