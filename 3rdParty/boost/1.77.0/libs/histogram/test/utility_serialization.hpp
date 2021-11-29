// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_TEST_UTILITY_SERIALIZATION_HPP
#define BOOST_HISTOGRAM_TEST_UTILITY_SERIALIZATION_HPP

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/config.hpp> // BOOST_WINDOWS
#include <boost/core/nvp.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

std::string join(const char* a, const char* b) {
  std::string filename = a;
  filename +=
#ifdef BOOST_WINDOWS
      "\\";
#else
      "/";
#endif
  filename += b;
  return filename;
}

template <class T>
void load_xml(const std::string& filename, T& t) {
  std::ifstream ifs(filename);
  assert(ifs.is_open());
  // manually skip XML comments at the beginning of the stream, because of
  // https://github.com/boostorg/serialization/issues/169
  char line[128];
  do {
    ifs.getline(line, 128);
    assert(std::strlen(line) < 127);
  } while (!ifs.fail() && !ifs.eof() && std::strstr(line, "-->") == nullptr);
  boost::archive::xml_iarchive ia(ifs);
  ia >> boost::make_nvp("item", t);
}

template <class T>
void print_xml(const std::string& filename, const T& t) {
  std::cout << filename << "\n";
  boost::archive::xml_oarchive oa(std::cout);
  oa << boost::make_nvp("item", t);
  std::cout << std::flush;
}

#endif
