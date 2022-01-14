// Copyright 2019 Ramil Gauss.
// Copyright 2019-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#if (__cplusplus >= 201402L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)

#include <boost/dll/smart_library.hpp>
#include <boost/dll/import_mangled.hpp>
#include <boost/dll/import_class.hpp>

#include <iostream>
#include <string>

#include "../example/b2_workarounds.hpp"
#include <boost/core/lightweight_test.hpp>


namespace space {
  class BOOST_SYMBOL_EXPORT my_plugin {
  public:
    template <typename Arg>
    BOOST_SYMBOL_EXPORT int Func(); // defined in cpp_test_library.cpp
  };
}

int main(int argc, char** argv) {
  unsigned matches_found = 0;
  boost::dll::fs::path lib_path = b2_workarounds::first_lib_from_argv(argc, argv);
  boost::dll::experimental::smart_library lib(lib_path);

  auto storage = lib.symbol_storage().get_storage();
  for (auto& s : storage) {
    auto& demangled = s.demangled;
    BOOST_TEST(demangled.data());

    auto beginFound = demangled.find("Func<");
    if (beginFound == std::string::npos)
      continue;

    auto endFound = demangled.find("(");
    if (endFound == std::string::npos)
      continue;

    // Usually "Func<space::my_plugin>" on Linux, "Func<class space::my_plugin>" on Windows.
    auto funcName = demangled.substr(beginFound, endFound - beginFound);
    std::cout << "Function name: " << funcName.data() << std::endl;
    auto typeIndexFunc = boost::dll::experimental::import_mangled<space::my_plugin, int()>(lib, funcName);

    space::my_plugin cl;
    BOOST_TEST_EQ(typeIndexFunc(&cl), 42);

    ++matches_found;
    break;
  }
  BOOST_TEST_EQ(matches_found, 1);

  return boost::report_errors();
}


#else
int main() {}
#endif
