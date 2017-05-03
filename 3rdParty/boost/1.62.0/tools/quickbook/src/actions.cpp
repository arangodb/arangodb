/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2005 Thomas Guest
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <numeric>
#include <functional>
#include <vector>
#include <map>
#include <set>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/distance.hpp>
#include <boost/range/algorithm/replace.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/next_prior.hpp>
#include <boost/foreach.hpp>
#include "quickbook.hpp"
#include "actions.hpp"
#include "syntax_highlight.hpp"
#include "utils.hpp"
#include "files.hpp"
#include "markups.hpp"
#include "state.hpp"
#include "state_save.hpp"
#include "grammar.hpp"
#include "native_text.hpp"
#include "block_tags.hpp"
#include "phrase_tags.hpp"
#include "document_state.hpp"
#include "include_paths.hpp"

namespace quickbook
{
    namespace {
        void write_anchors(quickbook::state& state, collector& tgt)
        {
            // TODO: This works but is a bit of an odd place to put it.
            // Might need to redefine the purpose of this function.
            if (state.source_mode_next) {
                detail::outwarn(state.source_mode_next_pos.get_file(),
                    state.source_mode_next_pos.get_position())
                    << "Temporary source mode unsupported here."
                    << std::endl;
                state.source_mode_next = 0;
            }

            for(quickbook::state::string_list::iterator
                it = state.anchors.begin(),
                end = state.anchors.end();
                it != end; ++it)
            {
                tgt << "<anchor id=\"";
                detail::print_string(*it, tgt.get());
                tgt << "\"/>";
            }
            
            state.anchors.clear();
        }
        
        std::string add_anchor(quickbook::state& state,
                boost::string_ref id,
                id_category::categories category =
                    id_category::explicit_anchor_id)
        {
            std::string placeholder = state.document.add_anchor(id, category);
            state.anchors.push_back(placeholder);
            return placeholder;
        }

        std::string get_attribute_value(quickbook::state& state,
                quickbook::value const& value)
        {
            std::string x = value.is_encoded() ?
                value.get_encoded() : detail::to_s(value.get_quickbook());

            if (x.empty()) {
                detail::outerr(value.get_file(), value.get_position())
                    << "Empty attribute value."
                    << std::endl;
                ++state.error_count;
                x = "xxx";
            }

            return x;
        }

        std::string validate_id(quickbook::state& state,
                quickbook::value const& id_value)
        {
            bool valid = true;
            std::string id = get_attribute_value(state, id_value);

            // Special case since I use dollar ids for id placeholders.
            if (id[0] == '$') { valid = false; id[0] = '_'; }

            if (qbk_version_n >= 107u) {
                char const* allowed_punctuation = "_.-";

                BOOST_FOREACH(char c, id) {
                    if (!std::isalnum(c) &&
                            !std::strchr(allowed_punctuation, c))
                        valid = false;
                }
            }

            if (!valid) {
                detail::outerr(id_value.get_file(), id_value.get_position())
                    << "Invalid id: "
                    << (id_value.is_encoded() ? id_value.get_encoded() :
                        detail::to_s(id_value.get_quickbook()))
                    << std::endl;
                ++state.error_count;
            }

            return id;
        }
    }

    bool quickbook_range::in_range() const {
        return qbk_version_n >= lower && qbk_version_n < upper;
    }

    void explicit_list_action(quickbook::state&, value);
    void header_action(quickbook::state&, value);
    void begin_section_action(quickbook::state&, value);
    void end_section_action(quickbook::state&, value, string_iterator);
    void block_action(quickbook::state&, value);
    void block_empty_action(quickbook::state&, value);
    void macro_definition_action(quickbook::state&, value);
    void template_body_action(quickbook::state&, value);
    void variable_list_action(quickbook::state&, value);
    void table_action(quickbook::state&, value);
    void xinclude_action(quickbook::state&, value);
    void include_action(quickbook::state&, value, string_iterator);
    void image_action(quickbook::state&, value);
    void anchor_action(quickbook::state&, value);
    void link_action(quickbook::state&, value);
    void phrase_action(quickbook::state&, value);
    void role_action(quickbook::state&, value);
    void footnote_action(quickbook::state&, value);
    void raw_phrase_action(quickbook::state&, value);
    void source_mode_action(quickbook::state&, value);
    void next_source_mode_action(quickbook::state&, value);
    void code_action(quickbook::state&, value);
    void do_template_action(quickbook::state&, value, string_iterator);
    
    void element_action::operator()(parse_iterator first, parse_iterator) const
    {
        value_consumer values = state.values.release();
        if(!values.check() || !state.conditional) return;
        value v = values.consume();
        values.finish();
        
        switch(v.get_tag())
        {
        case block_tags::ordered_list:
        case block_tags::itemized_list:
            return explicit_list_action(state, v);
        case block_tags::generic_heading:
        case block_tags::heading1:
        case block_tags::heading2:
        case block_tags::heading3:
        case block_tags::heading4:
        case block_tags::heading5:
        case block_tags::heading6:
            return header_action(state, v);
        case block_tags::begin_section:
            return begin_section_action(state, v);
        case block_tags::end_section:
            return end_section_action(state, v, first.base());
        case block_tags::blurb:
        case block_tags::preformatted:
        case block_tags::blockquote:
        case block_tags::warning:
        case block_tags::caution:
        case block_tags::important:
        case block_tags::note:
        case block_tags::tip:
        case block_tags::block:
            return block_action(state,v);
        case block_tags::hr:
            return block_empty_action(state,v);
        case block_tags::macro_definition:
            return macro_definition_action(state,v);
        case block_tags::template_definition:
            return template_body_action(state,v);
        case block_tags::variable_list:
            return variable_list_action(state, v);
        case block_tags::table:
            return table_action(state, v);
        case block_tags::xinclude:
            return xinclude_action(state, v);
        case block_tags::import:
        case block_tags::include:
            return include_action(state, v, first.base());
        case phrase_tags::image:
            return image_action(state, v);
        case phrase_tags::anchor:
            return anchor_action(state, v);
        case phrase_tags::url:
        case phrase_tags::link:
        case phrase_tags::funcref:
        case phrase_tags::classref:
        case phrase_tags::memberref:
        case phrase_tags::enumref:
        case phrase_tags::macroref:
        case phrase_tags::headerref:
        case phrase_tags::conceptref:
        case phrase_tags::globalref:
            return link_action(state, v);
        case phrase_tags::bold:
        case phrase_tags::italic:
        case phrase_tags::underline:
        case phrase_tags::teletype:
        case phrase_tags::strikethrough:
        case phrase_tags::quote:
        case phrase_tags::replaceable:
            return phrase_action(state, v);
        case phrase_tags::footnote:
            return footnote_action(state, v);
        case phrase_tags::escape:
            return raw_phrase_action(state, v);
        case phrase_tags::role:
            return role_action(state, v);
        case source_mode_tags::cpp:
        case source_mode_tags::python:
        case source_mode_tags::teletype:
            return source_mode_action(state, v);
        case code_tags::next_source_mode:
            return next_source_mode_action(state, v);
        case code_tags::code_block:
        case code_tags::inline_code_block:
        case code_tags::inline_code:
            return code_action(state, v);
        case template_tags::attribute_template:
        case template_tags::template_:
            return do_template_action(state, v, first.base());
        default:
            break;
        }
    }

