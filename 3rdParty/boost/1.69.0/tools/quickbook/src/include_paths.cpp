/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2005 Thomas Guest
    Copyright (c) 2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "include_paths.hpp"
#include <cassert>
#include <boost/filesystem/operations.hpp>
#include <boost/range/algorithm/replace.hpp>
#include "for.hpp"
#include "glob.hpp"
#include "path.hpp"
#include "quickbook.hpp" // For the include_path global (yuck)
#include "state.hpp"
#include "stream.hpp"
#include "utils.hpp"

namespace quickbook
{
    //
    // check_path
    //

    path_parameter check_path(value const& path, quickbook::state& state)
    {
        if (qbk_version_n >= 107u) {
            std::string path_text = path.get_encoded();
            if (path_text.empty()) {
                detail::outerr(path.get_file(), path.get_position())
                    << "Empty path argument"
                    << "std::endl";
                ++state.error_count;
                return path_parameter(path_text, path_parameter::invalid);
            }

            try {
                if (check_glob(path_text)) {
                    return path_parameter(path_text, path_parameter::glob);
                }
                else {
                    return path_parameter(
                        glob_unescape(path_text), path_parameter::path);
                }
            } catch (glob_error& e) {
                detail::outerr(path.get_file(), path.get_position())
                    << "Invalid path (" << e.what() << "): " << path_text
                    << std::endl;
                ++state.error_count;
                return path_parameter(path_text, path_parameter::invalid);
            }
        }
        else {
            // Paths are encoded for quickbook 1.6+ and also xmlbase
            // values (technically xmlbase is a 1.6 feature, but that
            // isn't enforced as it's backwards compatible).
            //
            // Counter-intuitively: encoded == plain text here.

            std::string path_text = qbk_version_n >= 106u || path.is_encoded()
                                        ? path.get_encoded()
                                        : path.get_quickbook().to_s();

            if (path_text.empty()) {
                detail::outerr(path.get_file(), path.get_position())
                    << "Empty path argument" << std::endl;
                ++state.error_count;
                return path_parameter(path_text, path_parameter::invalid);
            }

            // Check for windows paths, an error in quickbook 1.6
            // In quickbook 1.7 backslash is used as an escape character
            // for glob characters.
            if (path_text.find('\\') != std::string::npos) {
                quickbook::detail::ostream* err;

                if (qbk_version_n >= 106u) {
                    err = &detail::outerr(path.get_file(), path.get_position());
                    ++state.error_count;
                }
                else {
                    err =
                        &detail::outwarn(path.get_file(), path.get_position());
                }

                *err << "Path isn't portable: '" << path_text << "'"
                     << std::endl;

                boost::replace(path_text, '\\', '/');
            }

            return path_parameter(path_text, path_parameter::path);
        }
    }

    path_parameter check_xinclude_path(value const& p, quickbook::state& state)
    {
        path_parameter parameter = check_path(p, state);

        if (parameter.type == path_parameter::glob) {
            detail::outerr(p.get_file(), p.get_position())
                << "Glob used for xml path." << std::endl;
            ++state.error_count;
            parameter.type = path_parameter::invalid;
        }

        return parameter;
    }

    //
    // Search include path
    //

    void include_search_glob(
        std::set<quickbook_path>& result,
        quickbook_path const& location,
        std::string path,
        quickbook::state& state)
    {
        std::size_t glob_pos = find_glob_char(path);

        if (glob_pos == std::string::npos) {
            quickbook_path complete_path = location / glob_unescape(path);

            if (fs::exists(complete_path.file_path)) {
                state.dependencies.add_glob_match(complete_path.file_path);
                result.insert(complete_path);
            }
            return;
        }

        std::size_t prev = path.rfind('/', glob_pos);
        std::size_t next = path.find('/', glob_pos);

        std::size_t glob_begin = prev == std::string::npos ? 0 : prev + 1;
        std::size_t glob_end = next == std::string::npos ? path.size() : next;

        quickbook_path new_location = location;

        if (prev != std::string::npos) {
            new_location /= glob_unescape(path.substr(0, prev));
        }

        if (next != std::string::npos) ++next;

        quickbook::string_view glob(
            path.data() + glob_begin, glob_end - glob_begin);

        fs::path base_dir = new_location.file_path.empty()
                                ? fs::path(".")
                                : new_location.file_path;
        if (!fs::is_directory(base_dir)) return;

        // Walk through the dir for matches.
        for (fs::directory_iterator dir_i(base_dir), dir_e; dir_i != dir_e;
             ++dir_i) {
            fs::path f = dir_i->path().filename();
            std::string generic_path = detail::path_to_generic(f);

            // Skip if the dir item doesn't match.
            if (!quickbook::glob(glob, generic_path)) continue;

            // If it's a file we add it to the results.
            if (next == std::string::npos) {
                if (fs::is_regular_file(dir_i->status())) {
                    quickbook_path r = new_location / generic_path;
                    state.dependencies.add_glob_match(r.file_path);
                    result.insert(r);
                }
            }
            // If it's a matching dir, we recurse looking for more files.
            else {
                if (!fs::is_regular_file(dir_i->status())) {
                    include_search_glob(
                        result, new_location / generic_path, path.substr(next),
                        state);
                }
            }
        }
    }

