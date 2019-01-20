/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_QUICKBOOK_ACTIONS_HPP)
#define BOOST_SPIRIT_QUICKBOOK_ACTIONS_HPP

#include <string>
#include <vector>
#include <boost/spirit/include/classic_parser.hpp>
#include "fwd.hpp"
#include "iterator.hpp"
#include "scoped.hpp"
#include "utils.hpp"
#include "values.hpp"

namespace quickbook
{
    namespace cl = boost::spirit::classic;

    // Match if quickbook version is within range
    struct quickbook_range : cl::parser<quickbook_range>
    {
        explicit quickbook_range(unsigned lower_, unsigned upper_)
            : lower(lower_), upper(upper_)
        {
        }

        bool in_range() const;

        template <typename ScannerT>
        typename cl::parser_result<quickbook_range, ScannerT>::type parse(
            ScannerT const& scan) const
        {
            return in_range() ? scan.empty_match() : scan.no_match();
        }

        unsigned lower, upper;
    };

    inline quickbook_range qbk_ver(unsigned lower, unsigned upper = 999u)
    {
        return quickbook_range(lower, upper);
    }

    // Match if in strict mode.
    struct quickbook_strict : cl::parser<quickbook_strict>
    {
        explicit quickbook_strict(
            quickbook::state& state_, bool positive_ = true)
            : state(state_), positive(positive_)
        {
        }

        bool is_strict_checking() const;

        template <typename ScannerT>
        typename cl::parser_result<quickbook_range, ScannerT>::type parse(
            ScannerT const& scan) const
        {
            return is_strict_checking() == positive ? scan.empty_match()
                                                    : scan.no_match();
        }

        quickbook_strict operator~() const
        {
            return quickbook_strict(state, !positive);
        }

        quickbook::state& state;
        bool positive;
    };

    inline quickbook_strict qbk_strict(
        quickbook::state& state, unsigned lower = 999u)
    {
        return quickbook_strict(state, lower);
    }

    // Throws load_error
    int load_snippets(
        fs::path const& file,
        std::vector<template_symbol>& storage,
        std::string const& extension,
        value::tag_type load_type);

    struct error_message_action
    {
        // Prints an error message to std::cerr

        explicit error_message_action(
            quickbook::state& state_, std::string const& message_)
            : state(state_), message(message_)
        {
        }

        void operator()(parse_iterator, parse_iterator) const;

        quickbook::state& state;
        std::string message;
    };

    struct error_action
    {
        // Prints an error message to std::cerr

        explicit error_action(quickbook::state& state_) : state(state_) {}

        void operator()(parse_iterator first, parse_iterator last) const;

        error_message_action operator()(std::string const& message)
        {
            return error_message_action(state, message);
        }

        quickbook::state& state;
    };

    struct element_action
    {
        explicit element_action(quickbook::state& state_) : state(state_) {}

        void operator()(parse_iterator, parse_iterator) const;

        quickbook::state& state;
    };

    struct paragraph_action
    {
        //  implicit paragraphs
        //  doesn't output the paragraph if it's only whitespace.

        explicit paragraph_action(quickbook::state& state_) : state(state_) {}

        void operator()() const;
        void operator()(parse_iterator, parse_iterator) const { (*this)(); }

        quickbook::state& state;
    };

    struct explicit_list_action
    {
        //  implicit paragraphs
        //  doesn't output the paragraph if it's only whitespace.

        explicit explicit_list_action(quickbook::state& state_) : state(state_)
        {
        }

        void operator()() const;
        void operator()(parse_iterator, parse_iterator) const { (*this)(); }

        quickbook::state& state;
    };

    struct phrase_end_action
    {
        explicit phrase_end_action(quickbook::state& state_) : state(state_) {}

        void operator()() const;
        void operator()(parse_iterator, parse_iterator) const { (*this)(); }

        quickbook::state& state;
    };

    struct simple_phrase_action
    {
        //  Handles simple text formats

        explicit simple_phrase_action(quickbook::state& state_) : state(state_)
        {
        }

        void operator()(char) const;

        quickbook::state& state;
    };

    struct cond_phrase_push : scoped_action_base
    {
        cond_phrase_push(quickbook::state& x) : state(x) {}

        bool start();
        void cleanup();

        quickbook::state& state;
        bool saved_conditional;
        std::vector<std::string> anchors;
    };

