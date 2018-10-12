///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#ifndef BOOST_REGEX_PERFRMANCE_HPP
#define BOOST_REGEX_PERFRMANCE_HPP

#include <string>
#include <boost/shared_ptr.hpp>

struct abstract_regex
{
   virtual bool set_expression(const char*, bool isperl) = 0;
   virtual bool match_test(const char* text) = 0;
   virtual unsigned find_all(const char* text) = 0;
   virtual std::string name() = 0;
   static void register_instance(boost::shared_ptr<abstract_regex> item);
};

void report_execution_time(double t, std::string table, std::string row, std::string heading);
std::string boost_name();
std::string compiler_name();
std::string platform_name();


#endif


