/*=============================================================================
    Copyright (c) 2011, 2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "document_state_impl.hpp"
#include "utils.hpp"
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/algorithm.hpp>
#include <cctype>

namespace quickbook
{
    struct file_info
    {
        boost::shared_ptr<file_info> const parent;
        boost::shared_ptr<doc_info> const document;

        unsigned const compatibility_version;
        unsigned const depth;
        unsigned const override_depth;
        id_placeholder const* const override_id;

        // The 1.1-1.5 document id would actually change per file due to
        // explicit ids in includes and a bug which would sometimes use the
        // document title instead of the id.
        std::string const doc_id_1_1;

        // Constructor for files that aren't the root of a document.
        file_info(boost::shared_ptr<file_info> const& parent,
                unsigned compatibility_version,
                boost::string_ref doc_id_1_1,
                id_placeholder const* override_id) :
            parent(parent), document(parent->document),
            compatibility_version(compatibility_version),
            depth(parent->depth + 1),
            override_depth(override_id ? depth : parent->override_depth),
            override_id(override_id ? override_id : parent->override_id),
            doc_id_1_1(detail::to_s(doc_id_1_1))
        {}

        // Constructor for files that are the root of a document.
        file_info(boost::shared_ptr<file_info> const& parent,
                boost::shared_ptr<doc_info> const& document,
                unsigned compatibility_version,
                boost::string_ref doc_id_1_1) :
            parent(parent), document(document),
            compatibility_version(compatibility_version),
            depth(0), override_depth(0), override_id(0),
            doc_id_1_1(detail::to_s(doc_id_1_1))
        {}
    };

    struct doc_info
    {
        boost::shared_ptr<section_info> current_section;

        // Note: these are mutable to remain bug compatible with old versions
        // of quickbook. They would set these values at the start of new files
        // and sections and then not restore them at the end.
        std::string last_title_1_1;
        std::string section_id_1_1;
    };

    struct section_info
    {
        boost::shared_ptr<section_info> const parent;
        unsigned const compatibility_version;
        unsigned const file_depth;
        unsigned const level;
        std::string const id_1_1;
        id_placeholder const* const placeholder_1_6;
        source_mode_info const source_mode;

        section_info(boost::shared_ptr<section_info> const& parent,
                file_info const* current_file, boost::string_ref id,
                boost::string_ref id_1_1, id_placeholder const* placeholder_1_6,
                source_mode_info const& source_mode) :
            parent(parent),
            compatibility_version(current_file->compatibility_version),
            file_depth(current_file->depth),
            level(parent ? parent->level + 1 : 1),
            id_1_1(detail::to_s(id_1_1)),
            placeholder_1_6(placeholder_1_6),
            source_mode(source_mode) {}
    };

    //
    // document_state
    //

    document_state::document_state()
      : state(new document_state_impl)
    {
    }

    document_state::~document_state() {}

    void document_state::start_file(
            unsigned compatibility_version,
            boost::string_ref include_doc_id,
            boost::string_ref id,
            value const& title)
    {
        state->start_file(compatibility_version, false, include_doc_id, id, title);
    }

    std::string document_state::start_file_with_docinfo(
            unsigned compatibility_version,
            boost::string_ref include_doc_id,
            boost::string_ref id,
            value const& title)
    {
        return state->start_file(compatibility_version, true, include_doc_id,
            id, title)->to_string();
    }

    void document_state::end_file()
    {
        state->end_file();
    }

    std::string document_state::begin_section(boost::string_ref id,
            id_category category, source_mode_info const& source_mode)
    {
        return state->begin_section(id, category, source_mode)->to_string();
    }

    void document_state::end_section()
    {
        return state->end_section();
    }

    int document_state::section_level() const
    {
        return state->current_file->document->current_section->level;
    }

    source_mode_info document_state::section_source_mode() const
    {
        return state->current_file ?
            state->current_file->document->current_section->source_mode :
            source_mode_info();
    }

    std::string document_state::old_style_id(boost::string_ref id, id_category category)
    {
        return state->old_style_id(id, category)->to_string();
    }

    std::string document_state::add_id(boost::string_ref id, id_category category)
    {
        return state->add_id(id, category)->to_string();
    }

    std::string document_state::add_anchor(boost::string_ref id, id_category category)
    {
        return state->add_placeholder(id, category)->to_string();
    }

    std::string document_state::replace_placeholders_with_unresolved_ids(
            boost::string_ref xml) const
    {
        return replace_ids(*state, xml);
    }

    std::string document_state::replace_placeholders(boost::string_ref xml) const
    {
        assert(!state->current_file);
        std::vector<std::string> ids = generate_ids(*state, xml);
        return replace_ids(*state, xml, &ids);
    }

    unsigned document_state::compatibility_version() const
    {
        return state->current_file->compatibility_version;
    }

    //
    // id_placeholder
    //

    id_placeholder::id_placeholder(
            unsigned index,
            boost::string_ref id,
            id_category category,
            id_placeholder const* parent_)
      : index(index),
        unresolved_id(parent_ ?
            parent_->unresolved_id + '.' + detail::to_s(id) :
            detail::to_s(id)),
        id(id.begin(), id.end()),
        parent(parent_),
        category(category),
        num_dots(boost::range::count(id, '.') +
            (parent_ ? parent_->num_dots + 1 : 0))
    {
    }

    std::string id_placeholder::to_string() const
    {
        return '$' + boost::lexical_cast<std::string>(index);
    }

    //
    // document_state_impl
    //

    id_placeholder const* document_state_impl::add_placeholder(
            boost::string_ref id, id_category category,
            id_placeholder const* parent)
    {
        placeholders.push_back(id_placeholder(
            placeholders.size(), id, category, parent));
        return &placeholders.back();
    }

    id_placeholder const* document_state_impl::get_placeholder(boost::string_ref value) const
    {
        // If this isn't a placeholder id.
        if (value.size() <= 1 || *value.begin() != '$')
            return 0;

        unsigned index = boost::lexical_cast<int>(std::string(
                value.begin() + 1, value.end()));

        return &placeholders.at(index);
    }

    id_placeholder const* document_state_impl::get_id_placeholder(
            boost::shared_ptr<section_info> const& section) const
    {
        return !section ? 0 :
            section->file_depth < current_file->override_depth ?
                current_file->override_id : section->placeholder_1_6;
    }

    id_placeholder const* document_state_impl::start_file(
            unsigned compatibility_version,
            bool document_root,
            boost::string_ref include_doc_id,
            boost::string_ref id,
            value const& title)
    {
        boost::shared_ptr<file_info> parent = current_file;
        assert(parent || document_root);

        boost::shared_ptr<doc_info> document =
            document_root ? boost::make_shared<doc_info>() : parent->document;

        // Choose specified id to use. Prefer 'include_doc_id' (the id
        // specified in an 'include' element) unless backwards compatibility
        // is required.

        boost::string_ref initial_doc_id;

        if (document_root ||
            compatibility_version >= 106u ||
            parent->compatibility_version >= 106u)
        {
            initial_doc_id = !include_doc_id.empty() ? include_doc_id : id;
        }
        else {
            initial_doc_id = !id.empty() ? id : include_doc_id;
        }

        // Work out this file's doc_id for older versions of quickbook.
        // A bug meant that this need to be done per file, not per
        // document.

        std::string doc_id_1_1;

        if (document_root || compatibility_version < 106u) {
            if (title.check())
                document->last_title_1_1 = detail::to_s(title.get_quickbook());

            doc_id_1_1 = !initial_doc_id.empty() ? detail::to_s(initial_doc_id) :
                detail::make_identifier(document->last_title_1_1);
        }
        else if (parent) {
            doc_id_1_1 = parent->doc_id_1_1;
        }

        if (document_root) {
            // Create new file

            current_file = boost::make_shared<file_info>(parent,
                    document, compatibility_version, doc_id_1_1);

            // Create a section for the new document.

            source_mode_info default_source_mode;

            if (!initial_doc_id.empty()) {
                return create_new_section(id, id_category::explicit_section_id,
                    default_source_mode);
            }
            else if (!title.empty()) {
                return create_new_section(
                    detail::make_identifier(title.get_quickbook()),
                    id_category::generated_doc,
                    default_source_mode);
            }
            else if (compatibility_version >= 106u) {
                return create_new_section("doc", id_category::numbered, default_source_mode);
            }
            else {
                return create_new_section("", id_category::generated_doc, default_source_mode);
            }
        }
        else {
            // If an id was set for the file, then the file overrides the
            // current section's id with this id.
            //
            // Don't do this for document_root as it will create a section
            // for the document.
            //
            // Don't do this for older versions, as they use a different
            // backwards compatible mechanism to handle file ids.

            id_placeholder const* override_id = 0;

            if (!initial_doc_id.empty() && compatibility_version >= 106u)
            {
                boost::shared_ptr<section_info> null_section;

                override_id = add_id_to_section(initial_doc_id,
                    id_category::explicit_section_id, null_section);
            }

            // Create new file

            current_file =
                boost::make_shared<file_info>(parent, compatibility_version,
                        doc_id_1_1, override_id);

            return 0;
        }
    }

    void document_state_impl::end_file()
    {
        current_file = current_file->parent;
    }

    id_placeholder const* document_state_impl::add_id(
            boost::string_ref id,
            id_category category)
    {
        return add_id_to_section(id, category,
            current_file->document->current_section);
    }

    id_placeholder const* document_state_impl::add_id_to_section(
            boost::string_ref id,
            id_category category,
            boost::shared_ptr<section_info> const& section)
    {
        std::string id_part(id.begin(), id.end());

        // Note: Normalizing id according to file compatibility version, but
        // adding to section according to section compatibility version.

        if (current_file->compatibility_version >= 106u &&
                category.c < id_category::explicit_id) {
            id_part = normalize_id(id);
        }

        id_placeholder const* placeholder_1_6 = get_id_placeholder(section);

        if(!section || section->compatibility_version >= 106u) {
            return add_placeholder(id_part, category, placeholder_1_6);
        }
        else {
            std::string const& qualified_id = section->id_1_1;

            std::string new_id;
            if (!placeholder_1_6)
                new_id = current_file->doc_id_1_1;
            if (!new_id.empty() && !qualified_id.empty()) new_id += '.';
            new_id += qualified_id;
            if (!new_id.empty() && !id_part.empty()) new_id += '.';
            new_id += id_part;

            return add_placeholder(new_id, category, placeholder_1_6);
        }
    }

    id_placeholder const* document_state_impl::old_style_id(
        boost::string_ref id,
        id_category category)
    {
        return current_file->compatibility_version < 103u ?
            add_placeholder(
                current_file->document->section_id_1_1 + "." + detail::to_s(id), category) :
                add_id(id, category);
    }

    id_placeholder const* document_state_impl::begin_section(
            boost::string_ref id,
            id_category category,
            source_mode_info const& source_mode)
    {
        current_file->document->section_id_1_1 = detail::to_s(id);
        return create_new_section(id, category, source_mode);
    }

    id_placeholder const* document_state_impl::create_new_section(
            boost::string_ref id,
            id_category category,
            source_mode_info const& source_mode)
    {
        boost::shared_ptr<section_info> parent =
            current_file->document->current_section;

        id_placeholder const* p = 0;
        id_placeholder const* placeholder_1_6 = 0;

        std::string id_1_1;

        if (parent && current_file->compatibility_version < 106u) {
            id_1_1 = parent->id_1_1;
            if (!id_1_1.empty() && !id.empty())
                id_1_1 += ".";
            id_1_1.append(id.begin(), id.end());
        }

        if (current_file->compatibility_version >= 106u) {
            p = placeholder_1_6 = add_id_to_section(id, category, parent);
        }
        else if (current_file->compatibility_version >= 103u) {
            placeholder_1_6 = get_id_placeholder(parent);

            std::string new_id;
            if (!placeholder_1_6) {
                new_id = current_file->doc_id_1_1;
                if (!id_1_1.empty()) new_id += '.';
            }
            new_id += id_1_1;

            p = add_placeholder(new_id, category, placeholder_1_6);
        }
        else {
            placeholder_1_6 = get_id_placeholder(parent);

            std::string new_id;
            if (parent && !placeholder_1_6)
                new_id = current_file->doc_id_1_1 + '.';

            new_id += detail::to_s(id);

            p = add_placeholder(new_id, category, placeholder_1_6);
        }

        current_file->document->current_section =
            boost::make_shared<section_info>(parent,
                current_file.get(), id, id_1_1, placeholder_1_6,
                source_mode);

        return p;
    }

    void document_state_impl::end_section()
    {
        current_file->document->current_section =
            current_file->document->current_section->parent;
    }
}
