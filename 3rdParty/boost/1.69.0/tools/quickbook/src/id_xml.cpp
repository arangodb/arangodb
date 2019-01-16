/*=============================================================================
    Copyright (c) 2011-2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/sort.hpp>
#include "document_state_impl.hpp"
#include "simple_parse.hpp"
#include "utils.hpp"

namespace quickbook
{
    namespace
    {
        char const* id_attributes_[] = {"id", "linkend", "linkends",
                                        "arearefs"};
    }

    xml_processor::xml_processor()
    {
        static std::size_t const n_id_attributes =
            sizeof(id_attributes_) / sizeof(char const*);
        for (int i = 0; i != n_id_attributes; ++i) {
            id_attributes.push_back(id_attributes_[i]);
        }

        boost::sort(id_attributes);
    }

    void xml_processor::parse(quickbook::string_view source, callback& c)
    {
        typedef string_iterator iterator;

        c.start(source);

        iterator it = source.begin(), end = source.end();

        for (;;) {
            read_past(it, end, "<");
            if (it == end) break;

            if (read(it, end, "!--quickbook-escape-prefix-->")) {
                read_past(it, end, "<!--quickbook-escape-postfix-->");
                continue;
            }

            switch (*it) {
            case '?':
                ++it;
                read_past(it, end, "?>");
                break;

            case '!':
                if (read(it, end, "!--"))
                    read_past(it, end, "-->");
                else
                    read_past(it, end, ">");
                break;

            default:
                if ((*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z') ||
                    *it == '_' || *it == ':') {
                    read_to_one_of(it, end, " \t\n\r>");

                    for (;;) {
                        read_some_of(it, end, " \t\n\r");
                        iterator name_start = it;
                        read_to_one_of(it, end, "= \t\n\r>");
                        if (it == end || *it == '>') break;
                        quickbook::string_view name(
                            name_start, it - name_start);
                        ++it;

                        read_some_of(it, end, "= \t\n\r");
                        if (it == end || (*it != '"' && *it != '\'')) break;

                        char delim = *it;
                        ++it;

                        iterator value_start = it;

                        it = std::find(it, end, delim);
                        if (it == end) break;
                        quickbook::string_view value(
                            value_start, it - value_start);
                        ++it;

                        if (boost::find(id_attributes, name.to_s()) !=
                            id_attributes.end()) {
                            c.id_value(value);
                        }
                    }
                }
                else {
                    read_past(it, end, ">");
                }
            }
        }

        c.finish(source);
    }

    namespace detail
    {
        std::string linkify(
            quickbook::string_view source, quickbook::string_view linkend)
        {
            typedef string_iterator iterator;

            iterator it = source.begin(), end = source.end();

            bool contains_link = false;

            for (; !contains_link;) {
                read_past(it, end, "<");
                if (it == end) break;

                switch (*it) {
                case '?':
                    ++it;
                    read_past(it, end, "?>");
                    break;

                case '!':
                    if (read(it, end, "!--")) {
                        read_past(it, end, "-->");
                    }
                    else {
                        read_past(it, end, ">");
                    }
                    break;

                default:
                    if ((*it >= 'a' && *it <= 'z') ||
                        (*it >= 'A' && *it <= 'Z') || *it == '_' ||
                        *it == ':') {
                        iterator tag_name_start = it;
                        read_to_one_of(it, end, " \t\n\r>");
                        quickbook::string_view tag_name(
                            tag_name_start, it - tag_name_start);
                        if (tag_name == "link") {
                            contains_link = true;
                        }

                        for (;;) {
                            read_to_one_of(it, end, "\"'\n\r>");
                            if (it == end || *it == '>') break;
                            if (*it == '"' || *it == '\'') {
                                char delim = *it;
                                ++it;
                                it = std::find(it, end, delim);
                                if (it == end) break;
                                ++it;
                            }
                        }
                    }
                    else {
                        read_past(it, end, ">");
                    }
                }
            }

            std::string result;

            if (!contains_link) {
                result += "<link linkend=\"";
                result.append(linkend.begin(), linkend.end());
                result += "\">";
                result.append(source.begin(), source.end());
                result += "</link>";
            }
            else {
                result.append(source.begin(), source.end());
            }

            return result;
        }
    }
}
