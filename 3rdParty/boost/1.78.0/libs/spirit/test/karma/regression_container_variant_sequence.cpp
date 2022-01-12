//  Copyright (c) 2001-2012 Hartmut Kaiser
//  Copyright (c) 2012 Benjamin Schindler
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/spirit/include/karma.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <set>

namespace generator
{
    struct Enum
    {
        std::string enumName;
        std::vector<std::string> enumEntries;
    };
    typedef boost::variant<std::string, Enum> VariantType;

    namespace karma = boost::spirit::karma;

    // Our grammar definition
    template<typename Iterator>
    struct SettingsHeaderGenerator: karma::grammar<Iterator, VariantType()>
    {
        SettingsHeaderGenerator() : SettingsHeaderGenerator::base_type(baseRule)
        {
            using karma::lit;
            using karma::string;

            enumRule = lit("enum ") << string << lit("\n{\n") << string % ",\n" << "}";
            declarationRule = lit("class ") << string << ';';
            baseRule = (declarationRule | enumRule) << lit("\n");

            baseRule.name("base");
            enumRule.name("enum");
            declarationRule.name("declaration");
        }

        karma::rule<Iterator, std::string()> declarationRule;
        karma::rule<Iterator, Enum()> enumRule;
        karma::rule<Iterator, VariantType()> baseRule;
    };

    template <typename OutputIterator>
    bool generate_header(OutputIterator& sink, VariantType& data)
    {
        SettingsHeaderGenerator<OutputIterator> generator;
        return karma::generate(sink, generator, data);
    }
}

BOOST_FUSION_ADAPT_STRUCT(generator::Enum,
    (std::string, enumName)
    (std::vector<std::string>, enumEntries)
)

int main()
{
    generator::VariantType variant = "bla";
    std::string generated;
    std::back_insert_iterator<std::string> sink(generated);
    BOOST_TEST(generator::generate_header(sink, variant));
    BOOST_TEST(generated == "class bla;\n");

    return boost::report_errors();
}

