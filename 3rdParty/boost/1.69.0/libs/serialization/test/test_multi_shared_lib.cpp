/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_multi_shared_lib.cpp: test that implementation of extented_type_info
//		works when using multiple shared libraries
//
// This reproduces a crash that occurred when multiple shared libraries were
// using Boost.Serialization built statically. That causes core singletons to be
// instantiated in each shared library separately. Due to some destruction order
// mixup in the context of shared libraries on linux it is possible, that
// singletons accessed in destructors of other singletons are already destructed.
// Accessing them will then lead to a crash or memory corruption.
// For this we need 2 shared libraries, linked against static boost. They need to
// instantiate extended_type_info_typeid with different types, either by serializing
// 2 types (which will do that internally) or by accessing the singletons directly.

// (C) Copyright 2018 Alexander Grund
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/serialization/config.hpp>

// Both shall instantiate different(!) singletons and return true
BOOST_SYMBOL_IMPORT bool f();
BOOST_SYMBOL_IMPORT bool g();

int main(int argc, char**){
  if(f() && g())
  	return 0;
  return 1;
}
