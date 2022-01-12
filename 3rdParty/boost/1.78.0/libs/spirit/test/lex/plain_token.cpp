//  Copyright (c) 2001-2010 Hartmut Kaiser
//  Copyright (c) 2016 Jeffrey E. Trull
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/spirit/include/lex_plain_token.hpp>

#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/qi_parse.hpp>
#include <boost/spirit/include/qi_operator.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>

namespace qi = boost::spirit::qi;
namespace lex = boost::spirit::lex;

enum tokenids
{
    // left tokens
    IDLPAREN = lex::min_token_id, 
    IDLANGLE,
    IDLBRACE,
    IDLSQUARE,
    // right tokens
    IDRPAREN,
    IDRANGLE,
    IDRBRACE,
    IDRSQUARE,
    IDANY
};

template <typename Lexer>
struct delimiter_tokens : lex::lexer<Lexer>
{
    delimiter_tokens()
    {
        this->self = 
                lex::char_('(', IDLPAREN)
            |   lex::char_(')', IDRPAREN)
            |   lex::char_('<', IDLANGLE)
            |   lex::char_('>', IDRANGLE)
            |   lex::char_('{', IDLBRACE)
            |   lex::char_('}', IDRBRACE)
            |   lex::char_('[', IDLSQUARE)
            |   lex::char_(']', IDRSQUARE)
            |   lex::string(".", IDANY)
            ;
    }
};

int main()
{
    typedef lex::lexertl::token<
        std::string::iterator, boost::mpl::vector<std::string>
    > token_type;

    typedef lex::lexertl::lexer<token_type> lexer_type;
    delimiter_tokens<lexer_type> delims;

    // two test cases for the token range
    std::string angled_delimiter_str("<canvas>");

    using qi::token;
    // angle brackets
    std::string::iterator beg = angled_delimiter_str.begin();
    BOOST_TEST(lex::tokenize_and_parse(
                   beg, angled_delimiter_str.end(),
                   delims,
                   token(IDLPAREN, IDLSQUARE)
                     >> +token(IDANY)
                     >> token(IDRPAREN, IDRSQUARE)));
                                       
    std::string paren_delimiter_str("(setq foo nil)");
    beg = paren_delimiter_str.begin();
    BOOST_TEST(lex::tokenize_and_parse(
                   beg, paren_delimiter_str.end(),
                   delims,
                   token(IDLPAREN, IDLSQUARE)
                     >> +token(IDANY)
                     >> token(IDRPAREN, IDRSQUARE)));

    // reset and use a regular plain token
    beg = paren_delimiter_str.begin();
    BOOST_TEST(lex::tokenize_and_parse(
                   beg, paren_delimiter_str.end(),
                   delims,
                   token(IDLPAREN) >> +token(IDANY) >> token(IDRPAREN)));

    return boost::report_errors();
}