    std::set<quickbook_path> include_search(
        path_parameter const& parameter,
        quickbook::state& state,
        string_iterator pos)
    {
        std::set<quickbook_path> result;

        switch (parameter.type) {
        case path_parameter::glob:
            // If the path has some glob match characters
            // we do a discovery of all the matches..
            {
                fs::path current = state.current_file->path.parent_path();

                // Search for the current dir accumulating to the result.
                state.dependencies.add_glob(current / parameter.value);
                include_search_glob(
                    result, state.current_path.parent_path(), parameter.value,
                    state);

                // Search the include path dirs accumulating to the result.
                unsigned count = 0;
                QUICKBOOK_FOR (fs::path dir, include_path) {
                    ++count;
                    state.dependencies.add_glob(dir / parameter.value);
                    include_search_glob(
                        result, quickbook_path(dir, count, fs::path()),
                        parameter.value, state);
                }

                // Done.
                return result;
            }

        case path_parameter::path: {
            fs::path path = detail::generic_to_path(parameter.value);

            // If the path is relative, try and resolve it.
            if (!path.has_root_directory() && !path.has_root_name()) {
                quickbook_path path2 =
                    state.current_path.parent_path() / parameter.value;

                // See if it can be found locally first.
                if (state.dependencies.add_dependency(path2.file_path)) {
                    result.insert(path2);
                    return result;
                }

                // Search in each of the include path locations.
                unsigned count = 0;
                QUICKBOOK_FOR (fs::path full, include_path) {
                    ++count;
                    full /= path;

                    if (state.dependencies.add_dependency(full)) {
                        result.insert(quickbook_path(full, count, path));
                        return result;
                    }
                }
            }
            else {
                if (state.dependencies.add_dependency(path)) {
                    result.insert(quickbook_path(path, 0, path));
                    return result;
                }
            }

            detail::outerr(state.current_file, pos)
                << "Unable to find file: " << parameter.value << std::endl;
            ++state.error_count;

            return result;
        }

        case path_parameter::invalid:
            return result;

        default:
            assert(0);
            return result;
        }
    }

    //
    // quickbook_path
    //

    void swap(quickbook_path& x, quickbook_path& y)
    {
        boost::swap(x.file_path, y.file_path);
        boost::swap(x.include_path_offset, y.include_path_offset);
        boost::swap(x.abstract_file_path, y.abstract_file_path);
    }

    bool quickbook_path::operator<(quickbook_path const& other) const
    {
        return abstract_file_path != other.abstract_file_path
                   ? abstract_file_path < other.abstract_file_path
                   : include_path_offset != other.include_path_offset
                         ? include_path_offset < other.include_path_offset
                         : file_path < other.file_path;
    }

    quickbook_path quickbook_path::operator/(quickbook::string_view x) const
    {
        return quickbook_path(*this) /= x;
    }

    quickbook_path& quickbook_path::operator/=(quickbook::string_view x)
    {
        fs::path x2 = detail::generic_to_path(x);
        file_path /= x2;
        abstract_file_path /= x2;
        return *this;
    }

    quickbook_path quickbook_path::parent_path() const
    {
        return quickbook_path(
            file_path.parent_path(), include_path_offset,
            abstract_file_path.parent_path());
    }

    quickbook_path resolve_xinclude_path(
        std::string const& x, quickbook::state& state, bool is_file)
    {
        fs::path path = detail::generic_to_path(x);
        fs::path full_path = path;

        // If the path is relative
        if (!path.has_root_directory()) {
            // Resolve the path from the current file
            full_path = state.current_file->path.parent_path() / path;

            // Then calculate relative to the current xinclude_base.
            path = path_difference(state.xinclude_base, full_path, is_file);
        }

        return quickbook_path(full_path, 0, path);
    }
}
