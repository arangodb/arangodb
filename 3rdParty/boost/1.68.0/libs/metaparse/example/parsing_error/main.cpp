// Copyright Abel Sinkovics (abel@sinkovics.hu)  2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/repeated.hpp>
#include <boost/metaparse/sequence.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/debug_parsing_error.hpp>

#include <boost/metaparse/build_parser.hpp>
#include <boost/metaparse/string.hpp>

#include <boost/mpl/apply.hpp>

using boost::metaparse::sequence;
using boost::metaparse::lit_c;
using boost::metaparse::repeated;
using boost::metaparse::build_parser;
using boost::metaparse::debug_parsing_error;

using boost::mpl::apply;

/*
 * The grammar
 *
 * s ::= a*b
 */
typedef sequence<repeated<lit_c<'a'> >, lit_c<'b'> > s;

typedef build_parser<s> test_parser;

#if BOOST_METAPARSE_STD < 2011

typedef boost::metaparse::string<'a','a','a','c'> invalid_input;

#else

typedef BOOST_METAPARSE_STRING("aaac") invalid_input;

#endif

debug_parsing_error<test_parser, invalid_input> debug;

int main()
{
  // This causes an error
  // apply<test_parser, invalid_input>::type();
}


