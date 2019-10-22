// Copyright Sascha Ochsenknecht 2009.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/program_options/parsers.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/cmdline.hpp>
using namespace boost::program_options;

#include <iostream>
#include <sstream>
#include <vector>
#include <cassert>
using namespace std;

#include "minitest.hpp"


void test_ambiguous() 
{
    options_description desc; 
    desc.add_options()
        ("cfgfile,c", value<string>()->multitoken(), "the config file") 
        ("output,c", value<string>(), "the output file") 
        ("output,o", value<string>(), "the output file")
    ;

    const char* cmdline[] = {"program", "-c", "file", "-o", "anotherfile"};

    variables_map vm;
    try {
       store(parse_command_line(sizeof(cmdline)/sizeof(const char*), 
                                    const_cast<char**>(cmdline), desc), vm);
    }
    catch (ambiguous_option& e)
    {
        BOOST_CHECK_EQUAL(e.alternatives().size(), 2);
        BOOST_CHECK_EQUAL(e.get_option_name(), "-c");      
        BOOST_CHECK_EQUAL(e.alternatives()[0], "cfgfile");
        BOOST_CHECK_EQUAL(e.alternatives()[1], "output");
    }
}


void test_ambiguous_long()
{
    options_description desc;
    desc.add_options()
        ("cfgfile,c", value<string>()->multitoken(), "the config file")
        ("output,c", value<string>(), "the output file")
        ("output,o", value<string>(), "the output file")
    ;

    const char* cmdline[] = {"program", "--cfgfile", "file", "--output", "anotherfile"};

    variables_map vm;
    try {
       store(parse_command_line(sizeof(cmdline)/sizeof(const char*),
                                    const_cast<char**>(cmdline), desc), vm);
    }
    catch (ambiguous_option& e)
    {
        BOOST_CHECK_EQUAL(e.alternatives().size(), 2);
        BOOST_CHECK_EQUAL(e.get_option_name(), "--output");
        BOOST_CHECK_EQUAL(e.alternatives()[0], "output");
        BOOST_CHECK_EQUAL(e.alternatives()[1], "output");
    }
}

void test_ambiguous_multiple_long_names()
{
    options_description desc;
    desc.add_options()
        ("cfgfile,foo,c", value<string>()->multitoken(), "the config file")
        ("output,foo,o", value<string>(), "the output file")
    ;

    const char* cmdline[] = {"program", "--foo", "file"};

    variables_map vm;
    try {
       store(parse_command_line(sizeof(cmdline)/sizeof(const char*),
                                    const_cast<char**>(cmdline), desc), vm);
    }
    catch (ambiguous_option& e)
    {
        BOOST_CHECK_EQUAL(e.alternatives().size(), 2);
        BOOST_CHECK_EQUAL(e.get_option_name(), "--foo");
        BOOST_CHECK_EQUAL(e.alternatives()[0], "cfgfile");
        BOOST_CHECK_EQUAL(e.alternatives()[1], "output");
    }
}



void test_unknown_option() 
{
   options_description desc;
   desc.add_options()
        ("cfgfile,c", value<string>(), "the configfile")
      ;

   const char* cmdline[] = {"program", "-c", "file", "-f", "anotherfile"};
   
   variables_map vm;
   try {
      store(parse_command_line(sizeof(cmdline)/sizeof(const char*), 
                                    const_cast<char**>(cmdline), desc), vm);
   }
   catch (unknown_option& e)
   {
      BOOST_CHECK_EQUAL(e.get_option_name(), "-f");      
      BOOST_CHECK_EQUAL(string(e.what()), "unrecognised option '-f'");
   }
}