    void break_action::operator()(parse_iterator first, parse_iterator) const
    {
        write_anchors(state, state.phrase);

        if(*first == '\\')
        {
            detail::outwarn(state.current_file, first.base())
                //<< "in column:" << pos.column << ", "
                << "'\\n' is deprecated, pleases use '[br]' instead" << ".\n";
        }

        if(!state.warned_about_breaks)
        {
            detail::outwarn(state.current_file, first.base())
                << "line breaks generate invalid boostbook "
                   "(will only note first occurrence).\n";

            state.warned_about_breaks = true;
        }
            
        state.phrase << detail::get_markup(phrase_tags::break_mark).pre;
    }

    void error_message_action::operator()(parse_iterator first, parse_iterator last) const
    {
        file_position const pos = state.current_file->position_of(first.base());

        std::string value(first, last);
        std::string formatted_message = message;
        boost::replace_all(formatted_message, "%s", value);
        boost::replace_all(formatted_message, "%c",
            boost::lexical_cast<std::string>(pos.column));

        detail::outerr(state.current_file->path, pos.line)
            << formatted_message << std::endl;
        ++state.error_count;
    }

    void error_action::operator()(parse_iterator first, parse_iterator /*last*/) const
    {
        file_position const pos = state.current_file->position_of(first.base());

        detail::outerr(state.current_file->path, pos.line)
            << "Syntax Error near column " << pos.column << ".\n";
        ++state.error_count;
    }

    void block_action(quickbook::state& state, value block)
    {
        write_anchors(state, state.out);

        detail::markup markup = detail::get_markup(block.get_tag());

        value_consumer values = block;
        state.out << markup.pre << values.consume().get_encoded() << markup.post;
        values.finish();
    }

    void block_empty_action(quickbook::state& state, value block)
    {
        write_anchors(state, state.out);

        detail::markup markup = detail::get_markup(block.get_tag());
        state.out << markup.pre;
    }

    void phrase_action(quickbook::state& state, value phrase)
    {
        write_anchors(state, state.phrase);

        detail::markup markup = detail::get_markup(phrase.get_tag());

        value_consumer values = phrase;
        state.phrase << markup.pre << values.consume().get_encoded() << markup.post;
        values.finish();
    }

    void role_action(quickbook::state& state, value role_list)
    {
        write_anchors(state, state.phrase);

        value_consumer values = role_list;
        value role = values.consume();
        value phrase = values.consume();
        values.finish();

        state.phrase
            << "<phrase role=\"";
        detail::print_string(get_attribute_value(state, role),
                state.phrase.get());
        state.phrase
            << "\">"
            << phrase.get_encoded()
            << "</phrase>";
    }

    void footnote_action(quickbook::state& state, value phrase)
    {
        write_anchors(state, state.phrase);

        value_consumer values = phrase;
        state.phrase
            << "<footnote id=\""
            << state.document.add_id("f", id_category::numbered)
            << "\"><para>"
            << values.consume().get_encoded()
            << "</para></footnote>";
        values.finish();
    }

    void raw_phrase_action(quickbook::state& state, value phrase)
    {
        write_anchors(state, state.phrase);

        detail::markup markup = detail::get_markup(phrase.get_tag());
        state.phrase << markup.pre << phrase.get_quickbook() << markup.post;
    }

    void paragraph_action::operator()() const
    {
        std::string str;
        state.phrase.swap(str);

        std::string::const_iterator
            pos = str.begin(),
            end = str.end();

        while(pos != end && cl::space_p.test(*pos)) ++pos;

        if(pos != end) {
            detail::markup markup = state.in_list ?
                detail::get_markup(block_tags::paragraph_in_list) :
                detail::get_markup(block_tags::paragraph);
            state.out << markup.pre << str;
            write_anchors(state, state.out);
            state.out << markup.post;
        }
    }

    void explicit_list_action::operator()() const
    {
        state.explicit_list = true;
    }

    void phrase_end_action::operator()() const
    {
        write_anchors(state, state.phrase);
    }
    
    namespace {
        void write_bridgehead(quickbook::state& state, int level,
            std::string const& str, std::string const& id, bool self_link)
        {
            if (self_link && !id.empty())
            {
                state.out << "<bridgehead renderas=\"sect" << level << "\"";
                state.out << " id=\"";
                state.out << state.document.add_id("h", id_category::numbered);
                state.out << "\">";
                state.out << "<phrase id=\"" << id << "\"/>";
                state.out << "<link linkend=\"" << id << "\">";
                state.out << str;
                state.out << "</link>";
                state.out << "</bridgehead>";
            }
            else
            {
                state.out << "<bridgehead renderas=\"sect" << level << "\"";
                if(!id.empty()) state.out << " id=\"" << id << "\"";
                state.out << ">";
                state.out << str;
                state.out << "</bridgehead>";
            }
        }
    }

