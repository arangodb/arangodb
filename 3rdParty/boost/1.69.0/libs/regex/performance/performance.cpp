///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#include "performance.hpp"
#include <list>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <boost/chrono.hpp>
#include <boost/detail/lightweight_main.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

void load_file(std::string& text, const char* file)
{
   std::deque<char> temp_copy;
   std::ifstream is(file);
   if(!is.good())
   {
      std::string msg("Unable to open file: \"");
      msg.append(file);
      msg.append("\"");
      throw std::runtime_error(msg);
   }
   is.seekg(0, std::ios_base::end);
   std::istream::pos_type pos = is.tellg();
   is.seekg(0, std::ios_base::beg);
   text.erase();
   text.reserve(pos);
   std::istreambuf_iterator<char> it(is);
   std::copy(it, std::istreambuf_iterator<char>(), std::back_inserter(text));
}


typedef std::list<boost::shared_ptr<abstract_regex> > list_type;

list_type& engines()
{
   static list_type l;
   return l;
}

void abstract_regex::register_instance(boost::shared_ptr<abstract_regex> item)
{
   engines().push_back(item);
}

template <class Clock>
struct stopwatch
{
   typedef typename Clock::duration duration;
   stopwatch()
   {
      m_start = Clock::now();
   }
   duration elapsed()
   {
      return Clock::now() - m_start;
   }
   void reset()
   {
      m_start = Clock::now();
   }

private:
   typename Clock::time_point m_start;
};

unsigned sum = 0;
unsigned last_value_returned = 0;

template <class Func>
double exec_timed_test(Func f)
{
   double t = 0;
   unsigned repeats = 1;
   do {
      stopwatch<boost::chrono::high_resolution_clock> w;

      for(unsigned count = 0; count < repeats; ++count)
      {
         last_value_returned  = f();
         sum += last_value_returned;
      }

      t = boost::chrono::duration_cast<boost::chrono::duration<double>>(w.elapsed()).count();
      if(t < 0.5)
         repeats *= 2;
   } while(t < 0.5);
   return t / repeats;
}


std::string format_expression_as_quickbook(std::string s)
{
   static const boost::regex e("[`/_*=$^@#&%\\\\]");
   static const boost::regex open_b("\\[");
   static const boost::regex close_b("\\]");
   s = regex_replace(s, e, "\\\\$0");
   s = regex_replace(s, open_b, "\\\\u005B");
   s = regex_replace(s, close_b, "\\\\u005D");
   if(s.size() > 200)
   {
      s.erase(200);
      s += " ...";
   }
   return "[^" + s + "]";
}

void test_match(const char* expression, const char* text, bool isperl = false)
{
   std::string table = "Testing simple " + (isperl ? std::string("Perl") : std::string("leftmost-longest")) + " matches (platform = " + platform_name() + ", compiler = " + compiler_name() + ")";
   std::string row = format_expression_as_quickbook(expression);
   row += "[br]";
   row += format_expression_as_quickbook(text);
   for(list_type::const_iterator i = engines().begin(); i != engines().end(); ++i)
   {
      std::string heading = (*i)->name();
      if((*i)->set_expression(expression, isperl))
      {
         double time = exec_timed_test([&]() { return (*i)->match_test(text) ? 1 : 0; });
         report_execution_time(time, table, row, heading);
      }
   }
}

void test_search(const char* expression, const char* text, bool isperl = false, const char* filename = 0)
{
   std::string table = "Testing " + (isperl ? std::string("Perl") : std::string("leftmost-longest")) + " searches (platform = " + platform_name() + ", compiler = " + compiler_name() + ")";
   std::string row = format_expression_as_quickbook(expression);
   row += "[br]";
   if(filename)
   {
      row += "In file: ";
      row += filename;
   }
   else
   {
      row += format_expression_as_quickbook(text);
   }
   for(list_type::const_iterator i = engines().begin(); i != engines().end(); ++i)
   {
      std::string heading = (*i)->name();
      if((*i)->set_expression(expression, isperl))
      {
         double time = exec_timed_test([&]() { return (*i)->find_all(text); });
         report_execution_time(time, table, row, heading);
         std::cout << "Search with library: " << heading << " found " << last_value_returned << " occurances.\n";
      }
   }
}

