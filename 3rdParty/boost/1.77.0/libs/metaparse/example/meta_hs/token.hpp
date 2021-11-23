#ifndef META_HS_TOKEN_HPP
#define META_HS_TOKEN_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <ast.hpp>
#include <except_keywords.hpp>

#include <boost/metaparse/string.hpp>
#include <boost/metaparse/token.hpp>
#include <boost/metaparse/always_c.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/one_of.hpp>
#include <boost/metaparse/last_of.hpp>
#include <boost/metaparse/return_.hpp>
#include <boost/metaparse/int_.hpp>
#include <boost/metaparse/foldl_reject_incomplete_start_with_parser.hpp>
#include <boost/metaparse/alphanum.hpp>
#include <boost/metaparse/transform.hpp>
#include <boost/metaparse/letter.hpp>
#include <boost/metaparse/keyword.hpp>
#include <boost/metaparse/optional.hpp>

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/push_back.hpp>
#include <boost/mpl/vector.hpp>

namespace token
{
  typedef
    boost::metaparse::token<
      boost::metaparse::always_c<'+',boost::metaparse::string<'.','+','.'> >
    >
    plus;
  
  typedef
    boost::metaparse::token<
      boost::metaparse::always_c<'-',boost::metaparse::string<'.','-','.'> >
    >
    minus;
  
  typedef
    boost::metaparse::token<
      boost::metaparse::always_c<'*',boost::metaparse::string<'.','*','.'> >
    >
    mult;
  
  typedef
    boost::metaparse::token<
      boost::metaparse::always_c<'/',boost::metaparse::string<'.','/','.'> >
    >
    div;
  
  typedef
    boost::metaparse::token<
      boost::metaparse::one_of<
        boost::metaparse::last_of<
          boost::metaparse::lit_c<'='>,
          boost::metaparse::lit_c<'='>,
          boost::metaparse::return_<
            boost::metaparse::string<'.','=','=','.'>
          >
        >,
        boost::metaparse::last_of<
          boost::metaparse::lit_c<'/'>,
          boost::metaparse::lit_c<'='>,
          boost::metaparse::return_<
            boost::metaparse::string<'.','/','=','.'>
          >
        >,
        boost::metaparse::last_of<
          boost::metaparse::lit_c<'<'>,
          boost::metaparse::one_of<
            boost::metaparse::always_c<
              '=',
              boost::metaparse::string<'.','<','=','.'>
            >,
            boost::metaparse::return_<
              boost::metaparse::string<'.','<','.'>
            >
          >
        >,
        boost::metaparse::last_of<
          boost::metaparse::lit_c<'>'>,
          boost::metaparse::optional<
            boost::metaparse::always_c<
              '=',
              boost::metaparse::string<'.','>','=','.'>
            >,
            boost::metaparse::string<'.','>','.'>
          >
        >
      >
    >
    cmp;
  
  typedef
    boost::metaparse::token<boost::metaparse::lit_c<'('> >
    open_bracket;
  
  typedef
    boost::metaparse::token<boost::metaparse::lit_c<')'> >
    close_bracket;
  
  typedef
    boost::metaparse::token<boost::metaparse::lit_c<'='> >
    define;
  
  typedef boost::metaparse::token<boost::metaparse::int_> int_;
  
  typedef
    boost::metaparse::token<
      except_keywords<
        boost::metaparse::foldl_reject_incomplete_start_with_parser<
          boost::metaparse::one_of<
            boost::metaparse::alphanum,
            boost::metaparse::lit_c<'_'>
          >,
          boost::metaparse::transform<
            boost::metaparse::one_of<
              boost::metaparse::letter,
              boost::metaparse::lit_c<'_'>
            >,
            boost::mpl::lambda<
              boost::mpl::push_back<
                boost::metaparse::string<>,
                boost::mpl::_1
              >
            >::type
          >,
          boost::mpl::lambda<
            boost::mpl::push_back<boost::mpl::_1, boost::mpl::_2>
          >::type
        >,
        boost::mpl::vector<
          boost::metaparse::string<'i','f'>,
          boost::metaparse::string<'t','h','e','n'>,
          boost::metaparse::string<'e','l','s','e'>
        >
      >
    >
    name;
  
  typedef
    boost::metaparse::token<
      boost::metaparse::keyword<boost::metaparse::string<'i','f'> >
    >
    if_;
  
  typedef
    boost::metaparse::token<
      boost::metaparse::keyword<boost::metaparse::string<'t','h','e','n'> >
    >
    then;
  
  typedef
    boost::metaparse::token<
      boost::metaparse::keyword<boost::metaparse::string<'e','l','s','e'> >
    >
    else_;
}

#endif