    void header_action(quickbook::state& state, value heading_list)
    {
        value_consumer values = heading_list;

        bool generic = heading_list.get_tag() == block_tags::generic_heading;
        value element_id = values.optional_consume(general_tags::element_id);
        value content = values.consume();
        values.finish();

        int level;

        if (generic)
        {
            level = state.document.section_level() + 1;
                                            // We need to use a heading which is one greater
                                            // than the current.
            if (level > 6 )                 // The max is h6, clip it if it goes
                level =  6;                 // further than that
        }
        else
        {
            level = heading_list.get_tag() - block_tags::heading1 + 1;
        }

        write_anchors(state, state.out);

        if (!element_id.empty())
        {
            // Use an explicit id.

            std::string anchor = state.document.add_id(
                validate_id(state, element_id),
                id_category::explicit_id);

            write_bridgehead(state, level,
                content.get_encoded(), anchor, self_linked_headers);
        }
        else if (state.document.compatibility_version() >= 106u)
        {
            // Generate ids for 1.6+

            std::string anchor = state.document.add_id(
                detail::make_identifier(content.get_quickbook()),
                id_category::generated_heading);

            write_bridgehead(state, level,
                content.get_encoded(), anchor, self_linked_headers);
        }
        else
        {
            // Generate ids that are compatible with older versions of quickbook.

            // Older versions of quickbook used the generated boostbook, but
            // we only have an intermediate version which can contain id
            // placeholders. So to generate the ids they must be replaced
            // by the ids that the older versions would have used - i.e. the
            // unresolved ids.
            //
            // Note that this doesn't affect the actual boostbook generated for
            // the content, it's just used to generate this id.

            std::string id = detail::make_identifier(
                    state.document.replace_placeholders_with_unresolved_ids(
                        content.get_encoded()));

            if (generic || state.document.compatibility_version() >= 103) {
                std::string anchor =
                    state.document.add_id(id, id_category::generated_heading);

                write_bridgehead(state, level,
                    content.get_encoded(), anchor, self_linked_headers);
            }
            else {
                std::string anchor =
                    state.document.old_style_id(id, id_category::generated_heading);

                write_bridgehead(state, level,
                    content.get_encoded(), anchor, false);
            }
        }
    }

    void simple_phrase_action::operator()(char mark) const
    {
        write_anchors(state, state.phrase);

        int tag =
            mark == '*' ? phrase_tags::bold :
            mark == '/' ? phrase_tags::italic :
            mark == '_' ? phrase_tags::underline :
            mark == '=' ? phrase_tags::teletype :
            0;
        
        assert(tag != 0);
        detail::markup markup = detail::get_markup(tag);

        value_consumer values = state.values.release();
        value content = values.consume();
        values.finish();

        state.phrase << markup.pre;
        state.phrase << content.get_encoded();
        state.phrase << markup.post;
    }

    bool cond_phrase_push::start()
    {
        value_consumer values = state.values.release();

        saved_conditional = state.conditional;

        if (saved_conditional)
        {
            boost::string_ref macro1 = values.consume().get_quickbook();
            std::string macro(macro1.begin(), macro1.end());

            state.conditional = find(state.macro, macro.c_str());

            if (!state.conditional) {
                state.push_output();
                state.anchors.swap(anchors);
            }
        }

        return true;
    }
    
    void cond_phrase_push::cleanup()
    {
        if (saved_conditional && !state.conditional)
        {
            state.pop_output();
            state.anchors.swap(anchors);
        }

        state.conditional = saved_conditional;
    }

    void state::start_list(char mark)
    {
        push_tagged_source_mode(source_mode_next);
        source_mode_next = 0;

        write_anchors(*this, (in_list ? phrase : out));
        assert(mark == '*' || mark == '#');
        push_output();
        out << ((mark == '#') ? "<orderedlist>\n" : "<itemizedlist>\n");
        in_list = true;
    }

    void state::end_list(char mark)
    {
        write_anchors(*this, out);
        assert(mark == '*' || mark == '#');
        out << ((mark == '#') ? "\n</orderedlist>" : "\n</itemizedlist>");

        std::string list_output;
        out.swap(list_output);

        pop_output();

        (in_list ? phrase : out) << list_output;

        pop_tagged_source_mode();
    }

    void state::start_list_item()
    {
        out << "<listitem>";
        write_anchors(*this, phrase);
    }

    void state::end_list_item()
    {
        write_anchors(*this, phrase);
        paragraph_action para(*this);
        para();
        out << "</listitem>";
    }

    namespace
    {
        bool parse_template(value const&, quickbook::state& state,
                bool is_attribute_template = false);
    }

    void state::start_callouts()
    {
        ++callout_depth;
    }

    std::string state::add_callout(value v)
    {
        std::string callout_id1 = document.add_id("c", id_category::numbered);
        std::string callout_id2 = document.add_id("c", id_category::numbered);

        callouts.insert(encoded_value(callout_id1));
        callouts.insert(encoded_value(callout_id2));
        callouts.insert(v);

        std::string code;
        code += "<co id=\"" + callout_id1 + "\" ";
        code += "linkends=\"" + callout_id2 + "\" />";

        return code;
    }

    std::string state::end_callouts()
    {
        assert(callout_depth > 0);
        std::string block;

        --callout_depth;
        if (callout_depth > 0) return block;

        value_consumer c = callouts.release();
        if (!c.check()) return block;

        block += "<calloutlist>";
        while (c.check())
        {
            std::string callout_id1 = c.consume().get_encoded();
            std::string callout_id2 = c.consume().get_encoded();
            value callout_body = c.consume();

            std::string callout_value;

            {
                state_save save(*this, state_save::scope_all);
                ++template_depth;

                bool r = parse_template(callout_body, *this);

                if(!r)
                {
                    detail::outerr(callout_body.get_file(), callout_body.get_position())
                        << "Expanding callout." << std::endl
                        << "------------------begin------------------" << std::endl
                        << callout_body.get_quickbook()
                        << std::endl
                        << "------------------end--------------------" << std::endl
                        ;
                    ++error_count;
                }

                out.swap(callout_value);
            }
            
            block += "<callout arearefs=\"" + callout_id1 + "\" ";
            block += "id=\"" + callout_id2 + "\">";
            block += callout_value;
            block += "</callout>";
        }
        block += "</calloutlist>";
        
        return block;
    }

    void explicit_list_action(quickbook::state& state, value list)
    {
        write_anchors(state, state.out);

        detail::markup markup = detail::get_markup(list.get_tag());

        state.out << markup.pre;

        BOOST_FOREACH(value item, list)
        {
            state.out << "<listitem>";
            state.out << item.get_encoded();
            state.out << "</listitem>";
        }

        state.out << markup.post;
    }

    void anchor_action(quickbook::state& state, value anchor)
    {
        value_consumer values = anchor;
        value anchor_id = values.consume();
        // Note: anchor_id is never encoded as boostbook. If it
        // is encoded, it's just things like escapes.
        add_anchor(state, validate_id(state, anchor_id));
        values.finish();
    }

    void do_macro_action::operator()(std::string const& str) const
    {
        write_anchors(state, state.phrase);

        if (str == quickbook_get_date)
        {
            char strdate[64];
            strftime(strdate, sizeof(strdate), "%Y-%b-%d", current_time);
            state.phrase << strdate;
        }
        else if (str == quickbook_get_time)
        {
            char strdate[64];
            strftime(strdate, sizeof(strdate), "%I:%M:%S %p", current_time);
            state.phrase << strdate;
        }
        else
        {
            state.phrase << str;
        }
    }

