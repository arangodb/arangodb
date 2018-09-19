/*=============================================================================
Copyright (c) 2017 Daniel James

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_BOOSTBOOK_CHUNKER_HPP)
#define BOOST_QUICKBOOK_BOOSTBOOK_CHUNKER_HPP

#include "xml_parse.hpp"

namespace quickbook
{
    namespace detail
    {
        struct chunk : tree_node<chunk>
        {
            xml_tree contents_;
            xml_tree title_;
            xml_tree info_;
            bool inline_;
            std::string id_;
            std::string path_;

            explicit chunk(xml_tree&& contents)
                : contents_(std::move(contents)), inline_(false)
            {
            }
        };

        typedef tree<chunk> chunk_tree;

        chunk_tree chunk_document(xml_tree&);
        void inline_sections(chunk*, int depth);
        void inline_all(chunk*);
    }
}

#endif