int cpp_main(int argc, char* argv[])
{
   boost::filesystem::path here(__FILE__);
   here = here.parent_path().parent_path().parent_path().parent_path();

   boost::filesystem::path cpp_file = here / "boost";
   cpp_file /= "crc.hpp";

   // start with a simple test, this is basically a measure of the minimal overhead
   // involved in calling a regex matcher:
   test_match("abc", "abc");
   // these are from the regex docs:
   test_match("^([0-9]+)(\\-| |$)(.*)$", "100- this is a line of ftp response which contains a message string");
   test_match("([[:digit:]]{4}[- ]){3}[[:digit:]]{3,4}", "1234-5678-1234-456");
   // these are from http://www.regxlib.com/
   test_match("^([a-zA-Z0-9_\\-\\.]+)@((\\[[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.)|(([a-zA-Z0-9\\-]+\\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})(\\]?)$", "john@johnmaddock.co.uk");
   test_match("^([a-zA-Z0-9_\\-\\.]+)@((\\[[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.)|(([a-zA-Z0-9\\-]+\\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})(\\]?)$", "foo12@foo.edu");
   test_match("^([a-zA-Z0-9_\\-\\.]+)@((\\[[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.)|(([a-zA-Z0-9\\-]+\\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})(\\]?)$", "bob.smith@foo.tv");
   test_match("^[a-zA-Z]{1,2}[0-9][0-9A-Za-z]{0,1} {0,1}[0-9][A-Za-z]{2}$", "EH10 2QQ");
   test_match("^[a-zA-Z]{1,2}[0-9][0-9A-Za-z]{0,1} {0,1}[0-9][A-Za-z]{2}$", "G1 1AA");
   test_match("^[a-zA-Z]{1,2}[0-9][0-9A-Za-z]{0,1} {0,1}[0-9][A-Za-z]{2}$", "SW1 1ZZ");
   test_match("^[[:digit:]]{1,2}/[[:digit:]]{1,2}/[[:digit:]]{4}$", "4/1/2001");
   test_match("^[[:digit:]]{1,2}/[[:digit:]]{1,2}/[[:digit:]]{4}$", "12/12/2001");
   test_match("^[-+]?[[:digit:]]*\\.?[[:digit:]]*$", "123");
   test_match("^[-+]?[[:digit:]]*\\.?[[:digit:]]*$", "+3.14159");
   test_match("^[-+]?[[:digit:]]*\\.?[[:digit:]]*$", "-3.14159");

   // start with a simple test, this is basically a measure of the minimal overhead
   // involved in calling a regex matcher:
   test_match("abc", "abc", true);
   // these are from the regex docs:
   test_match("^([0-9]+)(\\-| |$)(.*)$", "100- this is a line of ftp response which contains a message string", true);
   test_match("([[:digit:]]{4}[- ]){3}[[:digit:]]{3,4}", "1234-5678-1234-456", true);
   // these are from http://www.regxlib.com/
   test_match("^([a-zA-Z0-9_\\-\\.]+)@((\\[[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.)|(([a-zA-Z0-9\\-]+\\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})(\\]?)$", "john@johnmaddock.co.uk", true);
   test_match("^([a-zA-Z0-9_\\-\\.]+)@((\\[[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.)|(([a-zA-Z0-9\\-]+\\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})(\\]?)$", "foo12@foo.edu", true);
   test_match("^([a-zA-Z0-9_\\-\\.]+)@((\\[[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.)|(([a-zA-Z0-9\\-]+\\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})(\\]?)$", "bob.smith@foo.tv", true);
   test_match("^[a-zA-Z]{1,2}[0-9][0-9A-Za-z]{0,1} {0,1}[0-9][A-Za-z]{2}$", "EH10 2QQ", true);
   test_match("^[a-zA-Z]{1,2}[0-9][0-9A-Za-z]{0,1} {0,1}[0-9][A-Za-z]{2}$", "G1 1AA", true);
   test_match("^[a-zA-Z]{1,2}[0-9][0-9A-Za-z]{0,1} {0,1}[0-9][A-Za-z]{2}$", "SW1 1ZZ", true);
   test_match("^[[:digit:]]{1,2}/[[:digit:]]{1,2}/[[:digit:]]{4}$", "4/1/2001", true);
   test_match("^[[:digit:]]{1,2}/[[:digit:]]{1,2}/[[:digit:]]{4}$", "12/12/2001", true);
   test_match("^[-+]?[[:digit:]]*\\.?[[:digit:]]*$", "123", true);
   test_match("^[-+]?[[:digit:]]*\\.?[[:digit:]]*$", "+3.14159", true);
   test_match("^[-+]?[[:digit:]]*\\.?[[:digit:]]*$", "-3.14159", true);

   std::string file_contents;

   const char* highlight_expression = // preprocessor directives: index 1
      "(^[ \\t]*#(?:(?>[^\\\\\\n]+)|\\\\(?>\\s*\\n|.))*)|";
      // comment: index 2
      "(//[^\\n]*|/\\*.*?\\*/)|"
      // literals: index 3
      "\\<([+-]?(?:(?:0x[[:xdigit:]]+)|(?:(?:[[:digit:]]*\\.)?[[:digit:]]+(?:[eE][+-]?[[:digit:]]+)?))u?(?:(?:int(?:8|16|32|64))|L)?)\\>|"
      // string literals: index 4
      "('(?:[^\\\\']|\\\\.)*'|\"(?:[^\\\\\"]|\\\\.)*\")|"
      // keywords: index 5
      "\\<(__asm|__cdecl|__declspec|__export|__far16|__fastcall|__fortran|__import"
      "|__pascal|__rtti|__stdcall|_asm|_cdecl|__except|_export|_far16|_fastcall"
      "|__finally|_fortran|_import|_pascal|_stdcall|__thread|__try|asm|auto|bool"
      "|break|case|catch|cdecl|char|class|const|const_cast|continue|default|delete"
      "|do|double|dynamic_cast|else|enum|explicit|extern|false|float|for|friend|goto"
      "|if|inline|int|long|mutable|namespace|new|operator|pascal|private|protected"
      "|public|register|reinterpret_cast|return|short|signed|sizeof|static|static_cast"
      "|struct|switch|template|this|throw|true|try|typedef|typeid|typename|union|unsigned"
      "|using|virtual|void|volatile|wchar_t|while)\\>"
      ;
   const char* class_expression = "(template[[:space:]]*<[^;:{]+>[[:space:]]*)?"
      "(class|struct)[[:space:]]*(\\w+([ \t]*\\([^)]*\\))?"
      "[[:space:]]*)*(\\w*)[[:space:]]*(<[^;:{]+>[[:space:]]*)?"
      "(\\{|:[^;\\{()]*\\{)";
   const char* call_expression = "\\w+\\s*(\\([^()]++(?:(?1)[^()]++)*+[^)]*\\))";

   const char* include_expression = "^[ \t]*#[ \t]*include[ \t]+(\"[^\"]+\"|<[^>]+>)";
   const char* boost_include_expression = "^[ \t]*#[ \t]*include[ \t]+(\"boost/[^\"]+\"|<boost/[^>]+>)";
   const char* brace_expression = "\\{[^{}]++((?0)[^{}]++)*+[^}]*+\\}";
   const char* function_with_body_expression = "(\\w+)\\s*(\\([^()]++(?:(?2)[^()]++)*+[^)]*\\))\\s*(\\{[^{}]++((?3)[^{}]++)*+[^}]*+\\})";


   load_file(file_contents, "../../../libs/libraries.htm");
   test_search("Beman|John|Dave", file_contents.c_str(), false, "../../../libs/libraries.htm");
   test_search("Beman|John|Dave", file_contents.c_str(), true, "../../../libs/libraries.htm");
   test_search("(?i)<p>.*?</p>", file_contents.c_str(), true, "../../../libs/libraries.htm");
   test_search("<a[^>]+href=(\"[^\"]*\"|[^[:space:]]+)[^>]*>", file_contents.c_str(), false, "../../../libs/libraries.htm");
   test_search("(?i)<a[^>]+href=(\"[^\"]*\"|[^[:space:]]+)[^>]*>", file_contents.c_str(), true, "../../../libs/libraries.htm");
   test_search("(?i)<h[12345678][^>]*>.*?</h[12345678]>", file_contents.c_str(), true, "../../../libs/libraries.htm");
   test_search("<img[^>]+src=(\"[^\"]*\"|[^[:space:]]+)[^>]*>", file_contents.c_str(), false, "../../../libs/libraries.htm");
   test_search("(?i)<img[^>]+src=(\"[^\"]*\"|[^[:space:]]+)[^>]*>", file_contents.c_str(), true, "../../../libs/libraries.htm");
   test_search("(?i)<font[^>]+face=(\"[^\"]*\"|[^[:space:]]+)[^>]*>.*?</font>", file_contents.c_str(), true, "../../../libs/libraries.htm");


   load_file(file_contents, "../../../boost/multiprecision/number.hpp");

   test_search(function_with_body_expression, file_contents.c_str(), true, "boost/multiprecision/number.hpp");
   test_search(brace_expression, file_contents.c_str(), true, "boost/multiprecision/number.hpp");
   test_search(call_expression, file_contents.c_str(), true, "boost/multiprecision/number.hpp");
   test_search(highlight_expression, file_contents.c_str(), true, "boost/multiprecision/number.hpp");
   test_search(class_expression, file_contents.c_str(), true, "boost/multiprecision/number.hpp");
   test_search(include_expression, file_contents.c_str(), true, "boost/multiprecision/number.hpp");
   test_search(boost_include_expression, file_contents.c_str(), true, "boost/multiprecision/number.hpp");

   return 0;
}