    void raw_char_action::operator()(char ch) const
    {
        state.phrase << ch;
    }

    void raw_char_action::operator()(parse_iterator first, parse_iterator last) const
    {
        while (first != last)
            state.phrase << *first++;
    }

    void source_mode_action(quickbook::state& state, value source_mode)
    {
        state.change_source_mode(source_mode.get_tag());
    }

    void next_source_mode_action(quickbook::state& state, value source_mode)
    {
        value_consumer values = source_mode;
        state.source_mode_next_pos = values.consume();
        state.source_mode_next = values.consume().get_int();
        values.finish();
    }

    void code_action(quickbook::state& state, value code_block)
    {
        int code_tag = code_block.get_tag();

        value_consumer values = code_block;
        boost::string_ref code_value = values.consume().get_quickbook();
        values.finish();

        bool inline_code = code_tag == code_tags::inline_code ||
            (code_tag == code_tags::inline_code_block && qbk_version_n < 106u);
        bool block = code_tag != code_tags::inline_code;

        source_mode_type source_mode = state.source_mode_next ?
            state.source_mode_next : state.current_source_mode().source_mode;
        state.source_mode_next = 0;

        if (inline_code) {
            write_anchors(state, state.phrase);
        }
        else {
            paragraph_action para(state);
            para();
            write_anchors(state, state.out);
        }

        if (block) {
            // preprocess the code section to remove the initial indentation
            mapped_file_builder mapped;
            mapped.start(state.current_file);
            mapped.unindent_and_add(code_value);

            file_ptr f = mapped.release();

            if (f->source().empty())
                return; // Nothing left to do here. The program is empty.

            if (qbk_version_n >= 107u) state.start_callouts();

            parse_iterator first_(f->source().begin());
            parse_iterator last_(f->source().end());

            file_ptr saved_file = f;
            boost::swap(state.current_file, saved_file);

            // print the code with syntax coloring
            //
            // We must not place a \n after the <programlisting> tag
            // otherwise PDF output starts code blocks with a blank line:
            state.phrase << "<programlisting>";
            syntax_highlight(first_, last_, state, source_mode, block);
            state.phrase << "</programlisting>\n";

            boost::swap(state.current_file, saved_file);

            if (qbk_version_n >= 107u) state.phrase << state.end_callouts();

            if (!inline_code) {
                state.out << state.phrase.str();
                state.phrase.clear();
            }
        }
        else {
            parse_iterator first_(code_value.begin());
            parse_iterator last_(code_value.end());

            state.phrase << "<code>";
            syntax_highlight(first_, last_, state, source_mode, block);
            state.phrase << "</code>";
        }
    }

    void plain_char_action::operator()(char ch) const
    {
        write_anchors(state, state.phrase);

        detail::print_char(ch, state.phrase.get());
    }

    void plain_char_action::operator()(parse_iterator first, parse_iterator last) const
    {
        write_anchors(state, state.phrase);

        while (first != last)
            detail::print_char(*first++, state.phrase.get());
    }

    void escape_unicode_action::operator()(parse_iterator first, parse_iterator last) const
    {
        write_anchors(state, state.phrase);

        while(first != last && *first == '0') ++first;

        // Just ignore \u0000
        // Maybe I should issue a warning?
        if(first == last) return;
        
        std::string hex_digits(first, last);
        
        if(hex_digits.size() == 2 && *first > '0' && *first <= '7') {
            using namespace std;
            detail::print_char(strtol(hex_digits.c_str(), 0, 16),
                    state.phrase.get());
        }
        else {
            state.phrase << "&#x" << hex_digits << ";";
        }
    }

    void write_plain_text(std::ostream& out, value const& v)
    {
        if (v.is_encoded())
        {
            detail::print_string(v.get_encoded(), out);
        }
        else {
            boost::string_ref value = v.get_quickbook();
            for(boost::string_ref::const_iterator
                first = value.begin(), last  = value.end();
                first != last; ++first)
            {
                if (*first == '\\' && ++first == last) break;
                detail::print_char(*first, out);
            }
        }
    }

