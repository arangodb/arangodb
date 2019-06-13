/*=============================================================================
    Copyright (c) 2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "dependency_tracker.hpp"
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include "for.hpp"
#include "path.hpp"

namespace quickbook
{
    static char const* control_escapes[16] = {
        "\\000", "\\001", "\\002", "\\003", "\\004", "\\005", "\\006", "\\a",
        "\\b",   "\\t",   "\\n",   "\\v",   "\\f",   "\\r",   "\\016", "\\017"};

    static std::string escaped_path(std::string const& generic)
    {
        std::string result;
        result.reserve(generic.size());

        QUICKBOOK_FOR (char c, generic) {
            if (c >= 0 && c < 16) {
                result += control_escapes[(unsigned int)c];
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

    static std::string get_path(
        fs::path const& path, dependency_tracker::flags f)
    {
        std::string generic = quickbook::detail::path_to_generic(path);

        if (f & dependency_tracker::escaped) {
            generic = escaped_path(generic);
        }

        return generic;
    }

    dependency_tracker::dependency_tracker()
        : dependencies()
        , glob_dependencies()
        , last_glob(glob_dependencies.end())
    {
    }

    bool dependency_tracker::add_dependency(fs::path const& f)
    {
        bool found = fs::exists(fs::status(f));
        dependencies[f] |= found;
        return found;
    }

    void dependency_tracker::add_glob(fs::path const& f)
    {
        std::pair<glob_list::iterator, bool> r = glob_dependencies.insert(
            std::make_pair(f, glob_list::mapped_type()));
        last_glob = r.first;
    }

    void dependency_tracker::add_glob_match(fs::path const& f)
    {
        assert(last_glob != glob_dependencies.end());
        last_glob->second.insert(f);
    }

    void dependency_tracker::write_dependencies(
        fs::path const& file_out, flags f)
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

    void dependency_tracker::write_dependencies(std::ostream& out, flags f)
    {
        if (f & checked) {
            QUICKBOOK_FOR (dependency_list::value_type const& d, dependencies) {
                out << (d.second ? "+ " : "- ") << get_path(d.first, f)
                    << std::endl;
            }

            QUICKBOOK_FOR (glob_list::value_type const& g, glob_dependencies) {
                out << "g " << get_path(g.first, f) << std::endl;

                QUICKBOOK_FOR (fs::path const& p, g.second) {
                    out << "+ " << get_path(p, f) << std::endl;
                }
            }
        }
        else {
            std::set<std::string> paths;

            QUICKBOOK_FOR (dependency_list::value_type const& d, dependencies) {
                if (d.second) {
                    paths.insert(get_path(d.first, f));
                }
            }

            QUICKBOOK_FOR (glob_list::value_type const& g, glob_dependencies) {
                QUICKBOOK_FOR (fs::path const& p, g.second) {
                    paths.insert(get_path(p, f));
                }
            }

            QUICKBOOK_FOR (std::string const& p, paths) {
                out << p << std::endl;
            }
        }
    }
}
