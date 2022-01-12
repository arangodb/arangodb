/*=============================================================================
    Copyright (c) 2001-2003 Joel de Guzman
    Copyright (c) 2003 Hartmut Kaiser
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <iostream>


//#define BOOST_SPIRIT_DEBUG
#define BOOST_SPIRIT_SINGLE_GRAMMAR_INSTANCE
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_grammar_def.hpp>

#include <boost/core/lightweight_test.hpp>

using namespace BOOST_SPIRIT_CLASSIC_NS;

//////////////////////////////////////////////////////////////////////////////
//
//  Grammar tests
//
///////////////////////////////////////////////////////////////////////////////
struct num_list : public grammar<num_list>
{
    enum {
        default_rule = 0,
        num_rule = 1
    };

    template <typename ScannerT>
    struct definition
    :   public grammar_def<rule<ScannerT>, same>
    {
        definition(num_list const& /*self*/)
        {
            num = int_p;
            r = num >> *(',' >> num);

            this->start_parsers(r, num);

            BOOST_SPIRIT_DEBUG_RULE(num);
            BOOST_SPIRIT_DEBUG_RULE(r);
        }

        rule<ScannerT> r, num;

        rule<ScannerT> const& start() const { return r; }
    };
};

struct num_list_ex : public grammar<num_list_ex>
{
    enum {
        default_rule = 0,
        num_rule = 1,
        integer_rule = 2
    };

    template <typename ScannerT>
    struct definition
    :   public grammar_def<rule<ScannerT>, same, int_parser<int, 10, 1, -1> >
    {
        definition(num_list_ex const& /*self*/)
        {
            num = integer;
            r = num >> *(',' >> num);

            this->start_parsers(r, num, integer);

            BOOST_SPIRIT_DEBUG_RULE(num);
            BOOST_SPIRIT_DEBUG_RULE(r);
        }

        rule<ScannerT> r, num;
        int_parser<int, 10, 1, -1> integer;

        rule<ScannerT> const& start() const { return r; }
    };
};

void
grammar_tests()
{
    num_list nlist;
    BOOST_SPIRIT_DEBUG_GRAMMAR(nlist);

    parse_info<char const*> pi;
    pi = parse("123, 456, 789", nlist, space_p);
    BOOST_TEST(pi.hit);
    BOOST_TEST(pi.full);

    num_list_ex nlistex;
    BOOST_SPIRIT_DEBUG_GRAMMAR(nlistex);

    pi = parse("123, 456, 789", nlist.use_parser<num_list::default_rule>(),
        space_p);
    BOOST_TEST(pi.hit);
    BOOST_TEST(pi.full);

    pi = parse("123", nlist.use_parser<num_list::num_rule>(), space_p);
    BOOST_TEST(pi.hit);
    BOOST_TEST(pi.full);

    pi = parse("123, 456, 789", nlistex, space_p);
    BOOST_TEST(pi.hit);
    BOOST_TEST(pi.full);

    pi = parse("123, 456, 789",
        nlistex.use_parser<num_list_ex::default_rule>(), space_p);
    BOOST_TEST(pi.hit);
    BOOST_TEST(pi.full);

    pi = parse("123", nlistex.use_parser<num_list_ex::num_rule>(), space_p);
    BOOST_TEST(pi.hit);
    BOOST_TEST(pi.full);

    pi = parse("123", nlistex.use_parser<num_list_ex::integer_rule>(),
        space_p);
    BOOST_TEST(pi.hit);
    BOOST_TEST(pi.full);
}

///////////////////////////////////////////////////////////////////////////////
//
//  Main
//
///////////////////////////////////////////////////////////////////////////////
int
main()
{
    grammar_tests();
    return boost::report_errors();
}

