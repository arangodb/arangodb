/*=============================================================================
Copyright (c) 2016 Frank Hein, maxence business consulting gmbh

Distributed under the Boost Software License, Version 1.0. (See accompanying
file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <iostream>
#include <map>

#include <boost/spirit/home/qi.hpp>
#include <boost/spirit/home/qi/nonterminal/grammar.hpp>
#include <boost/spirit/include/phoenix.hpp>

namespace qi = boost::spirit::qi;

typedef std::string::const_iterator iterator_type;
typedef std::string result_type;

template<typename Parser>
void parse(const std::string message, const std::string& input, const std::string& rule, const Parser& parser) {
    iterator_type iter = input.begin(), end = input.end();

    std::vector<result_type> parsed_result;

    std::cout << "-------------------------\n";
    std::cout << message << "\n";
    std::cout << "Rule: " << rule << std::endl;
    std::cout << "Parsing: \"" << input << "\"\n";

    bool result = qi::phrase_parse(iter, end, parser, qi::space, parsed_result);
    if (result)
    {
        std::cout << "Parser succeeded.\n";
        std::cout << "Parsed " << parsed_result.size() << " elements:";
        for (const auto& str : parsed_result)
            std::cout << "[" << str << "]";
        std::cout << std::endl;
    }
    else
    {
        std::cout << "Parser failed" << std::endl;
    }
    if (iter == end) {
        std::cout << "EOI reached." << std::endl;
    }
    else {
        std::cout << "EOI not reached. Unparsed: \"" << std::string(iter, end) << "\"" << std::endl;
    }
    std::cout << "-------------------------\n";

}

namespace grammars {
    namespace phx = boost::phoenix;

    template <typename Iterator>
    struct ident : qi::grammar < Iterator, std::string(), qi::space_type>
    {
        ident();

        qi::rule <iterator_type, std::string(), qi::space_type>
            id, id_list, qualified_id;
    };

    template <typename Iterator>
    ident<Iterator>::ident() : ident::base_type(id_list) {

        using qi::on_error;
        using qi::fail;
        using qi::expect;

        id = (qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_'));

        id_list = expect[id >> qi::lit(';')];

        on_error<fail>(id_list,
            phx::ref(std::cout)
            << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl
            << "Error! Expecting "
            << qi::_4
            << " here: "
            << phx::construct<std::string>(qi::_3, qi::_2) << std::endl
            << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl
            );
    }
}

int main() {

    grammars::ident<iterator_type> id;

    parse("expect directive, fail on first"
        , "1234; id2;"
        , "qi::expect[ id  >> qi::lit(';') ]"
        , id);

    parse("expect directive, fail on second"
        , "id1, id2"
        , "qi::expect[ id  >> qi::lit(';') ]"
        , id);

    parse("expect directive, success"
        , "id1;"
        , "qi::expect[ id  >> qi::lit(';') ]"
        , id);

    return 0;
}
