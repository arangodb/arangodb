/*=============================================================================
Copyright (c) 2017 Daniel James

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_XML_PARSE_HPP)
#define BOOST_QUICKBOOK_XML_PARSE_HPP

#include <list>
#include <string>
#include "string_view.hpp"
#include "tree.hpp"

namespace quickbook
{
    namespace detail
    {
        struct xml_element;
        typedef tree<xml_element> xml_tree;
        typedef tree_builder<xml_element> xml_tree_builder;
        struct xml_parse_error;

        struct xml_element : tree_node<xml_element>
        {
            enum element_type
            {
                element_node,
                element_text,
                element_html
            } type_;
            std::string name_;

          private:
            std::list<std::pair<std::string, std::string> > attributes_;

          public:
            std::string contents_;

            explicit xml_element(element_type n) : type_(n) {}

            explicit xml_element(element_type n, quickbook::string_view name)
                : type_(n), name_(name.begin(), name.end())
            {
            }

            static xml_element* text_node(quickbook::string_view x)
            {
                xml_element* n = new xml_element(element_text);
                n->contents_.assign(x.begin(), x.end());
                return n;
            }

            static xml_element* html_node(quickbook::string_view x)
            {
                xml_element* n = new xml_element(element_html);
                n->contents_.assign(x.begin(), x.end());
                return n;
            }

            static xml_element* node(quickbook::string_view x)
            {
                return new xml_element(element_node, x);
            }

            bool has_attribute(quickbook::string_view name)
            {
                for (auto it = attributes_.begin(), end = attributes_.end();
                     it != end; ++it) {
                    if (name == it->first) {
                        return true;
                    }
                }
                return false;
            }

            string_view get_attribute(quickbook::string_view name)
            {
                for (auto it = attributes_.begin(), end = attributes_.end();
                     it != end; ++it) {
                    if (name == it->first) {
                        return it->second;
                    }
                }
                return string_view();
            }

            string_view set_attribute(
                quickbook::string_view name, quickbook::string_view value)
            {
                for (auto it = attributes_.begin(), end = attributes_.end();
                     it != end; ++it) {
                    if (name == it->first) {
                        it->second.assign(value.begin(), value.end());
                        return it->second;
                    }
                }

                attributes_.push_back(
                    std::make_pair(name.to_s(), value.to_s()));
                return attributes_.back().second;
            }

            xml_element* get_child(quickbook::string_view name)
            {
                for (auto it = children(); it; it = it->next()) {
                    if (it->type_ == element_node && it->name_ == name) {
                        return it;
                    }
                }

                return 0;
            }
        };

        struct xml_parse_error
        {
            char const* message;
            string_iterator pos;

            xml_parse_error(char const* m, string_iterator p)
                : message(m), pos(p)
            {
            }
        };

        void write_xml_tree(xml_element*);
        xml_tree xml_parse(quickbook::string_view);
    }
}

#endif
