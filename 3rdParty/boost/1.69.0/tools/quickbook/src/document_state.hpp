/*=============================================================================
    Copyright (c) 2011,2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_DOCUMENT_STATE_HPP)
#define BOOST_QUICKBOOK_DOCUMENT_STATE_HPP

#include <string>
#include <boost/scoped_ptr.hpp>
#include "string_view.hpp"
#include "syntax_highlight.hpp"
#include "values.hpp"

namespace quickbook
{
    // id_category
    //
    // Higher categories get priority over lower ones.

    struct id_category
    {
        enum categories
        {
            default_category = 0,
            numbered,          // Just used to avoid random docbook ids
            generated,         // Generated ids for other elements.
            generated_heading, // Generated ids for headings.
            generated_section, // Generated ids for sections.
            generated_doc,     // Generated ids for document.
            explicit_id,       // Explicitly given by user
            explicit_section_id,
            explicit_anchor_id
        };

        id_category() : c(default_category) {}
        id_category(categories c_) : c(c_) {}
        explicit id_category(int c_) : c(categories(c_)) {}

        bool operator==(id_category rhs) const { return c == rhs.c; }

        categories c;
    };

    struct document_state_impl;

    struct document_state
    {
        document_state();
        ~document_state();

        std::string start_file_with_docinfo(
            unsigned compatibility_version,
            quickbook::string_view include_doc_id,
            quickbook::string_view id,
            value const& title);

        void start_file(
            unsigned compatibility_version,
            quickbook::string_view include_doc_id,
            quickbook::string_view id,
            value const& title);

        void end_file();

        std::string begin_section(
            value const&,
            quickbook::string_view,
            id_category,
            source_mode_info const&);
        void end_section();
        int section_level() const;
        value const& explicit_id() const;
        source_mode_info section_source_mode() const;

        std::string old_style_id(quickbook::string_view, id_category);
        std::string add_id(quickbook::string_view, id_category);
        std::string add_anchor(quickbook::string_view, id_category);

        std::string replace_placeholders_with_unresolved_ids(
            quickbook::string_view) const;
        std::string replace_placeholders(quickbook::string_view) const;

        unsigned compatibility_version() const;

      private:
        boost::scoped_ptr<document_state_impl> state;
    };
}

#endif
