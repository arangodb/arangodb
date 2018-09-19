/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2005 Thomas Guest
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <sstream>
#include <boost/algorithm/string/join.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include "doc_info_tags.hpp"
#include "document_state.hpp"
#include "files.hpp"
#include "for.hpp"
#include "path.hpp"
#include "quickbook.hpp"
#include "state.hpp"
#include "stream.hpp"
#include "utils.hpp"

namespace quickbook
{
    struct doc_info_values
    {
        std::string doc_type;
        value doc_title;
        std::vector<value> escaped_attributes;
        value qbk_version, compatibility_mode;
        value id, dirname, last_revision, purpose;
        std::vector<value> categories;
        value lang, version;
        std::vector<value> authors;
        std::vector<value> copyrights;
        value license;
        std::vector<value> biblioids;
        value xmlbase;
        std::string xmlbase_value;

        std::string id_placeholder;
        std::string include_doc_id_, id_;
    };

    static void write_document_title(
        collector& out, value const& title, value const& version);
    std::string write_boostbook_header(
        quickbook::state& state, doc_info_values const& info, bool nested_file);

    static std::string doc_info_output(value const& p, unsigned version)
    {
        if (qbk_version_n < version) {
            std::string value = p.get_quickbook().to_s();
            value.erase(value.find_last_not_of(" \t") + 1);
            return value;
        }
        else {
            return p.get_encoded();
        }
    }

    char const* doc_info_attribute_name(value::tag_type tag)
    {
        return doc_attributes::is_tag(tag) ? doc_attributes::name(tag)
                                           : doc_info_attributes::name(tag);
    }

    // Each docinfo attribute is stored in a value list, these are then stored
    // in a sorted value list. The following convenience methods extract all the
    // values for an attribute tag.

    // Expecting at most one attribute, with several values in the list.
    value consume_list(
        value_consumer& c,
        value::tag_type tag,
        std::vector<std::string>* duplicates)
    {
        value p;

        int count = 0;
        while (c.check(tag)) {
            p = c.consume();
            ++count;
        }

        if (count > 1) duplicates->push_back(doc_info_attribute_name(tag));

        return p;
    }

    // Expecting at most one attribute, with a single value, so extract that
    // immediately.
    value consume_value_in_list(
        value_consumer& c,
        value::tag_type tag,
        std::vector<std::string>* duplicates)
    {
        value l = consume_list(c, tag, duplicates);
        if (l.empty()) return l;

        assert(l.is_list());
        value_consumer c2 = l;
        value p = c2.consume();
        c2.finish();

        return p;
    }

    // Any number of attributes, so stuff them into a vector.
    std::vector<value> consume_multiple_values(
        value_consumer& c, value::tag_type tag)
    {
        std::vector<value> values;

        while (c.check(tag)) {
            values.push_back(c.consume());
        }

        return values;
    }

    enum version_state
    {
        version_unknown,
        version_stable,
        version_dev
    };
    version_state classify_version(unsigned v)
    {
        return v < 100u ? version_unknown
                        : v <= 107u ? version_stable :
                                    // v <= 107u ? version_dev :
                              version_unknown;
    }

    unsigned get_version(
        quickbook::state& state, bool using_docinfo, value version)
    {
        unsigned result = 0;

        if (!version.empty()) {
            value_consumer version_values(version);
            bool before_docinfo =
                version_values.optional_consume(doc_info_tags::before_docinfo)
                    .check();
            int major_verison = version_values.consume().get_int();
            int minor_verison = version_values.consume().get_int();
            version_values.finish();

            if (before_docinfo || using_docinfo) {
                result =
                    ((unsigned)major_verison * 100) + (unsigned)minor_verison;

                if (classify_version(result) == version_unknown) {
                    detail::outerr(state.current_file->path)
                        << "Unknown version: " << major_verison << "."
                        << minor_verison << std::endl;
                    ++state.error_count;
                }
            }
        }

        return result;
    }

