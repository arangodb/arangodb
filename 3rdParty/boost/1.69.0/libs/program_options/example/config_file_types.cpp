// Copyright Thomas Kent 2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// This example shows a config file (in ini format) being parsed by the
// program_options library. It includes a numebr of different value types.

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <assert.h>
#include <iostream>
#include <sstream>
using namespace std;

const double FLOAT_SEPERATION = 0.00000000001;
bool check_float(double test, double expected)
{
   double seperation = expected * (1 + FLOAT_SEPERATION) / expected;
   if ((test < expected + seperation) && (test > expected - seperation))
   {
      return true;
   }
   return false;
}

stringstream make_file()
{
   stringstream ss;
   ss << "# This file checks parsing of various types of config values\n";
   //FAILS: ss << "; a windows style comment\n";

   ss << "global_string = global value\n";
   ss << "unregistered_entry = unregistered value\n";

   ss << "\n[strings]\n";
   ss << "word = word\n";
   ss << "phrase = this is a phrase\n";
   ss << "quoted = \"quotes are in result\"\n";
   
   ss << "\n[ints]\n";
   ss << "positive = 41\n";
   ss << "negative = -42\n";
   //FAILS: Lexical cast doesn't support hex, oct, or bin
   //ss << "hex = 0x43\n";
   //ss << "oct = 044\n";
   //ss << "bin = 0b101010\n";

   ss << "\n[floats]\n";
   ss << "positive = 51.1\n";
   ss << "negative = -52.1\n";
   ss << "double = 53.1234567890\n";
   ss << "int = 54\n";
   ss << "int_dot = 55.\n";
   ss << "dot = .56\n";
   ss << "exp_lower = 57.1e5\n";
   ss << "exp_upper = 58.1E5\n";
   ss << "exp_decimal = .591e5\n";
   ss << "exp_negative = 60.1e-5\n";
   ss << "exp_negative_val = -61.1e5\n";
   ss << "exp_negative_negative_val = -62.1e-5\n";

   ss << "\n[booleans]\n";
   ss << "number_true = 1\n";
   ss << "number_false = 0\n";
   ss << "yn_true = yes\n";
   ss << "yn_false = no\n";
   ss << "tf_true = true\n";
   ss << "tf_false = false\n";
   ss << "onoff_true = on\n";
   ss << "onoff_false = off\n";
   ss << "present_equal_true = \n";
   //FAILS: Must be an = 
   //ss << "present_no_equal_true\n";

   ss.seekp(ios_base::beg);
   return ss;
}

po::options_description set_options()
{
   po::options_description opts;
   opts.add_options()
      ("global_string", po::value<string>())

      ("strings.word", po::value<string>())
      ("strings.phrase", po::value<string>())
      ("strings.quoted", po::value<string>())

      ("ints.positive", po::value<int>())
      ("ints.negative", po::value<int>())
      ("ints.hex", po::value<int>())
      ("ints.oct", po::value<int>())
      ("ints.bin", po::value<int>())

      ("floats.positive", po::value<float>())
      ("floats.negative", po::value<float>())
      ("floats.double", po::value<double>())
      ("floats.int", po::value<float>())
      ("floats.int_dot", po::value<float>())
      ("floats.dot", po::value<float>())
      ("floats.exp_lower", po::value<float>())
      ("floats.exp_upper", po::value<float>())
      ("floats.exp_decimal", po::value<float>())
      ("floats.exp_negative", po::value<float>())
      ("floats.exp_negative_val", po::value<float>())
      ("floats.exp_negative_negative_val", po::value<float>())

      // Load booleans as value<bool>, so they will require a --option=value on the command line
      //("booleans.number_true", po::value<bool>())
      //("booleans.number_false", po::value<bool>())
      //("booleans.yn_true", po::value<bool>())
      //("booleans.yn_false", po::value<bool>())
      //("booleans.tf_true", po::value<bool>())
      //("booleans.tf_false", po::value<bool>())
      //("booleans.onoff_true", po::value<bool>())
      //("booleans.onoff_false", po::value<bool>())
      //("booleans.present_equal_true", po::value<bool>())
      //("booleans.present_no_equal_true", po::value<bool>())

      // Load booleans as bool_switch, so that a --option will set it true on the command line
      // The difference between these two types does not show up when parsing a file
      ("booleans.number_true", po::bool_switch())
      ("booleans.number_false", po::bool_switch())
      ("booleans.yn_true", po::bool_switch())
      ("booleans.yn_false", po::bool_switch())
      ("booleans.tf_true", po::bool_switch())
      ("booleans.tf_false", po::bool_switch())
      ("booleans.onoff_true", po::bool_switch())
      ("booleans.onoff_false", po::bool_switch())
      ("booleans.present_equal_true", po::bool_switch())
      ("booleans.present_no_equal_true", po::bool_switch())
      ;
   return opts;
}