    void image_action(quickbook::state& state, value image)
    {
        write_anchors(state, state.phrase);

        // Note: attributes are never encoded as boostbook, if they're
        // encoded, it's just things like escapes.
        typedef std::map<std::string, value> attribute_map;
        attribute_map attributes;

        value_consumer values = image;
        attributes["fileref"] = values.consume();

        BOOST_FOREACH(value pair_, values)
        {
            value_consumer pair = pair_;
            value name = pair.consume();
            value value = pair.consume();
            std::string name_str(name.get_quickbook().begin(),
                name.get_quickbook().end());
            pair.finish();
            if(!attributes.insert(std::make_pair(name_str, value)).second)
            {
                detail::outwarn(name.get_file(), name.get_position())
                    << "Duplicate image attribute: "
                    << name.get_quickbook()
                    << std::endl;
            }
        }
        
        values.finish();

        // Find the file basename and extension.
        //
        // Not using Boost.Filesystem because I want to stay in UTF-8.
        // Need to think about uri encoding.
        
        std::string fileref = attributes["fileref"].is_encoded() ?
            attributes["fileref"].get_encoded() :
            detail::to_s(attributes["fileref"].get_quickbook());

        // Check for windows paths, then convert.
        // A bit crude, but there you go.

        if(fileref.find('\\') != std::string::npos)
        {
            (qbk_version_n >= 106u ?
                detail::outerr(attributes["fileref"].get_file(), attributes["fileref"].get_position()) :
                detail::outwarn(attributes["fileref"].get_file(), attributes["fileref"].get_position()))
                << "Image path isn't portable: '"
                << fileref
                << "'"
                << std::endl;
            if (qbk_version_n >= 106u) ++state.error_count;
        }

        boost::replace(fileref, '\\', '/');

        // Find the file basename and extension.
        //
        // Not using Boost.Filesystem because I want to stay in UTF-8.
        // Need to think about uri encoding.

        std::string::size_type pos;
        std::string stem, extension;

        pos = fileref.rfind('/');
        stem = pos == std::string::npos ?
            fileref :
            fileref.substr(pos + 1);

        pos = stem.rfind('.');
        if (pos != std::string::npos)
        {
            extension = stem.substr(pos + 1);
            stem = stem.substr(0, pos);
        }

        // Extract the alt tag, to use as a text description.
        // Or if there isn't one, use the stem of the file name.

        attribute_map::iterator alt_pos = attributes.find("alt");
        quickbook::value alt_text =
            alt_pos != attributes.end() ? alt_pos->second :
            qbk_version_n < 106u ? encoded_value(stem) :
            quickbook::value();
        attributes.erase("alt");

        if(extension == "svg")
        {
           //
           // SVG's need special handling:
           //
           // 1) We must set the "format" attribute, otherwise
           //    HTML generation produces code that will not display
           //    the image at all.
           // 2) We need to set the "contentwidth" and "contentdepth"
           //    attributes, otherwise the image will be displayed inside
           //    a tiny box with scrollbars (Firefox), or else cropped to
           //    fit in a tiny box (IE7).
           //

           attributes.insert(attribute_map::value_type("format",
                encoded_value("SVG")));

           //
           // Image paths are relative to the html subdirectory:
           //
           fs::path img = detail::generic_to_path(fileref);
           if (!img.has_root_directory())
              img = quickbook::image_location / img;  // relative path

           //
           // Now load the SVG file:
           //
           std::string svg_text;
           if (state.dependencies.add_dependency(img)) {
              fs::ifstream fs(img);
              std::stringstream buffer;
              buffer << fs.rdbuf();
              svg_text = buffer.str();
           }

           //
           // Extract the svg header from the file:
           //
           std::string::size_type a, b;
           a = svg_text.find("<svg");
           b = svg_text.find('>', a);
           svg_text = (a == std::string::npos) ? "" : svg_text.substr(a, b - a);
           //
           // Now locate the "width" and "height" attributes
           // and borrow their values:
           //
           a = svg_text.find("width");
           a = svg_text.find('=', a);
           a = svg_text.find('\"', a);
           b = svg_text.find('\"', a + 1);
           if(a != std::string::npos)
           {
              attributes.insert(std::make_pair(
                "contentwidth", encoded_value(std::string(
                    svg_text.begin() + a + 1, svg_text.begin() + b))
                ));
           }
           a = svg_text.find("height");
           a = svg_text.find('=', a);
           a = svg_text.find('\"', a);
           b = svg_text.find('\"', a + 1);
           if(a != std::string::npos)
           {
              attributes.insert(std::make_pair(
                "contentdepth", encoded_value(std::string(
                    svg_text.begin() + a + 1, svg_text.begin() + b))
                ));
           }
        }

        state.phrase << "<inlinemediaobject>";

        state.phrase << "<imageobject><imagedata";
        
        BOOST_FOREACH(attribute_map::value_type const& attr, attributes)
        {
            state.phrase << " " << attr.first << "=\"";
            write_plain_text(state.phrase.get(), attr.second);
            state.phrase << "\"";
        }

        state.phrase << "></imagedata></imageobject>";

        // Add a textobject containing the alt tag from earlier.
        // This will be used for the alt tag in html.
        if (alt_text.check()) {
            state.phrase << "<textobject><phrase>";
            write_plain_text(state.phrase.get(), alt_text);
            state.phrase << "</phrase></textobject>";
        }

        state.phrase << "</inlinemediaobject>";
    }

    void macro_definition_action(quickbook::state& state, quickbook::value macro_definition)
    {
        value_consumer values = macro_definition;
        std::string macro_id = detail::to_s(values.consume().get_quickbook());
        value phrase_value = values.optional_consume();
        std::string phrase;
        if (phrase_value.check()) phrase = phrase_value.get_encoded();
        values.finish();

        std::string* existing_macro =
            boost::spirit::classic::find(state.macro, macro_id.c_str());
        quickbook::ignore_variable(&existing_macro);

        if (existing_macro)
        {
            if (qbk_version_n < 106) return;

            // Do this if you're using spirit's TST.
            //
            // *existing_macro = phrase;
            // return;
        }

        state.macro.add(
            macro_id.begin()
          , macro_id.end()
          , phrase);
    }

    void template_body_action(quickbook::state& state, quickbook::value template_definition)
    {
        value_consumer values = template_definition;
        std::string identifier = detail::to_s(values.consume().get_quickbook());

        std::vector<std::string> template_values;
        BOOST_FOREACH(value const& p, values.consume()) {
            template_values.push_back(detail::to_s(p.get_quickbook()));
        }

        BOOST_ASSERT(values.check(template_tags::block) || values.check(template_tags::phrase));
        value body = values.consume();
        BOOST_ASSERT(!values.check());
    
        if (!state.templates.add(
            template_symbol(
                identifier,
                template_values,
                body,
                &state.templates.top_scope())))
        {
            detail::outwarn(body.get_file(), body.get_position())
                << "Template Redefinition: " << identifier << std::endl;
            ++state.error_count;
        }
    }

    namespace
    {
        string_iterator find_first_seperator(string_iterator begin, string_iterator end)
        {
            if(qbk_version_n < 105) {
                for(;begin != end; ++begin)
                {
                    switch(*begin)
                    {
                    case ' ':
                    case '\t':
                    case '\n':
                    case '\r':
                        return begin;
                    default:
                        break;
                    }
                }
            }
            else {
                unsigned int depth = 0;

                for(;begin != end; ++begin)
                {
                    switch(*begin)
                    {
                    case '[':
                        ++depth;
                        break;
                    case '\\':
                        if(++begin == end) return begin;
                        break;
                    case ']':
                        if (depth > 0) --depth;
                        break;
                    case ' ':
                    case '\t':
                    case '\n':
                    case '\r':
                        if (depth == 0) return begin;
                    default:
                        break;
                    }
                }
            }
            
            return begin;
        }
        
        std::pair<string_iterator, string_iterator> find_seperator(string_iterator begin, string_iterator end)
        {
            string_iterator first = begin = find_first_seperator(begin, end);

            for(;begin != end; ++begin)
            {
                switch(*begin)
                {
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    break;
                default:
                    return std::make_pair(first, begin);
                }
            }
            
            return std::make_pair(first, begin);
        }
    
        void break_arguments(
            std::vector<value>& args
          , std::vector<std::string> const& params
          , fs::path const& filename
        )
        {
            // Quickbook 1.4-: If there aren't enough parameters seperated by
            //                 '..' then seperate the last parameter using
            //                 whitespace.
            // Quickbook 1.5+: If '..' isn't used to seperate the parameters
            //                 then use whitespace to separate them
            //                 (2 = template name + argument).

            if (qbk_version_n < 105 || args.size() == 1)
            {
           
                while (args.size() < params.size())
                {
                    // Try to break the last argument at the first space found
                    // and push it into the back of args. Do this
                    // recursively until we have all the expected number of
                    // arguments, or if there are no more spaces left.

                    value last_arg = args.back();
                    string_iterator begin = last_arg.get_quickbook().begin();
                    string_iterator end = last_arg.get_quickbook().end();
                    
                    std::pair<string_iterator, string_iterator> pos =
                        find_seperator(begin, end);
                    if (pos.second == end) break;
                    value new_arg(
                        qbk_value(last_arg.get_file(),
                            pos.second, end, template_tags::phrase));

                    args.back() = qbk_value(last_arg.get_file(),
                        begin, pos.first, last_arg.get_tag());
                    args.push_back(new_arg);
                }
            }
        }

