/*=============================================================================
Copyright (c) 2017 Daniel James

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "boostbook_chunker.hpp"
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/unordered_set.hpp>

namespace quickbook
{
    namespace detail
    {
        boost::unordered_set<std::string> chunk_types;
        boost::unordered_set<std::string> chunkinfo_types;

        static struct init_chunk_type
        {
            init_chunk_type()
            {
                chunk_types.insert("book");
                chunk_types.insert("article");
                chunk_types.insert("library");
                chunk_types.insert("chapter");
                chunk_types.insert("part");
                chunk_types.insert("appendix");
                chunk_types.insert("preface");
                chunk_types.insert("qandadiv");
                chunk_types.insert("qandaset");
                chunk_types.insert("reference");
                chunk_types.insert("set");
                chunk_types.insert("section");

                for (boost::unordered_set<std::string>::const_iterator it =
                         chunk_types.begin();
                     it != chunk_types.end(); ++it) {
                    chunkinfo_types.insert(*it + "info");
                }
            }
        } init_chunk;

        struct chunk_builder : tree_builder<chunk>
        {
            int count;

            chunk_builder() : count(0) {}

            std::string next_path_name()
            {
                ++count;
                std::string result = "page-";
                result += boost::lexical_cast<std::string>(count);
                ++count;
                return result;
            }
        };

        void chunk_nodes(
            chunk_builder& builder, xml_tree& tree, xml_element* node);
        std::string id_to_path(quickbook::string_view);
        void inline_chunks(chunk*);

        chunk_tree chunk_document(xml_tree& tree)
        {
            chunk_builder builder;
            for (xml_element* it = tree.root(); it;) {
                xml_element* next = it->next();
                chunk_nodes(builder, tree, it);
                it = next;
            }

            return builder.release();
        }

        void inline_sections(chunk* c, int depth)
        {
            if (c->contents_.root()->name_ == "section" && depth > 1) {
                --depth;
            }

            // When depth is 0, inline leading sections.
            chunk* it = c->children();
            if (depth == 0) {
                for (; it && it->contents_.root()->name_ == "section";
                     it = it->next()) {
                    inline_chunks(it);
                }
            }

            for (; it; it = it->next()) {
                inline_sections(it, depth);
            }
        }

        void inline_all(chunk* c)
        {
            for (chunk* it = c->children(); it; it = it->next()) {
                inline_chunks(it);
            }
        }

        void inline_chunks(chunk* c)
        {
            c->inline_ = true;
            c->path_ = c->parent()->path_;
            for (chunk* it = c->children(); it; it = it->next()) {
                inline_chunks(it);
            }
        }

        void chunk_nodes(
            chunk_builder& builder, xml_tree& tree, xml_element* node)
        {
            chunk* parent = builder.parent();

            if (parent && node->type_ == xml_element::element_node &&
                node->name_ == "title") {
                parent->title_ = tree.extract(node);
            }
            else if (
                parent && node->type_ == xml_element::element_node &&
                chunkinfo_types.find(node->name_) != chunkinfo_types.end()) {
                parent->info_ = tree.extract(node);
            }
            else if (
                node->type_ == xml_element::element_node &&
                chunk_types.find(node->name_) != chunk_types.end()) {
                chunk* chunk_node = new chunk(tree.extract(node));
                builder.add_element(chunk_node);

                chunk_node->id_ = node->has_attribute("id")
                                      ? node->get_attribute("id").to_s()
                                      : builder.next_path_name();
                chunk_node->path_ = id_to_path(chunk_node->id_);

                builder.start_children();
                for (xml_element* it = node->children(); it;) {
                    xml_element* next = it->next();
                    chunk_nodes(builder, tree, it);
                    it = next;
                }
                builder.end_children();
            }
        }

        std::string id_to_path(quickbook::string_view id)
        {
            std::string result(id.begin(), id.end());
            boost::replace_all(result, ".", "/");
            result += ".html";
            return result;
        }
    }
}
