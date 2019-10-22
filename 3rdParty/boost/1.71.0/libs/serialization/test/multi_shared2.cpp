/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// multi_shared2.cpp: library simply using extended_type_info_typeid

// (C) Copyright 2018 Alexander Grund
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/archive/text_oarchive.hpp>
#include <iostream>

struct X2{
  template<class Archive>
  void serialize(Archive &, const unsigned int){}
};

BOOST_CLASS_IMPLEMENTATION(X2, boost::serialization::object_class_info)

BOOST_SYMBOL_EXPORT bool g(){
  boost::archive::text_oarchive(std::cout) & X2();
  return true;
}
