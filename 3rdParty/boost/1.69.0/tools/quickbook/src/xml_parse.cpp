/*=============================================================================
Copyright (c) 2017 Daniel James

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "xml_parse.hpp"
#include "simple_parse.hpp"
#include "stream.hpp"
#include "utils.hpp"

namespace quickbook
{
    namespace detail
    {
        // write_xml_tree

        void write_xml_tree_impl(
            std::string& out, xml_element* node, unsigned int depth);

        void write_xml_tree(xml_element* node)
        {
            std::string result;
            write_xml_tree_impl(result, node, 0);
            quickbook::detail::out() << result << std::flush;
        }

        void write_xml_tree_impl(
            std::string& out, xml_element* node, unsigned int depth)
        {
            if (!node) {
                return;
            }

            for (unsigned i = 0; i < depth; ++i) {
                out += "  ";
            }
            switch (node->type_) {
            case xml_element::element_node:
                out += "Node: ";
                out += node->name_;
                break;
            case xml_element::element_text:
                out += "Text";
                break;
            case xml_element::element_html:
                out += "HTML";
                break;
            default:
                out += "Unknown node type";
                break;
            }
            out += "\n";
            for (xml_element* it = node->children(); it; it = it->next()) {
                write_xml_tree_impl(out, it, depth + 1);
            }
        }

        // xml_parse

        void read_tag(
            xml_tree_builder&,
            string_iterator& it,
            string_iterator start,
            string_iterator end);
        void read_close_tag(
            xml_tree_builder&,
            string_iterator& it,
            string_iterator start,
            string_iterator end);
        void skip_question_mark_tag(
            string_iterator& it, string_iterator start, string_iterator end);
        void skip_exclamation_mark_tag(
            string_iterator& it, string_iterator start, string_iterator end);
        quickbook::string_view read_tag_name(
            string_iterator& it, string_iterator start, string_iterator end);
        quickbook::string_view read_attribute_value(
            string_iterator& it, string_iterator start, string_iterator end);
        quickbook::string_view read_string(
            string_iterator& it, string_iterator end);

        xml_tree xml_parse(quickbook::string_view source)
        {
            typedef string_iterator iterator;
            iterator it = source.begin(), end = source.end();

            xml_tree_builder builder;

            while (true) {
                iterator start = it;
                read_to(it, end, '<');
                if (start != it) {
                    builder.add_element(xml_element::text_node(
                        quickbook::string_view(start, it - start)));
                }

                if (it == end) {
                    break;
                }
                start = it++;
                if (it == end) {
                    throw xml_parse_error("Invalid tag", start);
                }

                switch (*it) {
                case '?':
                    skip_question_mark_tag(it, start, end);
                    break;
                case '!':
                    skip_exclamation_mark_tag(it, start, end);
                    break;
                case '/':
                    read_close_tag(builder, it, start, end);
                    break;
                default:
                    read_tag(builder, it, start, end);
                    break;
                }
            }

            return builder.release();
        }

        void read_tag(
            xml_tree_builder& builder,
            string_iterator& it,
            string_iterator start,
            string_iterator end)
        {
            assert(it == start + 1 && it != end);
            quickbook::string_view name = read_tag_name(it, start, end);
            xml_element* node = xml_element::node(name);
            builder.add_element(node);

            // Read attributes
            while (true) {
                read_some_of(it, end, " \t\n\r");
                if (it == end) {
                    throw xml_parse_error("Invalid tag", start);
                }
                if (*it == '>') {
                    ++it;
                    builder.start_children();
                    break;
                }
                if (*it == '/') {
                    ++it;
                    read_some_of(it, end, " \t\n\r");
                    if (it == end || *it != '>') {
                        throw xml_parse_error("Invalid tag", start);
                    }
                    ++it;
                    break;
                }
                quickbook::string_view attribute_name =
                    read_tag_name(it, start, end);
                read_some_of(it, end, " \t\n\r");
                if (it == end) {
                    throw xml_parse_error("Invalid tag", start);
                }
                quickbook::string_view attribute_value;
                if (*it == '=') {
                    ++it;
                    attribute_value = read_attribute_value(it, start, end);
                }
                node->set_attribute(
                    attribute_name,
                    quickbook::detail::decode_string(attribute_value));
            }
        }

        void read_close_tag(
            xml_tree_builder& builder,
            string_iterator& it,
            string_iterator start,
            string_iterator end)
        {
            assert(it == start + 1 && it != end && *it == '/');
            ++it;
            quickbook::string_view name = read_tag_name(it, start, end);
            read_some_of(it, end, " \t\n\r");
            if (it == end || *it != '>') {
                throw xml_parse_error("Invalid close tag", start);
            }
            ++it;

            if (!builder.parent() || builder.parent()->name_ != name) {
                throw xml_parse_error("Close tag doesn't match", start);
            }

            builder.end_children();
        }

        void skip_question_mark_tag(
            string_iterator& it, string_iterator start, string_iterator end)
        {
            assert(it == start + 1 && it != end && *it == '?');
            ++it;

            while (true) {
                read_to_one_of(it, end, "\"'?<>");
                if (it == end) {
                    throw xml_parse_error("Invalid tag", start);
                }
                switch (*it) {
                case '"':
                case '\'':
                    read_string(it, end);
                    break;
                case '?':
                    if (read(it, end, "?>")) {
                        return;
                    }
                    else {
                        ++it;
                    }
                    break;
                default:
                    throw xml_parse_error("Invalid tag", start);
                }
            }
        }

        void skip_exclamation_mark_tag(
            string_iterator& it, string_iterator start, string_iterator end)
        {
            assert(it == start + 1 && it != end && *it == '!');
            ++it;

            if (read(it, end, "--")) {
                if (read_past(it, end, "-->")) {
                    return;
                }
                else {
                    throw xml_parse_error("Invalid comment", start);
                }
            }

            while (true) {
                read_to_one_of(it, end, "\"'<>");
                if (it == end) {
                    throw xml_parse_error("Invalid tag", start);
                }
                switch (*it) {
                case '"':
                case '\'':
                    read_string(it, end);
                    break;
                case '>':
                    ++it;
                    return;
                default:
                    throw xml_parse_error("Invalid tag", start);
                }
            }
        }

        quickbook::string_view read_tag_name(
            string_iterator& it, string_iterator start, string_iterator end)
        {
            read_some_of(it, end, " \t\n\r");
            string_iterator name_start = it;
            read_some_of(
                it, end,
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ:-");
            if (name_start == it) {
                throw xml_parse_error("Invalid tag", start);
            }
            return quickbook::string_view(name_start, it - name_start);
        }

        quickbook::string_view read_attribute_value(
            string_iterator& it, string_iterator start, string_iterator end)
        {
            read_some_of(it, end, " \t\n\r");
            if (*it == '"' || *it == '\'') {
                return read_string(it, end);
            }
            else {
                throw xml_parse_error("Invalid tag", start);
            }
        }

        quickbook::string_view read_string(
            string_iterator& it, string_iterator end)
        {
            assert(it != end && (*it == '"' || *it == '\''));

            string_iterator start = it;
            char deliminator = *it;
            ++it;
            read_to(it, end, deliminator);
            if (it == end) {
                throw xml_parse_error("Invalid string", start);
            }
            ++it;
            return quickbook::string_view(start + 1, it - start - 2);
        }
    }
}
