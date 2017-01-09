#ifndef META_HS_AST_HPP
#define META_HS_AST_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

namespace ast
{
  template <class V>
  struct value
  {
    typedef value type;
  };
  
  template <class Name>
  struct ref
  {
    typedef ref type;
  };
  
  template <class F, class Arg>
  struct application
  {
    typedef application type;
  };
  
  template <class F, class ArgName>
  struct lambda
  {
    typedef lambda type;
  };
  
  template <class E>
  struct top_bound
  {
    typedef top_bound type;
  };
}

#endif

