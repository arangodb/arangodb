/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_ACTIONS_CLASS_HPP)
#define BOOST_SPIRIT_ACTIONS_CLASS_HPP

#include <map>
#include <boost/scoped_ptr.hpp>
#include "collector.hpp"
#include "dependency_tracker.hpp"
#include "include_paths.hpp"
#include "parsers.hpp"
#include "symbols.hpp"
#include "syntax_highlight.hpp"
#include "template_stack.hpp"
#include "values_parse.hpp"

namespace quickbook
{
    namespace cl = boost::spirit::classic;
    namespace fs = boost::filesystem;

    struct state
    {
        state(
            fs::path const& filein_,
            fs::path const& xinclude_base,
            string_stream& out_,
            document_state&);

      private:
        boost::scoped_ptr<quickbook_grammar> grammar_;

      public:
        ///////////////////////////////////////////////////////////////////////////
        // State
        ///////////////////////////////////////////////////////////////////////////

        typedef std::vector<std::string> string_list;

        static int const max_template_depth = 100;

        // global state
        unsigned order_pos;
        fs::path xinclude_base;
        template_stack templates;
        int error_count;
        string_list anchors;
        bool warned_about_breaks;
        bool conditional;
        document_state& document;
        value_builder callouts; // callouts are global as
        int callout_depth;      // they don't nest.
        dependency_tracker dependencies;
        bool explicit_list; // set when using a list
        bool strict_mode;

        // state saved for files and templates.
        bool imported;
        string_symbols macro;
        source_mode_info source_mode;
        source_mode_type source_mode_next;
        value source_mode_next_pos;
        std::vector<source_mode_info> tagged_source_mode_stack;
        file_ptr current_file;
        quickbook_path current_path;

        // state saved for templates.
        int template_depth;
        int min_section_level;

        // output state - scoped by templates and grammar
        bool in_list;                  // generating a list
        std::stack<bool> in_list_save; // save the in_list state
        collector out;                 // main output stream
        collector phrase;              // phrase output stream

        // values state - scoped by everything.
        value_parser values; // parsed values

        quickbook_grammar& grammar() const;

        ///////////////////////////////////////////////////////////////////////////
        // actions
        ///////////////////////////////////////////////////////////////////////////

        void update_filename_macro();

        unsigned get_new_order_pos();

        void push_output();
        void pop_output();

        void start_list(char mark);
        void end_list(char mark);
        void start_list_item();
        void end_list_item();

        void start_callouts();
        std::string add_callout(value);
        std::string end_callouts();

        source_mode_info current_source_mode() const;
        source_mode_info tagged_source_mode() const;
        void change_source_mode(source_mode_type);
        void push_tagged_source_mode(source_mode_type);
        void pop_tagged_source_mode();
    };

    extern unsigned
        qbk_version_n; // qbk_major_version * 100 + qbk_minor_version
    extern char const* quickbook_get_date;
    extern char const* quickbook_get_time;
}

#endif // BOOST_SPIRIT_ACTIONS_CLASS_HPP
