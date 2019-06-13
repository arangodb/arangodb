/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2011 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_FILES_HPP)
#define BOOST_QUICKBOOK_FILES_HPP

#include <cassert>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <boost/filesystem/path.hpp>
#include <boost/intrusive_ptr.hpp>
#include "string_view.hpp"

namespace quickbook
{

    namespace fs = boost::filesystem;

    struct file;
    typedef boost::intrusive_ptr<file> file_ptr;

    struct file_position
    {
        file_position() : line(1), column(1) {}
        file_position(std::ptrdiff_t l, std::ptrdiff_t c) : line(l), column(c)
        {
        }

        std::ptrdiff_t line;
        std::ptrdiff_t column;

        bool operator==(file_position const& other) const
        {
            return line == other.line && column == other.column;
        }

        friend std::ostream& operator<<(std::ostream&, file_position const&);
    };

    file_position relative_position(
        string_iterator begin, string_iterator iterator);

    struct file
    {
      private:
        // Non copyable
        file& operator=(file const&);
        file(file const&);

      public:
        fs::path const path;
        std::string source_;
        bool is_code_snippets;

      private:
        unsigned qbk_version;
        unsigned ref_count;

      public:
        quickbook::string_view source() const { return source_; }

        file(
            fs::path const& path_,
            quickbook::string_view source_view,
            unsigned qbk_version_)
            : path(path_)
            , source_(source_view.begin(), source_view.end())
            , is_code_snippets(false)
            , qbk_version(qbk_version_)
            , ref_count(0)
        {
        }

        explicit file(file const& f, quickbook::string_view s)
            : path(f.path)
            , source_(s.begin(), s.end())
            , is_code_snippets(f.is_code_snippets)
            , qbk_version(f.qbk_version)
            , ref_count(0)
        {
        }

        virtual ~file() { assert(!ref_count); }

        unsigned version() const
        {
            assert(qbk_version);
            return qbk_version;
        }

        void version(unsigned v)
        {
            // Check that either version hasn't been set, or it was
            // previously set to the same version (because the same
            // file has been loaded twice).
            assert(!qbk_version || qbk_version == v);
            qbk_version = v;
        }

        virtual file_position position_of(string_iterator) const;

        friend void intrusive_ptr_add_ref(file* ptr) { ++ptr->ref_count; }

        friend void intrusive_ptr_release(file* ptr)
        {
            if (--ptr->ref_count == 0) delete ptr;
        }
    };

    // If version isn't supplied then it must be set later.
    file_ptr load(fs::path const& filename, unsigned qbk_version = 0);

    struct load_error : std::runtime_error
    {
        explicit load_error(std::string const& arg) : std::runtime_error(arg) {}
    };

    // Interface for creating fake files which are mapped to
    // real files, so that the position can be found later.

    struct mapped_file_builder_data;

    struct mapped_file_builder
    {
        typedef string_iterator iterator;
        typedef quickbook::string_view::size_type pos_type;

        mapped_file_builder();
        ~mapped_file_builder();

        void start(file_ptr);
        file_ptr release();
        void clear();

        bool empty() const;
        pos_type get_pos() const;

        void add_at_pos(quickbook::string_view, iterator);
        void add(quickbook::string_view);
        void add(mapped_file_builder const&);
        void add(mapped_file_builder const&, pos_type, pos_type);
        void unindent_and_add(quickbook::string_view);

      private:
        mapped_file_builder_data* data;

        mapped_file_builder(mapped_file_builder const&);
        mapped_file_builder& operator=(mapped_file_builder const&);
    };
}

#endif // BOOST_QUICKBOOK_FILES_HPP
