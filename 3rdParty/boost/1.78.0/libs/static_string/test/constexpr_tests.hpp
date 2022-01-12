//
// Copyright (c) 2019-2020 Krystian Stasiowski (sdkrystian at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/static_string
//

#include <boost/static_string/static_string.hpp>

#include <string>

namespace boost {
namespace static_strings {

// char_traits aren't fully constexpr until c++20
#ifdef BOOST_STATIC_STRING_CPP14
struct cxper_char_traits
{
  using char_type = char;
  using int_type = int;
  using state_type = std::mbstate_t;

  static constexpr void assign(char_type& a, const char_type& b) noexcept { a = b;  }
  static constexpr bool eq(char_type a, char_type b) noexcept { return a == b; }
  static constexpr bool lt(char_type a, char_type b) noexcept { return a < b; }

  static constexpr int compare(const char_type*, const char_type*, std::size_t) { return 0; }
  static constexpr std::size_t length(const char_type* s)
  {
    std::size_t n = 0;
    while (*(s++));
    return n;
  }
  static constexpr const char_type* find(const char_type*, std::size_t, const char_type&){ return 0; }
  static constexpr char_type* move(char_type* dest, const char_type* src, std::size_t n)
  {
    const auto temp = dest;
    while (n--)
      *(dest++) = *(src++);
    return temp;
  }
  static constexpr char_type* copy(char_type* dest, const char_type* src, std::size_t n)
  {
    const auto temp = dest;
    while (n--)
      *(dest++) = *(src++);
    return temp;
  }
  static constexpr char_type* assign(char_type* dest, std::size_t n, char_type ch)
  {
    const auto temp = dest;
    while (n--)
      *(dest++) = ch;
    return temp;
  }
};
#else
using cxper_char_traits = std::char_traits<char>;
#endif
using cstatic_string = basic_static_string<50, char, cxper_char_traits>;

inline
constexpr
bool
testConstantEvaluation()
{
#ifdef BOOST_STATIC_STRING_CPP20
  // c++20 constexpr tests
  cstatic_string a;
  cstatic_string b(1, 'a');
  cstatic_string(b, 0);
  cstatic_string(b, 0, 1);
  cstatic_string("a", 1);
  cstatic_string("a");
  cstatic_string{b};
  cstatic_string({'a'});

  // assignment
  a = b;
  a = "a";
  a = 'a';
  a = {'a'};

  // assign
  a.assign(b);
  a.assign(b, 0, 1);
  a.assign("a", 1);
  a.assign("a");
  a.assign(b.begin(), b.end());
  a.assign({'a'});
  a.assign(1, 'a');

  // element access
  {
    auto j = a.at(0);
    static_cast<void>(j);
  }
  {
    auto j = a[0];
    static_cast<void>(j);
  }
  {
    auto j = a.front();
    static_cast<void>(j);
  }
  {
    auto j = a.back();
    static_cast<void>(j);
  }
  a.data();
  a.c_str();
  a.begin();
  a.cbegin();
  a.end();
  a.cend();

  // reverse iterators
  a.rbegin();
  a.crbegin();
  a.rend();
  a.crend();

  // capacity and size
  cstatic_string().size();
  // this is potentially marked nodiscard
  static_cast<void>(cstatic_string().empty());
  cstatic_string().length();
  cstatic_string().max_size();
  cstatic_string().capacity();

  // clear
  a.clear();

  // insert
  a.insert(a.begin(), 1, 'a');
  a.insert(0, a.begin());
  a.insert(0, a.begin(), 1);
  a.insert(a.begin(), 'a');
  a.insert(a.begin(), {'a'});

  // erase
  a.erase(0, 1);
  a.erase(a.begin());
  a.erase(a.begin(), a.end());

  // push
  a.push_back('a');
  a.pop_back();

  // append
  a.append(1, 'a');
  a.append("a", 1);
  a.append("a");
  a.append(a.begin(), a.end());
  a.append({'a'});

  // append operator
  a += 'a';
  a += "a";
  a += {'a'};

  // compare
  a.compare(b);
  a.compare(0, 1, b);
  a.compare(0, 1, b, 0, 1);
  a.compare("a");
  a.compare(0, 1, "a");
  a.compare(0, 1, "a", 1);

  a.substr(0);

  // subview
  a.subview(0);

  // copy
  char k[20]{};
  a.copy(k, 1, 0);

  // resize
  a.resize(1);
  a.resize(1, 'a');

  // swap
  a.swap(b);

  // replace
  a.replace(0, 1, a);
  a.replace(0, 1, a, 0, 1);
  a.replace(0, 1, a.data(), 1);
  a.replace(0, 1, a.data());
  a.replace(0, 1, 1, 'a');
  a.replace(a.begin(), a.end(), a);
  a.replace(a.begin(), a.end(), a.data(), 1);
  a.replace(a.begin(), a.end(), a.data());
  a.replace(a.begin(), a.end(), 1, 'a');
  a.replace(a.begin(), a.end(), a.begin(), a.end());
  a.replace(a.begin(), a.end(), {'a'});

#ifdef BOOST_STATIC_STRING_IS_CONST_EVAL
  a.clear();
  a.replace(a.begin(), a.end(), "a");
  a.replace(a.begin(), a.end(), "a", 1);
#endif

  // find
  a.find(a);
  a.find("a", 0, 1);
  a.find("a", 0);
  a.find('a', 0);

  // rfind
  a.rfind(a);
  a.rfind("a", 0, 1);
  a.rfind("a", 0);
  a.rfind('a', 0);

  // find_first_of
  a.find_first_of(a);
  a.find_first_of("a", 0, 1);
  a.find_first_of("a", 0);
  a.find_first_of('a', 0);

  // find_first_not_of
  a.find_first_not_of(a);
  a.find_first_not_of("a", 0, 1);
  a.find_first_not_of("a", 0);
  a.find_first_not_of('a', 0);

  // starts_with
  a.starts_with('a');
  a.starts_with("a");

  // ends_with
  a.ends_with('a');
  a.ends_with("a");

  return true;
#elif defined(BOOST_STATIC_STRING_CPP17)
  //c++17 constexpr tests

  // ctors
  cstatic_string a;
  cstatic_string b(1, 'a');
  cstatic_string(b, 0);
  cstatic_string(b, 0, 1);
  cstatic_string("a", 1);
  cstatic_string("a");
  cstatic_string{b};
  cstatic_string({'a'});

  // assignment
  a = b;
  a = "a";
  a = 'a';
  a = {'a'};

  // assign
  a.assign(b);
  a.assign(b, 0, 1);
  a.assign("a", 1);
  a.assign("a");
  a.assign(b.begin(), b.end());
  a.assign({'a'});
  a.assign(1, 'a');

  // element access
  {
    auto j = a.at(0);
    static_cast<void>(j);
  }
  {
    auto j = a[0];
    static_cast<void>(j);
  }
  {
    auto j = a.front();
    static_cast<void>(j);
  }
  {
    auto j = a.back();
    static_cast<void>(j);
  }
  a.data();
  a.c_str();
  a.begin();
  a.cbegin();
  a.end();
  a.cend();

  // reverse iterators
  //{
  //  auto j = a.rbegin();
  //}
  //{
  //  auto j = a.crbegin();
  //}
  //{
  //  auto j = a.rend();
  //}
  //{
  //  auto j = a.crend();
  //}

  // capacity and size
  cstatic_string().size();
  // this is potentially marked nodiscard
  static_cast<void>(cstatic_string().empty());
  cstatic_string().length();
  cstatic_string().max_size();
  cstatic_string().capacity();

  // clear
  a.clear();

  // insert
  a.insert(a.begin(), 1, 'a');
  a.insert(0, a.begin());
  a.insert(0, a.begin(), 1);
  a.insert(a.begin(), 'a');
  a.insert(a.begin(), {'a'});

  // erase
  a.erase(0, 1);
  a.erase(a.begin());
  a.erase(a.begin(), a.end());

  // push
  a.push_back('a');
  a.pop_back();

  // append
  a.append(1, 'a');
  a.append("a", 1);
  a.append("a");
  a.append(a.begin(), a.end());
  a.append({'a'});

  // append operator
  a += 'a';
  a += "a";
  a += {'a'};

  // compare
  a.compare(b);
  a.compare(0, 1, b);
  a.compare(0, 1, b, 0, 1);
  a.compare("a");
  a.compare(0, 1, "a");
  a.compare(0, 1, "a", 1);

  // substr
  // in gcc 5, a constexpr non-static member function returning the class
  // is a member of causes an ICE
#ifndef BOOST_STATIC_STRING_GCC5_BAD_CONSTEXPR
  a.substr(0);
#endif

  // subview
  a.subview(0);

  // copy
  char k[20]{};
  a.copy(k, 1, 0);

  // resize
  a.resize(1);
  a.resize(1, 'a');

  // swap
  a.swap(b);

  // replace
  a.replace(0, 1, a);
  a.replace(0, 1, a, 0, 1);
  a.replace(0, 1, a.data(), 1);
  a.replace(0, 1, a.data());
  a.replace(0, 1, 1, 'a');
  a.replace(a.begin(), a.end(), a);
  a.replace(a.begin(), a.end(), a.data(), 1);
  a.replace(a.begin(), a.end(), a.data());
  a.replace(a.begin(), a.end(), 1, 'a');
  a.replace(a.begin(), a.end(), a.begin(), a.end());
  a.replace(a.begin(), a.end(), {'a'});

#ifdef BOOST_STATIC_STRING_IS_CONST_EVAL
  a.clear();
  a.replace(a.begin(), a.end(), "a");
  a.replace(a.begin(), a.end(), "a", 1);
#endif

  // find
  a.find(a);
  a.find("a", 0, 1);
  a.find("a", 0);
  a.find('a', 0);

  // rfind
  a.rfind(a);
  a.rfind("a", 0, 1);
  a.rfind("a", 0);
  a.rfind('a', 0);

  // find_first_of
  a.find_first_of(a);
  a.find_first_of("a", 0, 1);
  a.find_first_of("a", 0);
  a.find_first_of('a', 0);

  // find_first_not_of
  a.find_first_not_of(a);
  a.find_first_not_of("a", 0, 1);
  a.find_first_not_of("a", 0);
  a.find_first_not_of('a', 0);

  // starts_with
  a.starts_with('a');
  a.starts_with("a");

  // ends_with
  a.ends_with('a');
  a.ends_with("a");

  return true;
#elif defined(BOOST_STATIC_STRING_CPP14)
  // c++14 constexpr tests

  // ctors
  cstatic_string a;
  cstatic_string b(1, 'a');
  cstatic_string(b, 0);
  cstatic_string(b, 0, 1);
  cstatic_string("a", 1);
  cstatic_string("a");
  cstatic_string{b};
  cstatic_string({'a'});

  // assignment
  a = b;
  a = "a";
  a = 'a';
  a = {'a'};

  // assign
  a.assign(b);
  a.assign(b, 0, 1);
  a.assign("a", 1);
  a.assign("a");
  a.assign(b.begin(), b.end());
  a.assign({'a'});
  a.assign(1, 'a');

  // element access
  {
    auto j = a.at(0);
    static_cast<void>(j);
  }
  {
    auto j = a[0];
    static_cast<void>(j);
  }
  {
    auto j = a.front();
    static_cast<void>(j);
  }
  {
    auto j = a.back();
    static_cast<void>(j);
  }
  a.data();
  a.c_str();
  a.begin();
  a.cbegin();
  a.end();
  a.cend();

  // capacity and size
  cstatic_string().size();
  // this is potentially marked nodiscard
  static_cast<void>(cstatic_string().empty());
  cstatic_string().length();
  cstatic_string().max_size();
  cstatic_string().capacity();

  // clear
  a.clear();

  // insert
  a.insert(a.begin(), 1, 'a');
  a.insert(0, a.begin());
  a.insert(0, a.begin(), 1);
  a.insert(a.begin(), 'a');
  a.insert(a.begin(), {'a'});

  // erase
  a.erase(0, 1);
  a.erase(a.begin());
  a.erase(a.begin(), a.end());

  // push
  a.push_back('a');
  a.pop_back();

  // append
  a.append(1, 'a');
  a.append("a", 1);
  a.append("a");
  a.append(a.begin(), a.end());
  a.append({'a'});

  // append operator
  a += 'a';
  a += "a";
  a += {'a'};

  // compare
  a.compare(b);
  a.compare(0, 1, b);
  a.compare(0, 1, b, 0, 1);
  a.compare("a");
  a.compare(0, 1, "a");
  a.compare(0, 1, "a", 1);

  // substr
  // in gcc 5, a constexpr non-static member function returning the class
  // is a member of causes an ICE
#ifndef BOOST_STATIC_STRING_GCC5_BAD_CONSTEXPR
  a.substr(0, 1);
#endif

  // subview
  a.subview(0);

  // copy
  char k[20]{};
  a.copy(k, 1, 0);

  // resize
  a.resize(1);
  a.resize(1, 'a');

  // swap
  a.swap(b);

  // replace
  a.replace(0, 1, a);
  a.replace(0, 1, a, 0, 1);
  a.replace(0, 1, a.data(), 1);
  a.replace(0, 1, a.data());
  a.replace(0, 1, 1, 'a');
  a.replace(a.begin(), a.end(), a);
  a.replace(a.begin(), a.end(), a.data(), 1);
  a.replace(a.begin(), a.end(), a.data());
  a.replace(a.begin(), a.end(), 1, 'a');
  a.replace(a.begin(), a.end(), a.begin(), a.end());
  a.replace(a.begin(), a.end(), {'a'});

#ifdef BOOST_STATIC_STRING_IS_CONST_EVAL
  a.clear();
  a.replace(a.begin(), a.end(), "a");
  a.replace(a.begin(), a.end(), "a", 1);
#endif

  // find
  a.find(a);
  a.find("a", 0, 1);
  a.find("a", 0);
  a.find('a', 0);

  // rfind
  a.rfind(a);
  a.rfind("a", 0, 1);
  a.rfind("a", 0);
  a.rfind('a', 0);

  // find_first_of
  a.find_first_of(a);
  a.find_first_of("a", 0, 1);
  a.find_first_of("a", 0);
  a.find_first_of('a', 0);

  // find_first_not_of
  a.find_first_not_of(a);
  a.find_first_not_of("a", 0, 1);
  a.find_first_not_of("a", 0);
  a.find_first_not_of('a', 0);

  // starts_with
  a.starts_with('a');
  a.starts_with("a");

  // ends_with
  a.ends_with('a');
  a.ends_with("a");
  return true;
#elif defined(BOOST_STATIC_STRING_CPP11)
  // c++11 constexpr tests
  return (cstatic_string().size() +
    cstatic_string().length() +
    cstatic_string().max_size() +
    cstatic_string().capacity()) != 0 &&
    cstatic_string().empty();
#endif
}
} // static_strings
} // boost