/*=============================================================================
    Copyright (c) 2011-2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_DOCUMENT_STATE_IMPL_HPP)
#define BOOST_QUICKBOOK_DOCUMENT_STATE_IMPL_HPP

#include <deque>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include "document_state.hpp"
#include "phrase_tags.hpp"
#include "string_view.hpp"
#include "utils.hpp"

namespace quickbook
{
    //
    // id_placeholder
    //
    // When generating the xml, quickbook can't allocate the identifiers until
    // the end, so it stores in the intermedia xml a placeholder string,
    // e.g. id="$1". This represents one of these placeholders.
    //

    struct id_placeholder
    {
        std::size_t index; // The index in document_state_impl::placeholders.
                           // Use for the dollar identifiers in
                           // intermediate xml.
        std::string id;    // The node id.
        std::string unresolved_id;
        // The id that would be generated
        // without any duplicate handling.
        // Used for generating old style header anchors.
        id_placeholder const* parent;
        // Placeholder of the parent id.
        id_category category;
        std::ptrdiff_t num_dots; // Number of dots in the id.
                                 // Normally equal to the section level
                                 // but not when an explicit id contains
                                 // dots.

        id_placeholder(
            std::size_t index,
            quickbook::string_view id,
            id_category category,
            id_placeholder const* parent_);

        std::string to_string() const;
    };

    //
    // document_state_impl
    //
    // Contains all the data tracked by document_state.
    //

    struct file_info;
    struct doc_info;
    struct section_info;

    struct document_state_impl
    {
        boost::shared_ptr<file_info> current_file;
        std::deque<id_placeholder> placeholders;

        // Placeholder methods

        id_placeholder const* add_placeholder(
            quickbook::string_view,
            id_category,
            id_placeholder const* parent = 0);

        id_placeholder const* get_placeholder(quickbook::string_view) const;

        id_placeholder const* get_id_placeholder(
            boost::shared_ptr<section_info> const& section) const;

        // Events

        id_placeholder const* start_file(
            unsigned compatibility_version,
            bool document_root,
            quickbook::string_view include_doc_id,
            quickbook::string_view id,
            value const& title);

        void end_file();

        id_placeholder const* add_id(
            quickbook::string_view id, id_category category);
        id_placeholder const* old_style_id(
            quickbook::string_view id, id_category category);
        id_placeholder const* begin_section(
            value const& explicit_id,
            quickbook::string_view id,
            id_category category,
            source_mode_info const&);
        void end_section();

      private:
        id_placeholder const* add_id_to_section(
            quickbook::string_view id,
            id_category category,
            boost::shared_ptr<section_info> const& section);
        id_placeholder const* create_new_section(
            value const& explicit_id,
            quickbook::string_view id,
            id_category category,
            source_mode_info const&);
    };

    std::string replace_ids(
        document_state_impl const& state,
        quickbook::string_view xml,
        std::vector<std::string> const* = 0);
    std::vector<std::string> generate_ids(
        document_state_impl const&, quickbook::string_view);

    std::string normalize_id(quickbook::string_view src_id);
    std::string normalize_id(quickbook::string_view src_id, std::size_t);

    //
    // Xml subset parser used for finding id values.
    //
    // I originally tried to integrate this into the post processor
    // but that proved tricky. Alternatively it could use a proper
    // xml parser, but I want this to be able to survive badly
    // marked up escapes.
    //

    struct xml_processor
    {
        xml_processor();

        std::vector<std::string> id_attributes;

        struct callback
        {
            virtual void start(quickbook::string_view) {}
            virtual void id_value(quickbook::string_view) {}
            virtual void finish(quickbook::string_view) {}
            virtual ~callback() {}
        };

        void parse(quickbook::string_view, callback&);
    };
}

#endif
