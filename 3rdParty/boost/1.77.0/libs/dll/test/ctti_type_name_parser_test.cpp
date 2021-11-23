// Copyright 2019 Lichao Xia
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/config.hpp>

#if (__cplusplus >= 201402L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
#include "../example/b2_workarounds.hpp"

#include <boost/core/lightweight_test.hpp>
#include <boost/dll/smart_library.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <string>

class alias;

namespace space {
template<typename... T>
class test_template_class
{};

template<typename T1,
         typename T2 = char,
         typename T3 = test_template_class<int>>
class test_template_class_have_default_args
{};
} // namespace space

int
main(int argc, char* argv[])
{
  using mangled_storage = boost::dll::detail::mangled_storage_impl;
  using library = boost::dll::experimental::smart_library;

  boost::dll::fs::path pt = b2_workarounds::first_lib_from_argv(argc, argv);

  std::cout << "Library: " << pt << std::endl;
  //  boost::dll::library_info lib{pt};

  library lib(pt);
  mangled_storage ms = lib.symbol_storage();
  //  mangled_storage ms(lib);

  std::cout << "Symbols: " << std::endl;

  for (auto& s : ms.get_storage()) {
    std::cout << s.demangled << std::endl;
    std::cout << s.mangled << std::endl;
    std::cout << std::endl;
  }

  std::string v;

  ms.add_alias<alias>("space::cpp_plugin_type_pasrser");

  auto ctor1 = ms.get_constructor<alias()>();
  BOOST_TEST(!ctor1.empty());

  auto ctor2 = ms.get_constructor<alias(int*)>();
  BOOST_TEST(!ctor2.empty());

  auto ctor3 = ms.get_constructor<alias(const int*)>();
  BOOST_TEST(!ctor3.empty());

  auto ctor4 = ms.get_constructor<alias(const volatile int*)>();
  BOOST_TEST(!ctor4.empty());

  auto ctor5 = ms.get_constructor<alias(const std::string&)>();
  BOOST_TEST(!ctor5.empty());

  auto ctor6 = ms.get_constructor<alias(const volatile std::string*)>();
  BOOST_TEST(!ctor6.empty());

  v = ms.get_mem_fn<alias, void(int*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const int*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const volatile int*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(std::string*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const std::string*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const volatile std::string*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const std::string&)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(void (*)(const char* volatile))>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const volatile char* const* volatile*&&)>(
    "type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<const alias, void(const char*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<volatile alias, void(const char*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<const volatile alias, void(const char*)>("type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const space::test_template_class<>&)>(
    "type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const space::test_template_class<void(int)>&)>(
    "type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<alias, void(const space::test_template_class<int>&)>(
    "type_test");
  BOOST_TEST(!v.empty());

  v =
    ms.get_mem_fn<alias, void(const space::test_template_class<std::string>&)>(
      "type_test");
  BOOST_TEST(!v.empty());

  v =
    ms.get_mem_fn<alias,
                  void(
                    const space::test_template_class<char, int, std::string>&)>(
      "type_test");
  BOOST_TEST(!v.empty());

  v =
    ms.get_mem_fn<alias,
                  void(
                    const space::test_template_class_have_default_args<int>&)>(
      "type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<
    alias,
    void(const space::test_template_class_have_default_args<int, double>&)>(
    "type_test");
  BOOST_TEST(!v.empty());

  v = ms.get_mem_fn<
    alias,
    void(const space::
           test_template_class_have_default_args<int, double, std::string>&)>(
    "type_test");
  BOOST_TEST(!v.empty());

  auto dtor = ms.get_destructor<alias>();

  BOOST_TEST(!dtor.empty());

  return boost::report_errors();
}

#else
int
main()
{
  return 0;
}
#endif
