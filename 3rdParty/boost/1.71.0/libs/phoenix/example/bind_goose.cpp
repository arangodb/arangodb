/*=============================================================================
For Boost Bind:
    Copyright (c) 2001-2004 Peter Dimov and Multi Media Ltd.
    Copyright (c) 2001 David Abrahams
    Copyright (c) 2005 Peter Dimov
For Boost Phoenix:
    Copyright (c) 2001-2010 Joel de Guzman
    Copyright (c) 2010 Thomas Heller
For the example:
    Copyright (c) 2011 Paul Heil
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
// bind_goose.cpp
// This example is based on code by Paul Heil to be found here:
// http://www.codeproject.com/Tips/248492/How-does-boost-phoenix-improve-boost-bind
//
// Show different ways of using boost bind and phoenix to handle deletion.
//


#include <iostream>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/bind.hpp>
#include <boost/phoenix/operator/comparison.hpp>
#include <boost/phoenix/stl/algorithm/transformation.hpp>
#include <functional>
#include <string>
#include <vector>

////////////////////////////////////////////
// Set up the list here
////////////////////////////////////////////
std::vector< std::string > make_list() {
  std::vector< std::string > list;
  list.push_back( "duck" );
  list.push_back( "duck" );
  list.push_back( "goose" );
  return list;
}
//////////////////////////////////////////////
// First example using standard library only
//////////////////////////////////////////////
bool IsGoose( const std::string& s )
{
  return s == "goose";
}

void delete_value1(std::vector< std::string > &list )
{
  list.erase( std::remove_if( list.begin(), list.end(), IsGoose ), list.end() );
}

void out_string(const std::string  &s)
{
  std::cout << s << std::endl;
}

void show_list1( const std::vector< std::string > &list )
{
  std::for_each(list.begin(), list.end(), out_string);
}

//////////////////////////////////////////////
// Second example using boost bind
//////////////////////////////////////////////

bool isValue(const std::string &s1, const std::string &s2)
{
  return s1==s2;
}

void delete_value2(std::vector< std::string > &list, const std::string & value)
{
  list.erase(
    std::remove_if(
        list.begin(),
        list.end(),
        boost::bind(
            isValue, // &isValue works as well.
            _1, // Boost.Bind placeholder
            boost::cref( value ) ) ),
    list.end() );
}

///////////////////////////////////////////////////////
// Third example using boost phoenix for the comparison
///////////////////////////////////////////////////////

namespace phx = boost::phoenix;
using phx::placeholders::arg1;
using phx::placeholders::arg2;

void delete_value3(std::vector< std::string > &list, const std::string & value)
{
  list.erase( std::remove_if(
        list.begin(),
        list.end(),
        // This needs header boost/phoenix/operator/comparison.
        // arg1 is a Boost.Phoenix placeholder.
        arg1 == phx::cref( value ) ),
        list.end() );
}

//////////////////////////////////////////////////////////////
// Third example using boost phoenix for the algorithm as well
//////////////////////////////////////////////////////////////

void delete_value4(std::vector< std::string > &list, const std::string & value)
{
  // This need header boost/phoenix/stl/algorithm/transformation
  list.erase( phx::remove_if( arg1, arg2 )
            ( list, arg1 == phx::cref( value ) ),
            list.end() );
}

int main() {
  std::cout << "--------------------------------" << std::endl;
  std::cout << "Delete the goose examples." << std::endl;
  std::cout << "--------------------------------" << std::endl;
  std::string value = "goose";

  std::vector< std::string > list1 = make_list();
  delete_value1(list1);
  show_list1(list1);
  std::cout << "--------------------------------" << std::endl;
  std::vector< std::string > list2 = make_list();
  delete_value2(list2,value);
  show_list1(list2);
  std::cout << "--------------------------------" << std::endl;
  std::vector< std::string > list3 = make_list();
  delete_value3(list3,value);
  show_list1(list3);
  std::cout << "--------------------------------" << std::endl;
  std::vector< std::string > list4 = make_list();
  delete_value4(list4,value);
  show_list1(list4);
  std::cout << "--------------------------------" << std::endl;
  return 0;
}