void test_multiple_values() 
{
   options_description desc;
   desc.add_options()
        ("cfgfile,c", value<string>()->multitoken(), "the config file")
        ("output,o", value<string>(), "the output file")
      ;

   const char* cmdline[] = { "program", "-o", "fritz", "hugo", "--cfgfile", "file", "c", "-o", "text.out" };
   
   variables_map vm;
   try {
      store(parse_command_line(sizeof(cmdline)/sizeof(const char*), 
                                    const_cast<char**>(cmdline), desc), vm);
      notify(vm);
   }
   catch (validation_error& e)
   {
      // TODO: this is currently validation_error, shouldn't it be multiple_values ???
      // 
      //   multiple_values is thrown only at one place untyped_value::xparse(),
      //    but I think this can never be reached
      //   because: untyped_value always has one value and this is filtered before reach specific
      //   validation and parsing
      //
      BOOST_CHECK_EQUAL(e.get_option_name(), "--cfgfile");      
      BOOST_CHECK_EQUAL(string(e.what()), "option '--cfgfile' only takes a single argument");
   }
}


void test_multiple_occurrences()
{
   options_description desc;
   desc.add_options()
        ("cfgfile,c", value<string>(), "the configfile")
      ;

   const char* cmdline[] = {"program", "--cfgfile", "file", "-c", "anotherfile"};

   variables_map vm;
   try {
      store(parse_command_line(sizeof(cmdline)/sizeof(const char*),
                                    const_cast<char**>(cmdline), desc), vm);
      notify(vm);
   }
   catch (multiple_occurrences& e)
   {
      BOOST_CHECK_EQUAL(e.get_option_name(), "--cfgfile");
      BOOST_CHECK_EQUAL(string(e.what()), "option '--cfgfile' cannot be specified more than once");
   }
}

void test_multiple_occurrences_with_different_names()
{
   options_description desc;
   desc.add_options()
        ("cfgfile,config-file,c", value<string>(), "the configfile")
      ;

   const char* cmdline[] = {"program", "--config-file", "file", "--cfgfile", "anotherfile"};

   variables_map vm;
   try {
      store(parse_command_line(sizeof(cmdline)/sizeof(const char*),
                                    const_cast<char**>(cmdline), desc), vm);
      notify(vm);
   }
   catch (multiple_occurrences& e)
   {
      BOOST_CHECK( (e.get_option_name() == "--cfgfile") || (e.get_option_name() == "--config-file"));
      BOOST_CHECK(
         (string(e.what()) == "option '--cfgfile' cannot be specified more than once") ||
         (string(e.what()) == "option '--config-file' cannot be specified more than once")
      );
   }
}


void test_multiple_occurrences_with_non_key_names()
{
   options_description desc;
   desc.add_options()
        ("cfgfile,config-file,c", value<string>(), "the configfile")
      ;

   const char* cmdline[] = {"program", "--config-file", "file", "-c", "anotherfile"};

   variables_map vm;
   try {
      store(parse_command_line(sizeof(cmdline)/sizeof(const char*),
                                    const_cast<char**>(cmdline), desc), vm);
      notify(vm);
   }
   catch (multiple_occurrences& e)
   {
       BOOST_CHECK_EQUAL(e.get_option_name(), "--cfgfile");
       BOOST_CHECK_EQUAL(string(e.what()), "option '--cfgfile' cannot be specified more than once");
   }
}


void test_missing_value()
{
    options_description desc;
    desc.add_options()
        ("cfgfile,c", value<string>()->multitoken(), "the config file") 
        ("output,o", value<string>(), "the output file")
    ;
    // missing value for option '-c'
    const char* cmdline[] = { "program", "-c", "-c", "output.txt"};
    
    variables_map vm;
    
    try {
      store(parse_command_line(sizeof(cmdline)/sizeof(const char*), 
                                       const_cast<char**>(cmdline), desc), vm);
      notify(vm);
   } 
   catch (invalid_command_line_syntax& e)
   {
      BOOST_CHECK_EQUAL(e.kind(), invalid_syntax::missing_parameter);
      BOOST_CHECK_EQUAL(e.tokens(), "--cfgfile");
   }
}



int main(int /*ac*/, char** /*av*/)
{
   test_ambiguous();
   test_ambiguous_long();
   test_ambiguous_multiple_long_names();
   test_unknown_option();
   test_multiple_values();
   test_multiple_occurrences();
   test_multiple_occurrences_with_different_names();
   test_multiple_occurrences_with_non_key_names();
   test_missing_value();

   return 0;
}

