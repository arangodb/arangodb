#ifndef META_HS_EXCEPT_KEYWORDS_HPP
#define META_HS_EXCEPT_KEYWORDS_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/accept_when.hpp>

#include <boost/mpl/equal.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/not.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/apply_wrap.hpp>

struct keywords_are_not_allowed {};

template <class P, class Keywords>
class except_keywords
{
private:
  template <class T>
  struct not_a_keyword
  {
    typedef not_a_keyword type;

    template <class Acc, class Keyword>
    struct apply :
      boost::mpl::and_<
        Acc,
        boost::mpl::not_<typename boost::mpl::equal<T, Keyword>::type>
      >
    {};
  };

  struct not_keyword
  {
    typedef not_keyword type;
  
    template <class T>
    struct apply :
      boost::mpl::fold<Keywords, boost::mpl::true_, not_a_keyword<T> >
    {};
  };
public:
  typedef except_keywords type;

  template <class S, class Pos>
  struct apply :
    boost::mpl::apply_wrap2<
      boost::metaparse::accept_when<P, not_keyword, keywords_are_not_allowed>,
      S,
      Pos
    >
  {};
};

#endif