    std::string pre(
        quickbook::state& state,
        parse_iterator pos,
        value include_doc_id,
        bool nested_file)
    {
        // The doc_info in the file has been parsed. Here's what we'll do
        // *before* anything else.
        //
        // If there isn't a doc info block, then values will be empty, so most
        // of the following code won't actually do anything.

        value_consumer values = state.values.release();

        // Skip over invalid attributes

        while (values.check(value::default_tag))
            values.consume();

        bool use_doc_info = false;
        doc_info_values info;

        if (values.check(doc_info_tags::type)) {
            info.doc_type =
                values.consume(doc_info_tags::type).get_quickbook().to_s();
            info.doc_title = values.consume(doc_info_tags::title);
            use_doc_info = !nested_file || qbk_version_n >= 106u;
        }
        else {
            if (!nested_file) {
                detail::outerr(state.current_file, pos.base())
                    << "No doc_info block." << std::endl;

                ++state.error_count;

                // Create a fake document info block in order to continue.
                info.doc_type = "article";
                info.doc_title = qbk_value(
                    state.current_file, pos.base(), pos.base(),
                    doc_info_tags::type);
                use_doc_info = true;
            }
        }

        std::vector<std::string> duplicates;

        info.escaped_attributes =
            consume_multiple_values(values, doc_info_tags::escaped_attribute);

        info.qbk_version =
            consume_list(values, doc_attributes::qbk_version, &duplicates);
        info.compatibility_mode = consume_list(
            values, doc_attributes::compatibility_mode, &duplicates);
        consume_multiple_values(values, doc_attributes::source_mode);

        info.id =
            consume_value_in_list(values, doc_info_attributes::id, &duplicates);
        info.dirname = consume_value_in_list(
            values, doc_info_attributes::dirname, &duplicates);
        info.last_revision = consume_value_in_list(
            values, doc_info_attributes::last_revision, &duplicates);
        info.purpose = consume_value_in_list(
            values, doc_info_attributes::purpose, &duplicates);
        info.categories =
            consume_multiple_values(values, doc_info_attributes::category);
        info.lang = consume_value_in_list(
            values, doc_info_attributes::lang, &duplicates);
        info.version = consume_value_in_list(
            values, doc_info_attributes::version, &duplicates);
        info.authors =
            consume_multiple_values(values, doc_info_attributes::authors);
        info.copyrights =
            consume_multiple_values(values, doc_info_attributes::copyright);
        info.license = consume_value_in_list(
            values, doc_info_attributes::license, &duplicates);
        info.biblioids =
            consume_multiple_values(values, doc_info_attributes::biblioid);
        info.xmlbase = consume_value_in_list(
            values, doc_info_attributes::xmlbase, &duplicates);

        values.finish();

        if (!duplicates.empty()) {
            detail::outwarn(state.current_file->path)
                << (duplicates.size() > 1 ? "Duplicate attributes"
                                          : "Duplicate attribute")
                << ":" << boost::algorithm::join(duplicates, ", ") << "\n";
        }

        if (!include_doc_id.empty())
            info.include_doc_id_ = include_doc_id.get_quickbook().to_s();
        if (!info.id.empty()) info.id_ = info.id.get_quickbook().to_s();

        // Quickbook version

        unsigned new_version =
            get_version(state, use_doc_info, info.qbk_version);

        if (new_version != qbk_version_n) {
            if (classify_version(new_version) == version_dev) {
                detail::outwarn(state.current_file->path)
                    << "Quickbook " << (new_version / 100) << "."
                    << (new_version % 100)
                    << " is still under development and is "
                       "likely to change in the future."
                    << std::endl;
            }
        }

        if (new_version) {
            qbk_version_n = new_version;
        }
        else if (use_doc_info) {
            // hard code quickbook version to v1.1
            qbk_version_n = 101;
            detail::outwarn(state.current_file, pos.base())
                << "Quickbook version undefined. "
                   "Version 1.1 is assumed"
                << std::endl;
        }

        state.current_file->version(qbk_version_n);

        // Compatibility Version

        unsigned compatibility_version =
            get_version(state, use_doc_info, info.compatibility_mode);

        if (!compatibility_version) {
            compatibility_version =
                use_doc_info ? qbk_version_n
                             : state.document.compatibility_version();
        }

        // Start file, finish here if not generating document info.

        if (!use_doc_info) {
            state.document.start_file(
                compatibility_version, info.include_doc_id_, info.id_,
                info.doc_title);
            return "";
        }

        info.id_placeholder = state.document.start_file_with_docinfo(
            compatibility_version, info.include_doc_id_, info.id_,
            info.doc_title);

        // Make sure we really did have a document info block.

        assert(info.doc_title.check() && !info.doc_type.empty());

        // Set xmlbase

        // std::string xmlbase_value;

        if (!info.xmlbase.empty()) {
            path_parameter x = check_xinclude_path(info.xmlbase, state);

            if (x.type == path_parameter::path) {
                quickbook_path path = resolve_xinclude_path(x.value, state);

                if (!fs::is_directory(path.file_path)) {
                    detail::outerr(
                        info.xmlbase.get_file(), info.xmlbase.get_position())
                        << "xmlbase \"" << info.xmlbase.get_quickbook()
                        << "\" isn't a directory." << std::endl;

                    ++state.error_count;
                }
                else {
                    info.xmlbase_value =
                        dir_path_to_url(path.abstract_file_path);
                    state.xinclude_base = path.file_path;
                }
            }
        }

        // Warn about invalid fields

        if (info.doc_type != "library") {
            std::vector<std::string> invalid_attributes;

            if (!info.purpose.empty()) invalid_attributes.push_back("purpose");

            if (!info.categories.empty())
                invalid_attributes.push_back("category");

            if (!info.dirname.empty()) invalid_attributes.push_back("dirname");

            if (!invalid_attributes.empty()) {
                detail::outwarn(state.current_file->path)
                    << (invalid_attributes.size() > 1 ? "Invalid attributes"
                                                      : "Invalid attribute")
                    << " for '" << info.doc_type << " document info': "
                    << boost::algorithm::join(invalid_attributes, ", ") << "\n";
            }
        }

        return write_boostbook_header(state, info, nested_file);
    }