    struct do_macro_action
    {
        // Handles macro substitutions

        explicit do_macro_action(quickbook::state& state_) : state(state_) {}

        void operator()(std::string const& str) const;
        quickbook::state& state;
    };

    struct raw_char_action
    {
        // Prints a space

        explicit raw_char_action(quickbook::state& state_) : state(state_) {}

        void operator()(char ch) const;
        void operator()(parse_iterator first, parse_iterator last) const;

        quickbook::state& state;
    };

    struct plain_char_action
    {
        // Prints a single plain char.
        // Converts '<' to "&lt;"... etc See utils.hpp

        explicit plain_char_action(quickbook::state& state_) : state(state_) {}

        void operator()(char ch) const;
        void operator()(parse_iterator first, parse_iterator last) const;

        quickbook::state& state;
    };

    struct escape_unicode_action
    {
        explicit escape_unicode_action(quickbook::state& state_) : state(state_)
        {
        }

        void operator()(parse_iterator first, parse_iterator last) const;

        quickbook::state& state;
    };

    struct break_action
    {
        explicit break_action(quickbook::state& state_) : state(state_) {}

        void operator()(parse_iterator f, parse_iterator) const;

        quickbook::state& state;
    };

    struct element_id_warning_action
    {
        explicit element_id_warning_action(quickbook::state& state_)
            : state(state_)
        {
        }

        void operator()(parse_iterator first, parse_iterator last) const;

        quickbook::state& state;
    };

    // Returns the doc_type, or an empty string if there isn't one.
    std::string pre(
        quickbook::state& state,
        parse_iterator pos,
        value include_doc_id,
        bool nested_file);
    void post(quickbook::state& state, std::string const& doc_type);

    struct to_value_scoped_action : scoped_action_base
    {
        to_value_scoped_action(quickbook::state& state_) : state(state_) {}

        bool start(value::tag_type = value::default_tag);
        void success(parse_iterator, parse_iterator);
        void cleanup();

        quickbook::state& state;
        std::vector<std::string> saved_anchors;
        value::tag_type tag;
    };

    // member_action
    //
    // Action for calling a member function taking two parse iterators.

    template <typename T> struct member_action
    {
        typedef void (T::*member_function)(parse_iterator, parse_iterator);

        T& l;
        member_function mf;

        explicit member_action(T& l_, member_function mf_) : l(l_), mf(mf_) {}

        void operator()(parse_iterator first, parse_iterator last) const
        {
            (l.*mf)(first, last);
        }
    };

    // member_action1
    //
    // Action for calling a member function taking two parse iterators and a
    // value.

    template <typename T, typename Arg1> struct member_action1
    {
        typedef void (T::*member_function)(
            parse_iterator, parse_iterator, Arg1);

        T& l;
        member_function mf;

        explicit member_action1(T& l_, member_function mf_) : l(l_), mf(mf_) {}

        struct impl
        {
            member_action1 a;
            Arg1 value;

            explicit impl(member_action1& a_, Arg1 value_)
                : a(a_), value(value_)
            {
            }

            void operator()(parse_iterator first, parse_iterator last) const
            {
                (a.l.*a.mf)(first, last, value);
            }
        };

        impl operator()(Arg1 a1) { return impl(*this, a1); }
    };

    // member_action_value
    //
    // Action for calling a unary member function.

    template <typename T, typename Value> struct member_action_value
    {
        typedef void (T::*member_function)(Value);

        T& l;
        member_function mf;

        explicit member_action_value(T& l_, member_function mf_)
            : l(l_), mf(mf_)
        {
        }

        void operator()(Value v) const { (l.*mf)(v); }
    };

    // member_action_value
    //
    // Action for calling a unary member function with a fixed value.

    template <typename T, typename Value> struct member_action_fixed_value
    {
        typedef void (T::*member_function)(Value);

        T& l;
        member_function mf;
        Value v;

        explicit member_action_fixed_value(T& l_, member_function mf_, Value v_)
            : l(l_), mf(mf_), v(v_)
        {
        }

        void operator()() const { (l.*mf)(v); }

        void operator()(parse_iterator, parse_iterator) const { (l.*mf)(v); }
    };
}

#endif // BOOST_SPIRIT_QUICKBOOK_ACTIONS_HPP
