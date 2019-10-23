// Copyright 2016 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DLL_MY_CPP_PLUGIN_API_HPP
#define BOOST_DLL_MY_CPP_PLUGIN_API_HPP

//[cppplug
#include <string>

namespace space {

class BOOST_SYMBOL_EXPORT my_plugin 
{
    std::string _name;
public:
   std::string name() const;
   float  calculate(float x, float y);
   int    calculate(int, x,  int y);
   static std::size_t size();
   my_plugin(const std::string & name);
   my_plugin();
   ~my_plugin_api();
   static int value;
};

}
//]

std::string space::my_plugin_api::name() const {return _name;}
float  space::my_plugin::calculate(float x, float y) {return x/y;}
int    space::my_plugin::calculate(int, x, int y)    {return x/y;}
std::size_t my_plugin::size() {return sizeof(my_plugin);}
space::my_plugin::my_plugin(const std::string & name) : _name(name) {}
space::my_plugin::my_plugin() : _name("Empty") {}
space::my_plugin::~my_plugin_api() {}
int space::my_plugin::value = 42;

   
#endif // BOOST_DLL_MY_PLUGIN_API_HPP

