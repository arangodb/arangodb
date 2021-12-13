/*=============================================================================
    Copyright (c) 2001-2011 Hartmut Kaiser
    Copyright (c) 2001-2011 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "lexer.hpp"

namespace client { namespace lexer
{
    template <typename BaseIterator>
    conjure_tokens<BaseIterator>::conjure_tokens()
      : identifier("[a-zA-Z_][a-zA-Z_0-9]*", token_ids::identifier)
      , lit_uint("[0-9]+", token_ids::lit_uint)
      , true_or_false("true|false", token_ids::true_or_false)
      , add(*this)
    {
        lex::_pass_type _pass;

        this->self = lit_uint | true_or_false;

        this->add
                ("void")
                ("int")
                ("if")
                ("else")
                ("while")
                ("return")
                ("=",       token_ids::assign)
                ("\\+=",    token_ids::plus_assign)
                ("\\-=",    token_ids::minus_assign)
                ("\\*=",    token_ids::times_assign)
                ("\\/=",    token_ids::divide_assign)
                ("%=",      token_ids::mod_assign)
                ("\\&=",    token_ids::bit_and_assign)
                ("\\^=",    token_ids::bit_xor_assign)
                ("\\|=",    token_ids::bit_or_assign)
                ("<<=",     token_ids::shift_left_assign)
                (">>=",     token_ids::shift_right_assign)
                ("\\|\\|",  token_ids::logical_or)
                ("&&",      token_ids::logical_and)
                ("\\|",     token_ids::bit_or)
                ("\\^",     token_ids::bit_xor)
                ("&",       token_ids::bit_and)
                ("<<",      token_ids::shift_left)
                (">>",      token_ids::shift_right)
                ("==",      token_ids::equal)
                ("!=",      token_ids::not_equal)
                ("<",       token_ids::less)
                ("<=",      token_ids::less_equal)
                (">",       token_ids::greater)
                (">=",      token_ids::greater_equal)
                ("\\+",     token_ids::plus)
                ("\\-",     token_ids::minus)
                ("\\*",     token_ids::times)
                ("\\/",     token_ids::divide)
                ("%",       token_ids::mod)
                ("\\+\\+",  token_ids::plus_plus)
                ("\\-\\-",  token_ids::minus_minus)
                ("~",       token_ids::compl_)
                ("!",       token_ids::not_)
            ;

        this->self += lex::char_('(') | ')' | '{' | '}' | ',' | ';';

        this->self +=
                identifier
            |   lex::string("(\\/\\*[^*]*\\*+([^/*][^*]*\\*+)*\\/)|(\\/\\/[^\r\n]*)", token_ids::comment)
                [
                    lex::_pass = lex::pass_flags::pass_ignore
                ]
            |   lex::string("[ \t\n\r]+", token_ids::whitespace)
                [
                    lex::_pass = lex::pass_flags::pass_ignore
                ]
            ;
    }

    template <typename BaseIterator>
    bool conjure_tokens<BaseIterator>::add_(
        std::string const& keyword, int id_)
    {
        // add the token to the lexer
        token_ids::type id;
        if (id_ == token_ids::invalid)
            id = token_ids::type(this->get_next_id());
        else
            id = token_ids::type(id_);

        this->self.add(keyword, id);
        // store the mapping for later retrieval
        std::pair<typename keyword_map_type::iterator, bool> p =
            keywords_.insert(typename keyword_map_type::value_type(keyword, id));

        return p.second;
    }
}}

