/* Boost.Flyweight test template for serialization capabilities.
 *
 * Copyright 2006-2014 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/flyweight for library home page.
 */

#ifndef BOOST_FLYWEIGHT_TEST_SERIALIZATION_TEMPLATE_HPP
#define BOOST_FLYWEIGHT_TEST_SERIALIZATION_TEMPLATE_HPP

#if defined(_MSC_VER)&&(_MSC_VER>=1200)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/flyweight/key_value.hpp>
#include <boost/flyweight/serialize.hpp>
#include <boost/functional/hash.hpp>
#include <boost/mpl/apply.hpp>
#include <boost/serialization/vector.hpp>
#include <string>
#include <sstream>
#include <vector>
#include "heavy_objects.hpp"

#define LENGTHOF(array) (sizeof(array)/sizeof((array)[0]))

struct tracked_string
{
  typedef tracked_string type;

  tracked_string(){}
  tracked_string(const char* str_):str(str_){}

  const std::string& get()const{return str;}

  friend bool operator==(const type& x,const type& y){return x.str==y.str;}
  friend bool operator< (const type& x,const type& y){return x.str< y.str;}
  friend bool operator!=(const type& x,const type& y){return x.str!=y.str;}
  friend bool operator> (const type& x,const type& y){return x.str> y.str;}
  friend bool operator>=(const type& x,const type& y){return x.str>=y.str;}
  friend bool operator<=(const type& x,const type& y){return x.str<=y.str;}

private:
  friend class boost::serialization::access;
  
  template<class Archive>
  void serialize(Archive& ar,const unsigned int){ar&str;}

  std::string str;
};

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
namespace boost{
#endif

inline std::size_t hash_value(const tracked_string& x)
{
  boost::hash<std::string> h;
  return h(x.get());
}

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
} /* namespace boost */
#endif

template<typename Flyweight,typename ForwardIterator>
void test_serialization_template(
  ForwardIterator first,ForwardIterator last
  BOOST_APPEND_EXPLICIT_TEMPLATE_TYPE(Flyweight))
{
  std::vector<Flyweight> v1;
  while(first!=last)v1.push_back(Flyweight(*first++));
  std::ostringstream oss;
  {
    const std::vector<Flyweight>& crv1=v1;
    boost::archive::text_oarchive oa(oss);
    oa<<crv1;
  }

  std::vector<Flyweight> v2;
  {
    std::istringstream iss(oss.str());
    boost::archive::text_iarchive ia(iss);
    ia>>v2;
  }

  BOOST_TEST(v1==v2);
}

template<typename FlyweightSpecifier>
void test_serialization_template(
  BOOST_EXPLICIT_TEMPLATE_TYPE(FlyweightSpecifier))
{
  typedef typename boost::mpl::apply1<
    FlyweightSpecifier,std::string
  >::type string_flyweight;

  typedef typename boost::mpl::apply1<
    FlyweightSpecifier,tracked_string
  >::type tracked_string_flyweight;

  typedef typename boost::mpl::apply1<
    FlyweightSpecifier,
    boost::flyweights::key_value<std::string,texture,from_texture_to_string>
  >::type texture_flyweight;

  const char* words[]={"hello","boost","flyweight","boost","bye","c++","c++"};
  test_serialization_template<string_flyweight>(
    &words[0],&words[0]+LENGTHOF(words));
  test_serialization_template<tracked_string_flyweight>(
    &words[0],&words[0]+LENGTHOF(words));

  const char* textures[]={
    "wood","grass","sand","granite","terracotta","wood","sand","grass"};
  test_serialization_template<texture_flyweight>(
    &textures[0],&textures[0]+LENGTHOF(textures));
}

#undef LENGTHOF

#endif
