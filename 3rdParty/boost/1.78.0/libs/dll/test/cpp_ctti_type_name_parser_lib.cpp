// Copyright 2019 Lichao Xia
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/config.hpp>

#if (__cplusplus >= 201402L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)

#include <boost/dll/config.hpp>
#include <string>

namespace space {
template<typename... T>
class test_template_class
{};

template<typename T1,
         typename T2 = char,
         typename T3 = test_template_class<int>>
class test_template_class_have_default_args
{};

class BOOST_SYMBOL_EXPORT cpp_plugin_type_pasrser
{
private:
  /* data */
public:
  cpp_plugin_type_pasrser(/* args */);
  cpp_plugin_type_pasrser(int*);
  cpp_plugin_type_pasrser(const int*);
  cpp_plugin_type_pasrser(const volatile int*);
  cpp_plugin_type_pasrser(const std::string&);
  cpp_plugin_type_pasrser(const volatile std::string*);
  ~cpp_plugin_type_pasrser();
  void type_test(int*);
  void type_test(const int*);
  void type_test(const volatile int*);
  void type_test(std::string*);
  void type_test(const std::string*);
  void type_test(const volatile std::string*);
  void type_test(const std::string&);
  void type_test(void (*)(const char* volatile));
  void type_test(volatile const char* const* volatile*&&);
  void type_test(const char*) const;
  void type_test(const char*) volatile;
  void type_test(const char*) const volatile;
  void type_test(const test_template_class<>&);
  void type_test(const test_template_class<int>&);
  void type_test(const test_template_class<std::string>&);
  void type_test(const test_template_class<void(int)>&);
  void type_test(const test_template_class<char, int, std::string>&);
  void type_test(const test_template_class_have_default_args<int>&);
  void type_test(const test_template_class_have_default_args<int, double>&);
  void type_test(
    const test_template_class_have_default_args<int, double, std::string>&);
};

cpp_plugin_type_pasrser::cpp_plugin_type_pasrser(/* args */) {}
cpp_plugin_type_pasrser::cpp_plugin_type_pasrser(int*) {}
cpp_plugin_type_pasrser::cpp_plugin_type_pasrser(const int*) {}
cpp_plugin_type_pasrser::cpp_plugin_type_pasrser(const volatile int*) {}
cpp_plugin_type_pasrser::cpp_plugin_type_pasrser(const std::string&) {}
cpp_plugin_type_pasrser::cpp_plugin_type_pasrser(const volatile std::string*) {}
cpp_plugin_type_pasrser::~cpp_plugin_type_pasrser() {}
void
cpp_plugin_type_pasrser::type_test(int*)
{}
void
cpp_plugin_type_pasrser::type_test(const int*)
{}
void
cpp_plugin_type_pasrser::type_test(const volatile int*)
{}
void
cpp_plugin_type_pasrser::type_test(std::string*)
{}
void
cpp_plugin_type_pasrser::type_test(const std::string*)
{}
void
cpp_plugin_type_pasrser::type_test(const volatile std::string*)
{}

void
cpp_plugin_type_pasrser::type_test(const std::string&)
{}

void
cpp_plugin_type_pasrser::type_test(void (*)(const char* volatile))
{}

void
cpp_plugin_type_pasrser::type_test(const volatile char* const* volatile*&&)
{}

void
cpp_plugin_type_pasrser::type_test(const char*) const
{}

void
cpp_plugin_type_pasrser::type_test(const char*) volatile
{}

void
cpp_plugin_type_pasrser::type_test(const char*) const volatile
{}
void
cpp_plugin_type_pasrser::type_test(const test_template_class<>&)
{}
void
cpp_plugin_type_pasrser::type_test(const test_template_class<int>&)
{}
void
cpp_plugin_type_pasrser::type_test(const test_template_class<std::string>&)
{}
void
cpp_plugin_type_pasrser::type_test(
  const test_template_class<char, int, std::string>&)
{}
void
cpp_plugin_type_pasrser::type_test(const test_template_class<void(int)>&)
{}
void
cpp_plugin_type_pasrser::type_test(
  const test_template_class_have_default_args<int>&)
{}
void
cpp_plugin_type_pasrser::type_test(
  const test_template_class_have_default_args<int, double>&)
{}
void
cpp_plugin_type_pasrser::type_test(
  const test_template_class_have_default_args<int, double, std::string>&)
{}
} // namespace space

#endif