        std::pair<bool, std::vector<std::string>::const_iterator>
        get_arguments(
            std::vector<value> const& args
          , std::vector<std::string> const& params
          , template_scope const& scope
          , string_iterator first
          , quickbook::state& state
        )
        {
            std::vector<value>::const_iterator arg = args.begin();
            std::vector<std::string>::const_iterator tpl = params.begin();
            std::vector<std::string> empty_params;

            // Store each of the argument passed in as local templates:
            while (arg != args.end())
            {
                if (!state.templates.add(
                        template_symbol(*tpl, empty_params, *arg, &scope)))
                {
                    detail::outerr(state.current_file, first)
                        << "Duplicate Symbol Found" << std::endl;
                    ++state.error_count;
                    return std::make_pair(false, tpl);
                }
                ++arg; ++tpl;
            }
            return std::make_pair(true, tpl);
        }
        
        bool parse_template(
            value const& content
          , quickbook::state& state
          , bool is_attribute_template
        )
        {
            file_ptr saved_current_file = state.current_file;

            state.current_file = content.get_file();
            boost::string_ref source = content.get_quickbook();

            parse_iterator first(source.begin());
            parse_iterator last(source.end());

            bool r = cl::parse(first, last,
                    is_attribute_template ?
                        state.grammar().attribute_template_body :
                    content.get_tag() == template_tags::phrase ?
                        state.grammar().inline_phrase :
                        state.grammar().block_start
                ).full;

            boost::swap(state.current_file, saved_current_file);

            return r;
        }
    }

    void call_template(quickbook::state& state,
            template_symbol const* symbol,
            std::vector<value> const& args,
            string_iterator first,
            bool is_attribute_template = false)
    {
        bool is_block = symbol->content.get_tag() != template_tags::phrase;
        assert(!(is_attribute_template && is_block));

        quickbook::paragraph_action paragraph_action(state);

        // Finish off any existing paragraphs.
        if (is_block) paragraph_action();

        // If this template contains already encoded text, then just
        // write it out, without going through any of the rigamarole.

        if (symbol->content.is_encoded())
        {
            (is_block ? state.out : state.phrase) << symbol->content.get_encoded();
            return;
        }

        // The template arguments should have the scope that the template was
        // called from, not the template's own scope.
        //
        // Note that for quickbook 1.4- this value is just ignored when the
        // arguments are expanded.
        template_scope const& call_scope = state.templates.top_scope();

        {
            state_save save(state, state_save::scope_callables);
            std::string save_block;
            std::string save_phrase;

            state.templates.start_template(symbol);

            qbk_version_n = symbol->content.get_file()->version();

            ++state.template_depth;
            if (state.template_depth > state.max_template_depth)
            {
                detail::outerr(state.current_file, first)
                    << "Infinite loop detected" << std::endl;
                ++state.error_count;
                return;
            }

            // Store the current section level so that we can ensure that
            // [section] and [endsect] tags in the template are balanced.
            state.min_section_level = state.document.section_level();

            ///////////////////////////////////
            // Prepare the arguments as local templates
            bool get_arg_result;
            std::vector<std::string>::const_iterator tpl;
            boost::tie(get_arg_result, tpl) =
                get_arguments(args, symbol->params, call_scope, first, state);

            if (!get_arg_result)
            {
                return;
            }

            ///////////////////////////////////
            // parse the template body:

            if (symbol->content.get_file()->version() < 107u) {
                state.out.swap(save_block);
                state.phrase.swap(save_phrase);
            }

            if (!parse_template(symbol->content, state, is_attribute_template))
            {
                detail::outerr(state.current_file, first)
                    << "Expanding "
                    << (is_block ? "block" : "phrase")
                    << " template: " << symbol->identifier << "\n\n"
                    << "------------------begin------------------\n"
                    << symbol->content.get_quickbook()
                    << "------------------end--------------------\n"
                    << std::endl;
                ++state.error_count;
                return;
            }

            if (state.document.section_level() != state.min_section_level)
            {
                detail::outerr(state.current_file, first)
                    << "Mismatched sections in template "
                    << symbol->identifier
                    << std::endl;
                ++state.error_count;
                return;
            }

            if (symbol->content.get_file()->version() < 107u) {
                state.out.swap(save_block);
                state.phrase.swap(save_phrase);

                if(is_block || !save_block.empty()) {
                    paragraph_action();
                    state.out << save_block;
                    state.phrase << save_phrase;
                    paragraph_action();
                }
                else {
                    state.phrase << save_phrase;
                }
            }
            else
            {
                if (is_block) paragraph_action();
            }
        }
    }

    void call_code_snippet(quickbook::state& state,
            template_symbol const* symbol,
            string_iterator first)
    {
        assert(symbol->params.size() == 0);
        std::vector<value> args;

        // Create a fake symbol for call_template
        template_symbol t(
            symbol->identifier,
            symbol->params,
            symbol->content,
            symbol->lexical_parent);

        state.start_callouts();
        call_template(state, &t, args, first);
        state.out << state.end_callouts();
    }

