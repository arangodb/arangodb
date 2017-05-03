/* Boost.Flyweight example of serialization.
 *
 * Copyright 2006-2014 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/flyweight for library home page.
 */

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <algorithm>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/flyweight.hpp>
#include <boost/flyweight/serialize.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/tokenizer.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace boost::flyweights;

typedef flyweight<std::string> fw_string;
typedef std::vector<fw_string> text_container;

/* Read a text file into a text_container and serialize to an archive. */

void save_serialization_file()
{
  /* Define a tokenizer on std::istreambuf. */
  
  typedef std::istreambuf_iterator<char> char_iterator;
  typedef boost::tokenizer<
    boost::char_separator<char>,
    char_iterator
  >                                      tokenizer;

  std::cout<<"enter input text file name: ";
  std::string in;
  std::getline(std::cin,in);
  std::ifstream ifs(in.c_str());
  if(!ifs){
    std::cout<<"can't open "<<in<<std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /* Tokenize using space and common punctuaction as separators, and
   * keeping the separators.
   */
   
  tokenizer tok=tokenizer(
    char_iterator(ifs),char_iterator(),
    boost::char_separator<char>(
      "",
      "\t\n\r !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"));
  text_container txt;
  for(tokenizer::iterator it=tok.begin();it!=tok.end();++it){
    txt.push_back(fw_string(*it));
  }

  std::cout<<"enter output serialization file name: ";
  std::string out;
  std::getline(std::cin,out);
  std::ofstream ofs(out.c_str());
  if(!ofs){
    std::cout<<"can't open "<<out<<std::endl;
    std::exit(EXIT_FAILURE);
  }
  boost::archive::text_oarchive oa(ofs);
  oa<<const_cast<const text_container&>(txt);
}

/* Read a serialization archive and save the result to a text file. */

void load_serialization_file()
{
  std::cout<<"enter input serialization file name: ";
  std::string in;
  std::getline(std::cin,in);
  std::ifstream ifs(in.c_str());
  if(!ifs){
    std::cout<<"can't open "<<in<<std::endl;
    std::exit(EXIT_FAILURE);
  }
  boost::archive::text_iarchive ia(ifs);
  text_container txt;
  ia>>txt;
  
  std::cout<<"enter output text file name: ";
  std::string out;
  std::getline(std::cin,out);
  std::ofstream ofs(out.c_str());
  if(!ofs){
    std::cout<<"can't open "<<out<<std::endl;
    std::exit(EXIT_FAILURE);
  }
  std::copy(
    txt.begin(),txt.end(),
    std::ostream_iterator<std::string>(ofs));
}

int main()
{
  try{
    std::cout<<"1 load a text file and save it as a serialization file\n"
               "2 load a serialization file and save it as a text file\n";
    for(;;){
      std::cout<<"select option, enter to exit: ";
      std::string str;
      std::getline(std::cin,str);
      if(str.empty())break;
      std::istringstream istr(str);
      int option=-1;
      istr>>option;
           if(option==1)save_serialization_file();
      else if(option==2)load_serialization_file();
    }
  }
  catch(const std::exception& e){
    std::cout<<"error: "<<e.what()<<std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  return 0;
}
