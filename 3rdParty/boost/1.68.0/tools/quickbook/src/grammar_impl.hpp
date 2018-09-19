/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2010 Daniel James
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_QUICKBOOK_GRAMMARS_IMPL_HPP)
#define BOOST_SPIRIT_QUICKBOOK_GRAMMARS_IMPL_HPP

#include <boost/spirit/include/classic_symbols.hpp>
#include "cleanup.hpp"
#include "grammar.hpp"
#include "values.hpp"

namespace quickbook
{
    namespace cl = boost::spirit::classic;

    // Information about a square bracket element (e.g. [* word]), and
    // some other syntactic elements (such as lists and horizontal rules)..
    struct element_info
    {
        // Types of elements.
        //
        // Used to determine:
        //
        //  - where they can be used.
        //  - whether they end a paragraph
        //  - how following newlines are interpreted by the grammar.
        //  - and possibly other things.....
        enum type_enum
        {
            // Used when there's no element.
            nothing = 0,

            // A section tag. These can't be nested.
            section_block = 1,

            // Block elements that can be used in conditional phrases and lists,
            // but not nested.
            conditional_or_block = 2,

            // Block elements that can be nested in other elements.
            nested_block = 4,

            // Phrase elements.
            phrase = 8,

            // Depending on the context this can be a block or phrase.
            //
            // Currently this is only used for elements that don't actually
            // generate output (e.g. anchors, source mode tags). The main
            // reason is so that lists can be preceeded by the element, e.g.
            //
            // [#anchor]
            // * list item.
            //
            // If the anchor was considered to be a phrase element, then the
            // list wouldn't be recognised.
            maybe_block = 16
        };

        // Masks to determine which context elements can be used in (in_*), and
        // whether they are consided to be a block element (is_*).
        enum context
        {
            // At the top level we allow everything.
            in_top_level = phrase | maybe_block | nested_block |
                           conditional_or_block | section_block,

            // In conditional phrases and list blocks we everything but section
            // elements.
            in_conditional =
                phrase | maybe_block | nested_block | conditional_or_block,
            in_list_block =
                phrase | maybe_block | nested_block | conditional_or_block,

            // In nested blocks we allow a more limited range of elements.
            in_nested_block = phrase | maybe_block | nested_block,

            // In a phrase we only allow phrase elements, ('maybe_block'
            // elements are treated as phrase elements in this context)
            in_phrase = phrase | maybe_block,

            // At the start of a block these are all block elements.
            is_contextual_block = maybe_block | nested_block |
                                  conditional_or_block | section_block,

            // These are all block elements in all other contexts.
            is_block = nested_block | conditional_or_block | section_block
        };

        element_info() : type(nothing), rule(), tag(0) {}

        element_info(
            type_enum t,
            cl::rule<scanner>* r,
            value::tag_type tag_ = value::default_tag,
            unsigned int v = 0)
            : type(t), rule(r), tag(tag_), qbk_version(v)
        {
        }

        type_enum type;
        cl::rule<scanner>* rule;
        value::tag_type tag;
        unsigned int qbk_version;
    };

    struct quickbook_grammar::impl
    {
        quickbook::state& state;
        cleanup cleanup_;

        // Main Grammar
        cl::rule<scanner> block_start;
        cl::rule<scanner> phrase_start;
        cl::rule<scanner> nested_phrase;
        cl::rule<scanner> inline_phrase;
        cl::rule<scanner> paragraph_phrase;
        cl::rule<scanner> extended_phrase;
        cl::rule<scanner> table_title_phrase;
        cl::rule<scanner> inside_preformatted;
        cl::rule<scanner> inside_paragraph;
        cl::rule<scanner> command_line;
        cl::rule<scanner> attribute_template_body;
        cl::rule<scanner> attribute_value_1_7;
        cl::rule<scanner> escape;
        cl::rule<scanner> raw_escape;
        cl::rule<scanner> skip_entity;

        // Miscellaneous stuff
        cl::rule<scanner> hard_space; // Either non-empty space, or
                                      // empty and not followed by
                                      // alphanumeric/_. Use to match the
                                      // the end of an itendifier.
        cl::rule<scanner> space; // Space/tab/newline/comment (possibly empty)
        cl::rule<scanner> blank; // Space/tab/comment (possibly empty)
        cl::rule<scanner> eol;   // blank >> eol
        cl::rule<scanner> phrase_end; // End of phrase text, context sensitive
        cl::rule<scanner> comment;
        cl::rule<scanner> line_comment;
        cl::rule<scanner> macro_identifier;

        // Element Symbols
        cl::symbols<element_info> elements;

        // Source mode
        cl::symbols<source_mode_type> source_modes;

        // Doc Info
        cl::rule<scanner> doc_info_details;

        impl(quickbook::state&);

      private:
        void init_main();
        void init_block_elements();
        void init_phrase_elements();
        void init_doc_info();
    };
}

#endif // BOOST_SPIRIT_QUICKBOOK_GRAMMARS_HPP