    void do_template_action(quickbook::state& state, value template_list,
            string_iterator first)
    {
        bool const is_attribute_template =
            template_list.get_tag() == template_tags::attribute_template;

        // Get the arguments
        value_consumer values = template_list;

        bool template_escape = values.check(template_tags::escape);
        if(template_escape) values.consume();

        std::string identifier = detail::to_s(values.consume(template_tags::identifier).get_quickbook());

        std::vector<value> args;

        BOOST_FOREACH(value arg, values)
        {
            args.push_back(arg);
        }
        
        values.finish();

        template_symbol const* symbol = state.templates.find(identifier);
        BOOST_ASSERT(symbol);

        // Deal with escaped templates.

        if (template_escape)
        {
            if (!args.empty())
            {
                detail::outerr(state.current_file, first)
                    << "Arguments for escaped template."
                    <<std::endl;
                ++state.error_count;
            }

            if (symbol->content.is_encoded())
            {
                state.phrase << symbol->content.get_encoded();
            }
            else
            {
                state.phrase << symbol->content.get_quickbook();

                /*

                This would surround the escaped template in escape
                comments to indicate to the post-processor that it
                isn't quickbook generated markup. But I'm not sure if
                it would work.

                quickbook::detail::markup escape_markup
                    = detail::get_markup(phrase_tags::escape);

                state.phrase
                    << escape_markup.pre
                    << symbol->content.get_quickbook()
                    << escape_markup.post
                    ;
                */
            }

            return;
        }

        ///////////////////////////////////
        // Check that attribute templates are phrase templates

        if (is_attribute_template &&
                symbol->content.get_tag() != template_tags::phrase)
        {
            detail::outerr(state.current_file, first)
                << "Only phrase templates can be used in attribute values."
                << std::endl;

            ++state.error_count;
            return;
        }

        ///////////////////////////////////
        // Initialise the arguments

        switch(symbol->content.get_tag())
        {
        case template_tags::block:
        case template_tags::phrase:
            // Break the arguments for a template

            break_arguments(args, symbol->params, state.current_file->path);

            if (args.size() != symbol->params.size())
            {
                detail::outerr(state.current_file, first)
                    << "Invalid number of arguments passed. Expecting: "
                    << symbol->params.size()
                    << " argument(s), got: "
                    << args.size()
                    << " argument(s) instead."
                    << std::endl;

                ++state.error_count;
                return;
            }

            call_template(state, symbol, args, first, is_attribute_template);
            break;

        case template_tags::snippet:

            if (!args.empty())
            {
                detail::outerr(state.current_file, first)
                    << "Arguments for code snippet."
                    <<std::endl;
                ++state.error_count;

                args.clear();
            }

            call_code_snippet(state, symbol, first);
            break;

        default:
            assert(0);
        }
    }

    void link_action(quickbook::state& state, value link)
    {
        write_anchors(state, state.phrase);

        detail::markup markup = detail::get_markup(link.get_tag());

        value_consumer values = link;
        value dst_value = values.consume();
        value content = values.consume();
        values.finish();

        std::string dst;

        if (link.get_tag() == phrase_tags::link) {
            dst = validate_id(state, dst_value);
        }
        else {
            dst = get_attribute_value(state, dst_value);

            // TODO: Might be better to have an error for some invalid urls.
            if (link.get_tag() == phrase_tags::url) {
                dst = detail::partially_escape_uri(dst);
            }
        } 
        
        state.phrase << markup.pre;
        detail::print_string(dst, state.phrase.get());
        state.phrase << "\">";

        if (content.empty())
            detail::print_string(dst, state.phrase.get());
        else
            state.phrase << content.get_encoded();

        state.phrase << markup.post;
    }

    void variable_list_action(quickbook::state& state, value variable_list)
    {
        write_anchors(state, state.out);

        value_consumer values = variable_list;
        std::string title = detail::to_s(values.consume(table_tags::title).get_quickbook());

        state.out << "<variablelist>\n";

        state.out << "<title>";
        detail::print_string(title, state.out.get());
        state.out << "</title>\n";

        BOOST_FOREACH(value_consumer entry, values) {
            state.out << "<varlistentry>";
            
            if(entry.check()) {
                state.out << "<term>";
                state.out << entry.consume().get_encoded();
                state.out << "</term>";
            }
            
            if(entry.check()) {
                state.out << "<listitem>";
                BOOST_FOREACH(value phrase, entry) state.out << phrase.get_encoded();
                state.out << "</listitem>";
            }

            state.out << "</varlistentry>\n";
        }

        state.out << "</variablelist>\n";
        
        values.finish();
    }

    void table_action(quickbook::state& state, value table)
    {
        write_anchors(state, state.out);

        value_consumer values = table;

        std::string element_id;
        if(values.check(general_tags::element_id)) {
            element_id = validate_id(state, values.consume());
        }

        value title = values.consume(table_tags::title);
        bool has_title = !title.empty();
        
        std::string table_id;

        if (!element_id.empty()) {
            table_id = state.document.add_id(element_id, id_category::explicit_id);
        }
        else if (has_title) {
            if (state.document.compatibility_version() >= 105) {
                table_id = state.document.add_id(detail::make_identifier(title.get_quickbook()), id_category::generated);
            }
            else {
                table_id = state.document.add_id("t", id_category::numbered);
            }
        }

        // Emulating the old behaviour which used the width of the final
        // row for span_count.
        int row_count = 0;
        int span_count = 0;

        value_consumer lookahead = values;
        BOOST_FOREACH(value row, lookahead) {
            ++row_count;
            span_count = boost::distance(row);
        }
        lookahead.finish();

        if (has_title)
        {
            state.out << "<table frame=\"all\"";
            if(!table_id.empty())
                state.out << " id=\"" << table_id << "\"";
            state.out << ">\n";
            state.out << "<title>";
            if (qbk_version_n < 106u) {
                detail::print_string(title.get_quickbook(), state.out.get());
            }
            else {
                state.out << title.get_encoded();
            }
            state.out << "</title>";
        }
        else
        {
            state.out << "<informaltable frame=\"all\"";
            if(!table_id.empty())
                state.out << " id=\"" << table_id << "\"";
            state.out << ">\n";
        }

        state.out << "<tgroup cols=\"" << span_count << "\">\n";

        if (row_count > 1)
        {
            state.out << "<thead>" << "<row>";
            BOOST_FOREACH(value cell, values.consume()) {
                state.out << "<entry>" << cell.get_encoded() << "</entry>";
            }
            state.out << "</row>\n" << "</thead>\n";
        }

        state.out << "<tbody>\n";

        BOOST_FOREACH(value row, values) {
            state.out << "<row>";
            BOOST_FOREACH(value cell, row) {
                state.out << "<entry>" << cell.get_encoded() << "</entry>";
            }
            state.out << "</row>\n";
        }
        
        values.finish();

        state.out << "</tbody>\n"
            << "</tgroup>\n";

        if (has_title)
        {
            state.out << "</table>\n";
        }
        else
        {
            state.out << "</informaltable>\n";
        }
    }

