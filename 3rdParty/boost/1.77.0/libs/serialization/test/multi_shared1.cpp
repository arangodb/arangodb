/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_multi_shared_lib.cpp: test that implementation of extented_type_info
//		works when using multiple shared libraries

// (C) Copyright 2018 Alexander Grund
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/archive/text_oarchive.hpp>
#include <iostream>

struct X1{
  template<class Archive>
  void serialize(Archive &, const unsigned int){}
};

BOOST_CLASS_IMPLEMENTATION(X1, boost::serialization::object_class_info)

BOOST_SYMBOL_EXPORT bool f(){
  boost::archive::text_oarchive(std::cout) & X1();
  return true;
}
