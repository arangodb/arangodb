/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    Copyright (c) 2005 Thomas Guest
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include "state.hpp"
#include "document_state.hpp"
#include "for.hpp"
#include "grammar.hpp"
#include "path.hpp"
#include "phrase_tags.hpp"
#include "quickbook.hpp"
#include "state_save.hpp"
#include "utils.hpp"

#if (defined(BOOST_MSVC) && (BOOST_MSVC <= 1310))
#pragma warning(disable : 4355)
#endif

namespace quickbook
{
    char const* quickbook_get_date = "__quickbook_get_date__";
    char const* quickbook_get_time = "__quickbook_get_time__";

    unsigned qbk_version_n = 0; // qbk_major_version * 100 + qbk_minor_version

    state::state(
        fs::path const& filein_,
        fs::path const& xinclude_base_,
        string_stream& out_,
        document_state& document_)
        : grammar_()

        , order_pos(0)
        , xinclude_base(xinclude_base_)

        , templates()
        , error_count(0)
        , anchors()
        , warned_about_breaks(false)
        , conditional(true)
        , document(document_)
        , callouts()
        , callout_depth(0)
        , dependencies()
        , explicit_list(false)
        , strict_mode(false)

        , imported(false)
        , macro()
        , source_mode()
        , source_mode_next()
        , source_mode_next_pos()
        , current_file(0)
        , current_path(filein_, 0, filein_.filename())

        , template_depth(0)
        , min_section_level(1)

        , in_list(false)
        , in_list_save()
        , out(out_)
        , phrase()

        , values(&current_file)
    {
        // add the predefined macros
        macro.add("__DATE__", std::string(quickbook_get_date))(
            "__TIME__",
            std::string(quickbook_get_time))("__FILENAME__", std::string());
        update_filename_macro();

        boost::scoped_ptr<quickbook_grammar> g(new quickbook_grammar(*this));
        grammar_.swap(g);
    }

    quickbook_grammar& state::grammar() const { return *grammar_; }

    void state::update_filename_macro()
    {
        *boost::spirit::classic::find(macro, "__FILENAME__") =
            detail::encode_string(
                detail::path_to_generic(current_path.abstract_file_path));
    }

    unsigned state::get_new_order_pos() { return ++order_pos; }

    void state::push_output()
    {
        out.push();
        phrase.push();
        in_list_save.push(in_list);
    }

    void state::pop_output()
    {
        phrase.pop();
        out.pop();
        in_list = in_list_save.top();
        in_list_save.pop();
    }

    source_mode_info state::tagged_source_mode() const
    {
        source_mode_info result;

        QUICKBOOK_FOR (source_mode_info const& s, tagged_source_mode_stack) {
            result.update(s);
        }

        return result;
    }

    source_mode_info state::current_source_mode() const
    {
        source_mode_info result = source_mode;

        result.update(document.section_source_mode());

        QUICKBOOK_FOR (source_mode_info const& s, tagged_source_mode_stack) {
            result.update(s);
        }

        return result;
    }

    void state::change_source_mode(source_mode_type s)
    {
        source_mode = source_mode_info(s, get_new_order_pos());
    }

    void state::push_tagged_source_mode(source_mode_type s)
    {
        tagged_source_mode_stack.push_back(
            source_mode_info(s, s ? get_new_order_pos() : 0));
    }

    void state::pop_tagged_source_mode()
    {
        assert(!tagged_source_mode_stack.empty());
        tagged_source_mode_stack.pop_back();
    }

    state_save::state_save(quickbook::state& state_, scope_flags scope_)
        : state(state_)
        , scope(scope_)
        , qbk_version(qbk_version_n)
        , imported(state.imported)
        , current_file(state.current_file)
        , current_path(state.current_path)
        , xinclude_base(state.xinclude_base)
        , source_mode(state.source_mode)
        , macro()
        , template_depth(state.template_depth)
        , min_section_level(state.min_section_level)
    {
        if (scope & scope_macros) macro = state.macro;
        if (scope & scope_templates) state.templates.push();
        if (scope & scope_output) {
            state.push_output();
        }
        state.values.builder.save();
    }

    state_save::~state_save()
    {
        state.values.builder.restore();
        boost::swap(qbk_version_n, qbk_version);
        boost::swap(state.imported, imported);
        boost::swap(state.current_file, current_file);
        boost::swap(state.current_path, current_path);
        boost::swap(state.xinclude_base, xinclude_base);
        boost::swap(state.source_mode, source_mode);
        if (scope & scope_output) {
            state.pop_output();
        }
        if (scope & scope_templates) state.templates.pop();
        if (scope & scope_macros) state.macro = macro;
        boost::swap(state.template_depth, template_depth);
        boost::swap(state.min_section_level, min_section_level);
    }
}