    void begin_section_action(quickbook::state& state, value begin_section_list)
    {    
        value_consumer values = begin_section_list;

        value element_id = values.optional_consume(general_tags::element_id);
        value content = values.consume();
        values.finish();

        std::string full_id = state.document.begin_section(
            element_id.empty() ?
                detail::make_identifier(content.get_quickbook()) :
                validate_id(state, element_id),
            element_id.empty() ?
                id_category::generated_section :
                id_category::explicit_section_id,
            state.current_source_mode());

        state.out << "\n<section id=\"" << full_id << "\">\n";
        state.out << "<title>";

        write_anchors(state, state.out);

        if (self_linked_headers && state.document.compatibility_version() >= 103)
        {
            state.out << "<link linkend=\"" << full_id << "\">"
                << content.get_encoded()
                << "</link>"
                ;
        }
        else
        {
            state.out << content.get_encoded();
        }
        
        state.out << "</title>\n";
    }

    void end_section_action(quickbook::state& state, value end_section, string_iterator first)
    {
        write_anchors(state, state.out);

        if (state.document.section_level() <= state.min_section_level)
        {
            file_position const pos = state.current_file->position_of(first);

            detail::outerr(state.current_file->path, pos.line)
                << "Mismatched [endsect] near column " << pos.column << ".\n";
            ++state.error_count;
            
            return;
        }

        state.out << "</section>";
        state.document.end_section();
    }
    
    void element_id_warning_action::operator()(parse_iterator first, parse_iterator) const
    {
        detail::outwarn(state.current_file, first.base()) << "Empty id.\n";
    }

    void xinclude_action(quickbook::state& state, value xinclude)
    {
        write_anchors(state, state.out);

        value_consumer values = xinclude;
        path_parameter x = check_xinclude_path(values.consume(), state);
        values.finish();

        if (x.type == path_parameter::path)
        {
            quickbook_path path = resolve_xinclude_path(x.value, state);

            state.out << "\n<xi:include href=\"";
            detail::print_string(file_path_to_url(path.abstract_file_path), state.out.get());
            state.out << "\" />\n";
        }
    }

    void load_quickbook(quickbook::state& state,
            quickbook_path const& path,
            value::tag_type load_type,
            value const& include_doc_id = value())
    {
        assert(load_type == block_tags::include ||
            load_type == block_tags::import);

        // Check this before qbk_version_n gets changed by the inner file.
        bool keep_inner_source_mode = (qbk_version_n < 106);
        
        {
            // When importing, state doesn't scope templates and macros so that
            // they're added to the existing scope. It might be better to add
            // them to a new scope then explicitly import them into the
            // existing scope.
            //
            // For old versions of quickbook, templates aren't scoped by the
            // file.
            state_save save(state,
                load_type == block_tags::import ? state_save::scope_output :
                qbk_version_n >= 106u ? state_save::scope_callables :
                state_save::scope_macros);

            state.current_file = load(path.file_path); // Throws load_error
            state.current_path = path;
            state.imported = (load_type == block_tags::import);

            // update the __FILENAME__ macro
            state.update_filename_macro();
        
            // parse the file
            quickbook::parse_file(state, include_doc_id, true);

            // Don't restore source_mode on older versions.
            if (keep_inner_source_mode) save.source_mode = state.source_mode;
        }

        // restore the __FILENAME__ macro
        state.update_filename_macro();
    }

    void load_source_file(quickbook::state& state,
            quickbook_path const& path,
            value::tag_type load_type,
            string_iterator first,
            value const& include_doc_id = value())
    {
        assert(load_type == block_tags::include ||
            load_type == block_tags::import);

        std::string ext = path.file_path.extension().generic_string();
        std::vector<template_symbol> storage;
        // Throws load_error
        state.error_count +=
            load_snippets(path.file_path, storage, ext, load_type);

        if (load_type == block_tags::include)
        {
            state.templates.push();
        }

        BOOST_FOREACH(template_symbol& ts, storage)
        {
            std::string tname = ts.identifier;
            if (tname != "!")
            {
                ts.lexical_parent = &state.templates.top_scope();
                if (!state.templates.add(ts))
                {
                    detail::outerr(ts.content.get_file(), ts.content.get_position())
                        << "Template Redefinition: " << tname << std::endl;
                    ++state.error_count;
                }
            }
        }

        if (load_type == block_tags::include)
        {
            BOOST_FOREACH(template_symbol& ts, storage)
            {
                std::string tname = ts.identifier;

                if (tname == "!")
                {
                    ts.lexical_parent = &state.templates.top_scope();
                    call_code_snippet(state, &ts, first);
                }
            }

            state.templates.pop();
        }
    }

    void include_action(quickbook::state& state, value include, string_iterator first)
    {
        write_anchors(state, state.out);

        value_consumer values = include;
        value include_doc_id = values.optional_consume(general_tags::include_id);
        path_parameter parameter = check_path(values.consume(), state);
        values.finish();

        std::set<quickbook_path> search =
            include_search(parameter, state, first);
        std::set<quickbook_path>::iterator i = search.begin();
        std::set<quickbook_path>::iterator e = search.end();
        for (; i != e; ++i)
        {
            quickbook_path const & path = *i;
            try {
                if (qbk_version_n >= 106)
                {
                    if (state.imported && include.get_tag() == block_tags::include)
                        return;

                    std::string ext = path.file_path.extension().generic_string();

                    if (ext == ".qbk" || ext == ".quickbook")
                    {
                        load_quickbook(state, path, include.get_tag(), include_doc_id);
                    }
                    else
                    {
                        load_source_file(state, path, include.get_tag(), first, include_doc_id);
                    }
                }
                else
                {
                    if (include.get_tag() == block_tags::include)
                    {
                        load_quickbook(state, path, include.get_tag(), include_doc_id);
                    }
                    else
                    {
                        load_source_file(state, path, include.get_tag(), first, include_doc_id);
                    }
                }
            }
            catch (load_error& e) {
                ++state.error_count;

                detail::outerr(state.current_file, first)
                    << "Loading file "
                    << path.file_path
                    << ": "
                    << e.what()
                    << std::endl;
            }
        }
    }

    bool to_value_scoped_action::start(value::tag_type t)
    {
        state.push_output();
        state.anchors.swap(saved_anchors);
        tag = t;

        return true;
    }

    void to_value_scoped_action::success(parse_iterator first, parse_iterator last)
    {
        std::string value;

        if (!state.out.str().empty())
        {
            paragraph_action para(state);
            para(); // For paragraphs before the template call.
            write_anchors(state, state.out);
            state.out.swap(value);
        }
        else
        {
            write_anchors(state, state.phrase);
            state.phrase.swap(value);
        }

        state.values.builder.insert(encoded_qbk_value(
            state.current_file, first.base(), last.base(), value, tag));
    }
    
    
    void to_value_scoped_action::cleanup()
    {
        state.pop_output();
        state.anchors.swap(saved_anchors);
    }
}
