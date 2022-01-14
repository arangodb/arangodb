/*=============================================================================
    Copyright (c) 2001-2007 Hartmut Kaiser
    Copyright (c) 2020 Nikita Kniazev
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/spirit/include/classic_core.hpp> 
#include <boost/spirit/include/classic_ast.hpp> 
#include <boost/spirit/include/classic_tree_to_xml.hpp> 

#include <iostream>
#include <iterator>
#include <fstream>
#include <ostream>
#include <string>

using namespace BOOST_SPIRIT_CLASSIC_NS; 

///////////////////////////////////////////////////////////////////////////////
struct calculator : public grammar<calculator>
{
    static const int integerID = 1;
    static const int factorID = 2;
    static const int termID = 3;
    static const int expressionID = 4;

    template <typename ScannerT>
    struct definition
    {
        definition(calculator const& /*self*/)
        {
            //  Start grammar definition
            integer     =   leaf_node_d[ lexeme_d[
                                (!ch_p('-') >> +digit_p)
                            ] ];

            factor      =   integer
                        |   inner_node_d[ch_p('(') >> expression >> ch_p(')')]
                        |   (root_node_d[ch_p('-')] >> factor);

            term        =   factor >>
                            *(  (root_node_d[ch_p('*')] >> factor)
                              | (root_node_d[ch_p('/')] >> factor)
                            );

            expression  =   term >>
                            *(  (root_node_d[ch_p('+')] >> term)
                              | (root_node_d[ch_p('-')] >> term)
                            );
            //  End grammar definition

            // turn on the debugging info.
            BOOST_SPIRIT_DEBUG_RULE(integer);
            BOOST_SPIRIT_DEBUG_RULE(factor);
            BOOST_SPIRIT_DEBUG_RULE(term);
            BOOST_SPIRIT_DEBUG_RULE(expression);
        }

        rule<ScannerT, parser_context<>, parser_tag<expressionID> >   expression;
        rule<ScannerT, parser_context<>, parser_tag<termID> >         term;
        rule<ScannerT, parser_context<>, parser_tag<factorID> >       factor;
        rule<ScannerT, parser_context<>, parser_tag<integerID> >      integer;

        rule<ScannerT, parser_context<>, parser_tag<expressionID> > const&
        start() const { return expression; }
    };
};

///////////////////////////////////////////////////////////////////////////////
/// a streambuf implementation that sinks characters to output iterator
template <typename OutputIterator, typename Char>
struct psbuf : std::basic_streambuf<Char>
{
    template <typename T>
    psbuf(T& sink) : sink_(sink) {}

    // silence MSVC warning C4512: assignment operator could not be generated
    BOOST_DELETED_FUNCTION(psbuf& operator=(psbuf const&))

protected:
    typename psbuf::int_type overflow(typename psbuf::int_type ch) BOOST_OVERRIDE
    {
        if (psbuf::traits_type::eq_int_type(ch, psbuf::traits_type::eof()))
            return psbuf::traits_type::not_eof(ch);

        *sink_ = psbuf::traits_type::to_char_type(ch);
        ++sink_;
        return ch;
    }

private:
    OutputIterator sink_;
};

///////////////////////////////////////////////////////////////////////////////
#define EXPECTED_XML_OUTPUT "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n\
<!DOCTYPE parsetree SYSTEM \"parsetree.dtd\">\n\
<!-- 1+2 -->\n\
<parsetree version=\"1.0\">\n\
    <parsenode>\n\
        <value>+</value>\n\
        <parsenode>\n\
            <value>1</value>\n\
        </parsenode>\n\
        <parsenode>\n\
            <value>2</value>\n\
        </parsenode>\n\
    </parsenode>\n\
</parsetree>\n"

#define EXPECTED_XML_OUTPUT_WIDE BOOST_PP_CAT(L, EXPECTED_XML_OUTPUT)

bool test(wchar_t const *text)
{
    typedef std::basic_string<wchar_t>::iterator iterator_t; 

    std::basic_string<wchar_t> input(text); 
    calculator calc; 
    tree_parse_info<iterator_t> ast_info = 
        ast_parse(iterator_t(input.begin()), iterator_t(input.end()), 
            calc >> end_p, space_p); 

    std::basic_string<wchar_t> out;
    {
        psbuf<std::back_insert_iterator<std::wstring>, wchar_t> buf(out);
        std::wostream outsink(&buf);
        basic_tree_to_xml<wchar_t>(outsink, ast_info.trees, input); 
    }
    return out == EXPECTED_XML_OUTPUT_WIDE;
} 

bool test(char const *text)
{
    typedef std::string::iterator iterator_t; 

    std::string input(text); 
    calculator calc; 
    tree_parse_info<iterator_t> ast_info = 
        ast_parse(iterator_t(input.begin()), iterator_t(input.end()), 
            calc >> end_p, space_p); 

    std::string out;
    {
        psbuf<std::back_insert_iterator<std::string>, char> buf(out);
        std::ostream outsink(&buf);
        basic_tree_to_xml<char>(outsink, ast_info.trees, input); 
    }
    return out == EXPECTED_XML_OUTPUT;
} 

int main() 
{ 
    BOOST_TEST(test("1+2"));
    if (std::has_facet<std::ctype<wchar_t> >(std::locale()))
    {
        BOOST_TEST(test(L"1+2"));
    }
    return boost::report_errors();
}