    std::string write_boostbook_header(
        quickbook::state& state, doc_info_values const& info, bool nested_file)
    {
        // Write out header

        if (!nested_file) {
            state.out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      << "<!DOCTYPE " << info.doc_type
                      << " PUBLIC \"-//Boost//DTD BoostBook XML V1.0//EN\"\n"
                      << "     "
                         "\"http://www.boost.org/tools/boostbook/dtd/"
                         "boostbook.dtd\">\n";
        }

        state.out << '<' << info.doc_type << "\n"
                  << "    id=\"" << info.id_placeholder << "\"\n";

        if (!info.lang.empty()) {
            state.out << "    lang=\"" << doc_info_output(info.lang, 106)
                      << "\"\n";
        }

        if (info.doc_type == "library" && !info.doc_title.empty()) {
            state.out << "    name=\"" << doc_info_output(info.doc_title, 106)
                      << "\"\n";
        }

        // Set defaults for dirname + last_revision

        if (!info.dirname.empty() || info.doc_type == "library") {
            state.out << "    dirname=\"";
            if (!info.dirname.empty()) {
                state.out << doc_info_output(info.dirname, 106);
            }
            else if (!info.id_.empty()) {
                state.out << info.id_;
            }
            else if (!info.include_doc_id_.empty()) {
                state.out << info.include_doc_id_;
            }
            else if (!info.doc_title.empty()) {
                state.out << detail::make_identifier(
                    info.doc_title.get_quickbook());
            }
            else {
                state.out << "library";
            }

            state.out << "\"\n";
        }

        state.out << "    last-revision=\"";
        if (!info.last_revision.empty()) {
            state.out << doc_info_output(info.last_revision, 106);
        }
        else {
            // default value for last-revision is now

            char strdate[64];
            strftime(
                strdate, sizeof(strdate),
                (debug_mode ? "DEBUG MODE Date: %Y/%m/%d %H:%M:%S $"
                            : "$" /* prevent CVS substitution */
                              "Date: %Y/%m/%d %H:%M:%S $"),
                current_gm_time);

            state.out << strdate;
        }

        state.out << "\" \n";

        if (!info.xmlbase_value.empty()) {
            state.out << "    xml:base=\"" << info.xmlbase_value << "\"\n";
        }

        state.out << "    xmlns:xi=\"http://www.w3.org/2001/XInclude\">\n";

        std::ostringstream tmp;

        if (!info.authors.empty()) {
            tmp << "    <authorgroup>\n";
            QUICKBOOK_FOR (value_consumer author_values, info.authors) {
                while (author_values.check()) {
                    value surname =
                        author_values.consume(doc_info_tags::author_surname);
                    value first =
                        author_values.consume(doc_info_tags::author_first);

                    tmp << "      <author>\n"
                        << "        <firstname>" << doc_info_output(first, 106)
                        << "</firstname>\n"
                        << "        <surname>" << doc_info_output(surname, 106)
                        << "</surname>\n"
                        << "      </author>\n";
                }
            }
            tmp << "    </authorgroup>\n";
        }

        QUICKBOOK_FOR (value_consumer copyright, info.copyrights) {
            while (copyright.check()) {
                tmp << "\n"
                    << "    <copyright>\n";

                while (copyright.check(doc_info_tags::copyright_year)) {
                    value year_start_value = copyright.consume();
                    int year_start = year_start_value.get_int();
                    int year_end =
                        copyright.check(doc_info_tags::copyright_year_end)
                            ? copyright.consume().get_int()
                            : year_start;

                    if (year_end < year_start) {
                        ++state.error_count;

                        detail::outerr(
                            state.current_file,
                            copyright.begin()->get_position())
                            << "Invalid year range: " << year_start << "-"
                            << year_end << "." << std::endl;
                    }

                    for (; year_start <= year_end; ++year_start)
                        tmp << "      <year>" << year_start << "</year>\n";
                }

                tmp << "      <holder>"
                    << doc_info_output(
                           copyright.consume(doc_info_tags::copyright_name),
                           106)
                    << "</holder>\n"
                    << "    </copyright>\n"
                    << "\n";
            }
        }

        if (!info.license.empty()) {
            tmp << "    <legalnotice id=\""
                << state.document.add_id("legal", id_category::generated)
                << "\">\n"
                << "      <para>\n"
                << "        " << doc_info_output(info.license, 103) << "\n"
                << "      </para>\n"
                << "    </legalnotice>\n"
                << "\n";
        }

        if (!info.purpose.empty()) {
            tmp << "    <" << info.doc_type << "purpose>\n"
                << "      " << doc_info_output(info.purpose, 103) << "    </"
                << info.doc_type << "purpose>\n"
                << "\n";
        }

        QUICKBOOK_FOR (value_consumer category_values, info.categories) {
            value category = category_values.optional_consume();
            if (!category.empty()) {
                tmp << "    <" << info.doc_type << "category name=\"category:"
                    << doc_info_output(category, 106) << "\"></"
                    << info.doc_type << "category>\n"
                    << "\n";
            }
            category_values.finish();
        }

        QUICKBOOK_FOR (value_consumer biblioid, info.biblioids) {
            value class_ = biblioid.consume(doc_info_tags::biblioid_class);
            value value_ = biblioid.consume(doc_info_tags::biblioid_value);

            tmp << "    <biblioid class=\"" << class_.get_quickbook() << "\">"
                << doc_info_output(value_, 106) << "</biblioid>"
                << "\n";
            biblioid.finish();
        }

        QUICKBOOK_FOR (value escaped, info.escaped_attributes) {
            tmp << "<!--quickbook-escape-prefix-->" << escaped.get_quickbook()
                << "<!--quickbook-escape-postfix-->";
        }

        if (info.doc_type != "library") {
            write_document_title(state.out, info.doc_title, info.version);
        }

        std::string docinfo = tmp.str();
        if (!docinfo.empty()) {
            state.out << "  <" << info.doc_type << "info>\n"
                      << docinfo << "  </" << info.doc_type << "info>\n"
                      << "\n";
        }

        if (info.doc_type == "library") {
            write_document_title(state.out, info.doc_title, info.version);
        }

        return info.doc_type;
    }

    void post(quickbook::state& state, std::string const& doc_type)
    {
        // We've finished generating our output. Here's what we'll do
        // *after* everything else.

        // Close any open sections.
        if (!doc_type.empty() && state.document.section_level() > 1) {
            if (state.strict_mode) {
                detail::outerr(state.current_file->path)
                    << "Missing [endsect] detected at end of file (strict "
                       "mode)."
                    << std::endl;
                ++state.error_count;
            }
            else {
                detail::outwarn(state.current_file->path)
                    << "Missing [endsect] detected at end of file."
                    << std::endl;
            }

            while (state.document.section_level() > 1) {
                state.out << "</section>";
                state.document.end_section();
            }
        }

        state.document.end_file();
        if (!doc_type.empty()) state.out << "\n</" << doc_type << ">\n\n";
    }

    static void write_document_title(
        collector& out, value const& title, value const& version)
    {
        if (!title.empty()) {
            out << "  <title>" << doc_info_output(title, 106);
            if (!version.empty()) {
                out << ' ' << doc_info_output(version, 106);
            }
            out << "</title>\n\n\n";
        }
    }
}