vector<string> parse_file(stringstream &file, po::options_description &opts, po::variables_map &vm)
{
   const bool ALLOW_UNREGISTERED = true;
   cout << file.str() << endl;

   po::parsed_options parsed = parse_config_file(file, opts, ALLOW_UNREGISTERED);
   store(parsed, vm);
   vector<string> unregistered = po::collect_unrecognized(parsed.options, po::exclude_positional);
   notify(vm);

   return unregistered;
}

void check_results(po::variables_map &vm, vector<string> unregistered)
{
   // Check that we got the correct values back
   string expected_global_string = "global value";

   string expected_unreg_option = "unregistered_entry";
   string expected_unreg_value = "unregistered value";

   string expected_strings_word = "word";
   string expected_strings_phrase = "this is a phrase";
   string expected_strings_quoted = "\"quotes are in result\"";

   int expected_int_postitive = 41;
   int expected_int_negative = -42;
   int expected_int_hex = 0x43;
   int expected_int_oct = 044;
   int expected_int_bin = 0b101010;

   float expected_float_positive = 51.1f;
   float expected_float_negative = -52.1f;
   double expected_float_double = 53.1234567890;
   float expected_float_int = 54.0f;
   float expected_float_int_dot = 55.0f;
   float expected_float_dot = .56f;
   float expected_float_exp_lower = 57.1e5f;
   float expected_float_exp_upper = 58.1E5f;
   float expected_float_exp_decimal = .591e5f;
   float expected_float_exp_negative = 60.1e-5f;
   float expected_float_exp_negative_val = -61.1e5f;
   float expected_float_exp_negative_negative_val = -62.1e-5f;

   bool expected_number_true = true;
   bool expected_number_false = false;
   bool expected_yn_true = true;
   bool expected_yn_false = false;
   bool expected_tf_true = true;
   bool expected_tf_false = false;
   bool expected_onoff_true = true;
   bool expected_onoff_false = false;
   bool expected_present_equal_true = true;
   bool expected_present_no_equal_true = true;

   assert(vm["global_string"].as<string>() == expected_global_string);

   assert(unregistered[0] == expected_unreg_option);
   assert(unregistered[1] == expected_unreg_value);

   assert(vm["strings.word"].as<string>() == expected_strings_word);
   assert(vm["strings.phrase"].as<string>() == expected_strings_phrase);
   assert(vm["strings.quoted"].as<string>() == expected_strings_quoted);

   assert(vm["ints.positive"].as<int>() == expected_int_postitive);
   assert(vm["ints.negative"].as<int>() == expected_int_negative);
   //assert(vm["ints.hex"].as<int>() == expected_int_hex);
   //assert(vm["ints.oct"].as<int>() == expected_int_oct);
   //assert(vm["ints.bin"].as<int>() == expected_int_bin);

   assert(check_float(vm["floats.positive"].as<float>(), expected_float_positive));
   assert(check_float(vm["floats.negative"].as<float>(), expected_float_negative));
   assert(check_float(vm["floats.double"].as<double>(), expected_float_double));
   assert(check_float(vm["floats.int"].as<float>(), expected_float_int));
   assert(check_float(vm["floats.int_dot"].as<float>(), expected_float_int_dot));
   assert(check_float(vm["floats.dot"].as<float>(), expected_float_dot));
   assert(check_float(vm["floats.exp_lower"].as<float>(), expected_float_exp_lower));
   assert(check_float(vm["floats.exp_upper"].as<float>(), expected_float_exp_upper));
   assert(check_float(vm["floats.exp_decimal"].as<float>(), expected_float_exp_decimal));
   assert(check_float(vm["floats.exp_negative"].as<float>(), expected_float_exp_negative));
   assert(check_float(vm["floats.exp_negative_val"].as<float>(), expected_float_exp_negative_val));
   assert(check_float(vm["floats.exp_negative_negative_val"].as<float>(), expected_float_exp_negative_negative_val));

   assert(vm["booleans.number_true"].as<bool>() == expected_number_true);
   assert(vm["booleans.number_false"].as<bool>() == expected_number_false);
   assert(vm["booleans.yn_true"].as<bool>() == expected_yn_true);
   assert(vm["booleans.yn_false"].as<bool>() == expected_yn_false);
   assert(vm["booleans.tf_true"].as<bool>() == expected_tf_true);
   assert(vm["booleans.tf_false"].as<bool>() == expected_tf_false);
   assert(vm["booleans.onoff_true"].as<bool>() == expected_onoff_true);
   assert(vm["booleans.onoff_false"].as<bool>() == expected_onoff_false);
   assert(vm["booleans.present_equal_true"].as<bool>() == expected_present_equal_true);
   //assert(vm["booleans.present_no_equal_true"].as<bool>() == expected_present_no_equal_true);
}

int main(int ac, char* av[])
{
   auto file = make_file();
   auto opts = set_options();
   po::variables_map vars;
   auto unregistered = parse_file(file, opts, vars);
   check_results(vars, unregistered);   

   return 0;
}
