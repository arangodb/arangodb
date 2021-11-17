//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2019-2020 Krystian Stasiowski (sdkrystian at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/static_string
//

// Test that header file is self-contained.
#include <boost/static_string/static_string.hpp>
#include "constexpr_tests.hpp"
#include "compile_fail.hpp"

#include <boost/core/lightweight_test.hpp>
#include <cstdlib>
#include <cwchar>
#include <cctype>
#include <sstream>
#include <string>

namespace boost {
namespace static_strings {

template class basic_static_string<420, char>;
  
using string_view = basic_string_view<char, std::char_traits<char>>;

template <class S>
bool
testS(const S& s, typename S::size_type pos, typename S::size_type n)
{
  if (pos <= s.size())
  {
    S str = s.substr(pos, n);
    typename S::size_type rlen = (std::min)(n, s.size() - pos);
    return (S::traits_type::compare(s.data() + pos, str.data(), rlen) == 0);
  }
  else
  {
    BOOST_TEST_THROWS((s.substr(pos, n)), std::out_of_range);
    return true;
  }
}

template <class S>
bool
testSV(const S& s, typename S::size_type pos, typename S::size_type n)
{
  if (pos <= s.size())
  {
    typename S::string_view_type str = s.subview(pos, n);
    typename S::size_type rlen = (std::min)(n, s.size() - pos);
    return (S::traits_type::compare(s.data() + pos, str.data(), rlen) == 0);
  }
  else
  {
    BOOST_TEST_THROWS((s.subview(pos, n)), std::out_of_range);
    return true;
  }
}

template <class S>
bool
testAS(S s, const typename S::value_type* str, typename S::size_type n, S expected)
{
  s.assign(str, n);
  return s == expected;
}

template <class S>
bool
testI(S s, typename S::size_type pos, const typename S::value_type* str, typename S::size_type n, S expected)
{
  const typename S::size_type old_size = s.size();
  if (pos <= old_size)
  {
    s.insert(pos, str, n);
    return s == expected;
  }
  else
  {
    BOOST_TEST_THROWS((s.insert(pos, str, n)), std::out_of_range);
    return true;
  }
}

template <class S>
bool
testE(S s, typename S::size_type pos, typename S::size_type n, S expected)
{
  const typename S::size_type old_size = s.size();
  if (pos <= old_size)
  {
    s.erase(pos, n);
    return s[s.size()] == typename S::value_type() && s == expected;
  }
  else
  {
    BOOST_TEST_THROWS(s.erase(pos, n), std::out_of_range);
    return true;
  }
}

template <class S>
bool
testA(S s, const typename S::value_type* str, typename S::size_type n, S expected)
{
  return s.append(str, n) == expected;
}

int 
sign(int x)
{
  if (x == 0)
    return 0;
  if (x < 0)
    return -1;
  return 1;
}

template <class S>
bool
testC(const S& s, typename S::size_type pos, typename S::size_type n1, const typename S::value_type* str, typename S::size_type n2, int x)
{
  if (pos <= s.size())
    return sign(s.compare(pos, n1, str, n2)) == sign(x);
  else
  {
    BOOST_TEST_THROWS(s.compare(pos, n1, str, n2), std::out_of_range);
    return true;
  }
}

template <class S>
bool
testF(const S& s, const typename S::value_type* str, typename S::size_type pos, typename S::size_type n, typename S::size_type x)
{
  return s.find(str, pos, n) == x;
}

template <class S>
bool
testRF(const S& s, const typename S::value_type* str, typename S::size_type pos, typename S::size_type n, typename S::size_type x)
{
  return s.rfind(str, pos, n) == x;
}

template <class S>
bool
testFF(const S& s, const typename S::value_type* str, typename S::size_type pos, typename S::size_type n, typename S::size_type x)
{
  return s.find_first_of(str, pos, n) == x;
}

template <class S>
bool
testFL(const S& s, const typename S::value_type* str, typename S::size_type pos, typename S::size_type n, typename S::size_type x)
{
  return s.find_last_of(str, pos, n) == x;
}

template <class S>
bool
testFFN(const S& s, const typename S::value_type* str, typename S::size_type pos, typename S::size_type n, typename S::size_type x)
{
  return s.find_first_not_of(str, pos, n) == x;
}

template <class S>
bool
testFLN(const S& s, const typename S::value_type* str, typename S::size_type pos, typename S::size_type n, typename S::size_type x)
{
  return s.find_last_not_of(str, pos, n) == x;
}

template <class S>
bool
testR(S s, typename S::size_type pos1, typename S::size_type n1, const typename S::value_type* str, S expected)
{
  typename S::const_iterator first = s.begin() + pos1;
  typename S::const_iterator last = s.begin() + pos1 + n1;
  s.replace(first, last, str);
  return s == expected;
}

template <class S>
bool
testR(S s, typename S::size_type pos, typename S::size_type n1, typename S::size_type n2, typename S::value_type c, S expected)
{
  const typename S::size_type old_size = s.size();
  if (pos <= old_size)
  {
    s.replace(pos, n1, n2, c);
    return s == expected;
  }
  else
  {
    BOOST_TEST_THROWS((s.replace(pos, n1, n2, c)), std::out_of_range);
    return true;
  }
}

template <class S>
bool
testR(S s, typename S::size_type pos, typename S::size_type n1, const typename S::value_type* str, typename S::size_type n2, S expected)
{
  const typename S::size_type old_size = s.size();
  S s0 = s;
  if (n1 > old_size)
    s.size();
  if (pos <= old_size)
  {
    if (pos + n1 > s0.size())
      // this is a precondition violation for the const_iterator overload
      return s.replace(pos, n1, str, n2) == expected;
    else
      return s.replace(pos, n1, str, n2) == expected && 
        s0.replace(s0.begin() + pos, s0.begin() + pos + n1, str, str + n2) == expected;
  }
  else
  {
    BOOST_TEST_THROWS((s.replace(pos, n1, str, n2)), std::out_of_range);
    return true;
  }
}

template<typename Arithmetic>
bool
testTS(Arithmetic value, const char* str_expected = "", const wchar_t* wstr_expected = L"", bool test_expected = false)
{
  const auto str = to_static_string(value);
  const auto wstr = to_static_wstring(value);
  if (std::is_floating_point<Arithmetic>::value)
  {
    const auto std_res = std::to_string(value);
    const auto wstd_res = std::to_wstring(value);
    return str == std_res.data() && wstr == wstd_res.data();
  }
  else
  {
    if (std::is_signed<Arithmetic>::value)
    {
      return Arithmetic(std::strtoll(str.begin(), nullptr, 10)) == value &&
        Arithmetic(std::wcstoll(wstr.begin(), nullptr, 10)) == value &&
        (test_expected ? str == str_expected && wstr == wstr_expected : true);
    }
    else
    {
      return Arithmetic(std::strtoull(str.begin(), nullptr, 10)) == value &&
        Arithmetic(std::wcstoull(wstr.begin(), nullptr, 10)) == value &&
        (test_expected ? str == str_expected && wstr == wstr_expected : true);
    }
  }
}

// done
static
void
testConstruct()
{
    {
        static_string<1> s;
        BOOST_TEST(s.empty());
        BOOST_TEST(s.size() == 0);
        BOOST_TEST(s == "");
        BOOST_TEST(*s.end() == 0);
    }
    {
        static_string<4> s1(3, 'x');
        BOOST_TEST(! s1.empty());
        BOOST_TEST(s1.size() == 3);
        BOOST_TEST(s1 == "xxx");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            (static_string<2>(3, 'x')),
            std::length_error);
    }
    {
        static_string<5> s1("12345");
        BOOST_TEST(*s1.end() == 0);
        static_string<3> s2(s1, 2);
        BOOST_TEST(s2 == "345");
        BOOST_TEST(*s2.end() == 0);
        static_string<0> s3(s1, 5);
        BOOST_TEST(s3.empty());
        BOOST_TEST(s3.front() == 0);
        BOOST_TEST(*s3.end() == 0);
    }
    {
        static_string<5> s1("12345");
        static_string<2> s2(s1, 1, 2);
        BOOST_TEST(s2 == "23");
        BOOST_TEST(*s2.end() == 0);
        static_string<0> s3(s1, 5, 1);
        BOOST_TEST(s3.empty());
        BOOST_TEST(s3.front() == 0);
        BOOST_TEST(*s3.end() == 0);
        BOOST_TEST_THROWS(
            (static_string<5>(s1, 6)),
            std::out_of_range);
    }
    {
        static_string<5> s1("UVXYZ", 3);
        BOOST_TEST(s1 == "UVX");
        BOOST_TEST(*s1.end() == 0);
        static_string<5> s2("X\0""Y\0""Z", 3);
        BOOST_TEST(std::memcmp(
            s2.data(), "X\0""Y", 3) == 0);
        BOOST_TEST(*s2.end() == 0);
    }
    {
        static_string<5> s1("12345");
        static_string<3> s2(
            s1.begin() + 1, s1.begin() + 3);
        BOOST_TEST(s2 == "23");
        BOOST_TEST(*s2.end() == 0);
    }
    {
        static_string<5> s1("12345");
        static_string<5> s2(s1);
        BOOST_TEST(s2 == "12345");
        BOOST_TEST(*s2.end() == 0);
        static_string<6> s3(s1);
        BOOST_TEST(s3 == "12345");
        BOOST_TEST(*s3.end() == 0);
        BOOST_TEST_THROWS(
            (static_string<4>(s1)),
            std::length_error);
    }
    {
        static_string<3> s1({'1', '2', '3'});
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST(
            static_string<0>({}) == static_string<0>());
        BOOST_TEST_THROWS(
            (static_string<2>({'1', '2', '3'})),
            std::length_error);
    }
    {
        static_string<3> s1(
            string_view("123"));
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            (static_string<2>(string_view("123"))),
            std::length_error);
    }
    {
        static_string<5> s1(
            std::string("12345"), 2, 2);
        BOOST_TEST(s1 == "34");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            (static_string<2>(std::string("12345"), 1, 3)),
            std::length_error);
    }
    {
      BOOST_TEST_THROWS(static_string<5>("12345678"), std::length_error);
    }
}

//done
static
void
testAssignment()
{
    // assign(size_type count, CharT ch)
    BOOST_TEST(static_string<3>{}.assign(1, '*') == "*");
    BOOST_TEST(static_string<3>{}.assign(3, '*') == "***");
    BOOST_TEST(static_string<3>{"abc"}.assign(3, '*') == "***");
    BOOST_TEST_THROWS(static_string<1>{"a"}.assign(2, '*'), std::length_error);

    // assign(static_string const& s) noexcept
    BOOST_TEST(static_string<3>{}.assign(static_string<3>{"abc"}) == "abc");
    BOOST_TEST(static_string<3>{"*"}.assign(static_string<3>{"abc"}) == "abc");
    BOOST_TEST(static_string<3>{"***"}.assign(static_string<3>{"abc"}) == "abc");

    // assign(static_string<M, CharT, Traits> const& s)
    BOOST_TEST(static_string<3>{}.assign(static_string<5>{"abc"}) == "abc");
    BOOST_TEST(static_string<3>{"*"}.assign(static_string<5>{"abc"}) == "abc");
    BOOST_TEST(static_string<3>{"***"}.assign(static_string<5>{"abc"}) == "abc");
    {
      static_string<3> s("***");
      BOOST_TEST(s.assign(s) == s);
    }
    BOOST_TEST_THROWS(static_string<3>{}.assign(static_string<5>{"abcde"}), std::length_error);

    // assign(static_string<M, CharT, Traits> const& s, size_type pos, size_type count = npos)
    BOOST_TEST(static_string<4>{}.assign(static_string<5>{"abcde"}, 1) == "bcde");
    BOOST_TEST(static_string<3>{}.assign(static_string<5>{"abcde"}, 1, 3) == "bcd");
    BOOST_TEST(static_string<3>{"*"}.assign(static_string<5>{"abcde"}, 1, 3) == "bcd");
    BOOST_TEST(static_string<3>{"***"}.assign(static_string<5>{"abcde"}, 1, 3) == "bcd");
    BOOST_TEST_THROWS(static_string<3>{}.assign(static_string<5>{"abcde"}, 0), std::length_error);

    // assign(CharT const* s, size_type count)
    BOOST_TEST(static_string<3>{}.assign("abc", 3) == "abc");
    BOOST_TEST(static_string<3>{"*"}.assign("abc", 3) == "abc");
    BOOST_TEST_THROWS(static_string<1>{}.assign("abc", 3), std::length_error);
    
    // assign(CharT const* s)
    BOOST_TEST(static_string<3>{}.assign("abc") == "abc");
    BOOST_TEST(static_string<3>{"*"}.assign("abc") == "abc");
    BOOST_TEST_THROWS(static_string<1>{}.assign("abc"), std::length_error);

    // assign(InputIt first, InputIt last)
    {
        static_string<4> const cs{"abcd"};
        static_string<4> s{"ad"};
        BOOST_TEST(static_string<4>{}.assign(cs.begin(), cs.end()) == "abcd");
        BOOST_TEST(static_string<4>{"*"}.assign(cs.begin(), cs.end()) == "abcd");
        BOOST_TEST_THROWS(static_string<2>{"*"}.assign(cs.begin(), cs.end()), std::length_error);
    }
    
    // assign(std::initializer_list<CharT> ilist)
    BOOST_TEST(static_string<3>{}.assign({'a', 'b', 'c'}) == "abc");
    BOOST_TEST(static_string<3>{"*"}.assign({'a', 'b', 'c'}) == "abc");
    BOOST_TEST(static_string<3>{"***"}.assign({'a', 'b', 'c'}) == "abc");
    BOOST_TEST_THROWS(static_string<1>{}.assign({'a', 'b', 'c'}), std::length_error);

    // assign(T const& t)
    {
        struct T
        {
            operator string_view() const noexcept
            {
                return "abc";
            }
        };
        BOOST_TEST(static_string<3>{}.assign(T{}) == "abc");
        BOOST_TEST(static_string<3>{"*"}.assign(T{}) == "abc");
        BOOST_TEST(static_string<3>{"***"}.assign(T{}) == "abc");
        BOOST_TEST_THROWS(static_string<2>{"**"}.assign(T{}), std::length_error);
    }

    // assign(T const& t, size_type pos, size_type count = npos)
    {
        struct T
        {
            operator string_view() const noexcept
            {
                return "abcde";
            }
        };
        BOOST_TEST(static_string<5>{}.assign(T{}, 0) == "abcde");
        BOOST_TEST(static_string<5>{}.assign(T{}, 0, 5) == "abcde");
        BOOST_TEST(static_string<5>{}.assign(T{}, 1, 3) == "bcd");
        BOOST_TEST(static_string<5>{"*"}.assign(T{}, 1) == "bcde");
        BOOST_TEST_THROWS(static_string<2>{"**"}.assign(T{}, 6, 3), std::out_of_range);
        BOOST_TEST_THROWS(static_string<2>{"**"}.assign(T{}, 1, 3), std::length_error);
    }
    
    //---

    {
        static_string<3> s1("123");
        static_string<3> s2;
        s2 = s1;
        BOOST_TEST(s2 == "123");
        BOOST_TEST(*s2.end() == 0);
    }
    {
        static_string<3> s1("123");
        static_string<5> s2;
        s2 = s1;
        BOOST_TEST(s2 == "123");
        BOOST_TEST(*s2.end() == 0);
        static_string<1> s3;
        BOOST_TEST_THROWS(
            s3 = s1,
            std::length_error);
    }
    
    {
        static_string<3> s1;
        s1 = "123";
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
        static_string<1> s2;
        BOOST_TEST_THROWS(
            s2 = "123",
            std::length_error);
    }
    {
        static_string<1> s1;
        s1 = 'x';
        BOOST_TEST(s1 == "x");
        BOOST_TEST(*s1.end() == 0);
        static_string<0> s2;
        BOOST_TEST_THROWS(
            s2 = 'x',
            std::length_error);
    }
    {
        static_string<3> s1;
        s1 = {'1', '2', '3'};
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
        static_string<1> s2;
        BOOST_TEST_THROWS(
            (s2 = {'1', '2', '3'}),
            std::length_error);
    }
    {
        static_string<3> s1;
        s1 = string_view("123");
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
        static_string<1> s2;
        BOOST_TEST_THROWS(
            s2 = string_view("123"),
            std::length_error);
    }

    {
        static_string<4> s1;
        s1.assign(3, 'x');
        BOOST_TEST(s1 == "xxx");
        BOOST_TEST(*s1.end() == 0);
        static_string<2> s2;
        BOOST_TEST_THROWS(
            s2.assign(3, 'x'),
            std::length_error);
    }
    {
        static_string<5> s1("12345");
        BOOST_TEST(*s1.end() == 0);
        static_string<5> s2;
        s2.assign(s1);
        BOOST_TEST(s2 == "12345");
        BOOST_TEST(*s2.end() == 0);
    }
    {
        static_string<5> s1("12345");
        BOOST_TEST(*s1.end() == 0);
        static_string<7> s2;
        s2.assign(s1);
        BOOST_TEST(s2 == "12345");
        BOOST_TEST(*s2.end() == 0);
        static_string<3> s3;
        BOOST_TEST_THROWS(
            s3.assign(s1),
            std::length_error);
    }
    {
        static_string<5> s1("12345");
        static_string<5> s2;
        s2.assign(s1, 1);
        BOOST_TEST(s2 == "2345");
        BOOST_TEST(*s2.end() == 0);
        s2.assign(s1, 1, 2);
        BOOST_TEST(s2 == "23");
        BOOST_TEST(*s2.end() == 0);
        s2.assign(s1, 1, 100);
        BOOST_TEST(s2 == "2345");
        BOOST_TEST(*s2.end() == 0);
        BOOST_TEST_THROWS(
            (s2.assign(s1, 6)),
            std::out_of_range);
        static_string<3> s3;
        BOOST_TEST_THROWS(
            s3.assign(s1, 1),
            std::length_error);
    }
    {
        static_string<5> s1;
        s1.assign("12");
        BOOST_TEST(s1 == "12");
        BOOST_TEST(*s1.end() == 0);
        s1.assign("12345");
        BOOST_TEST(s1 == "12345");
        BOOST_TEST(*s1.end() == 0);
    }
    {
        static_string<5> s1;
        s1.assign("12345", 3);
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
    }
    {
        static_string<5> s1("12345");
        static_string<3> s2;
        s2.assign(s1.begin(), s1.begin() + 2);
        BOOST_TEST(s2 == "12");
        BOOST_TEST(*s2.end() == 0);
        BOOST_TEST_THROWS(
            (s2.assign(s1.begin(), s1.end())),
            std::length_error);
    }
    {
        static_string<5> s1;
        s1.assign({'1', '2', '3'});
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
        static_string<1> s2;
        BOOST_TEST_THROWS(
            (s2.assign({'1', '2', '3'})),
            std::length_error);
    }
    {
        static_string<5> s1;
        s1.assign(string_view("123"));
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
        s1.assign(string_view("12345"));
        BOOST_TEST(s1 == "12345");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            s1.assign(string_view("1234567")),
            std::length_error);
    }
    {
        static_string<5> s1;
        s1.assign(std::string("12345"), 2, 2);
        BOOST_TEST(s1 == "34");
        BOOST_TEST(*s1.end() == 0);
        s1.assign(std::string("12345"), 3);
        BOOST_TEST(s1 == "45");
        BOOST_TEST(*s1.end() == 0);
        static_string<2> s2;
        BOOST_TEST_THROWS(
            (s2.assign(std::string("12345"), 1, 3)),
            std::length_error);
    }

    using S = static_string<400>;
    BOOST_TEST(testAS(S(), "", 0, S()));
    BOOST_TEST(testAS(S(), "12345", 3, S("123")));
    BOOST_TEST(testAS(S(), "12345", 4, S("1234")));
    BOOST_TEST(testAS(S(), "12345678901234567890", 0, S()));
    BOOST_TEST(testAS(S(), "12345678901234567890", 1, S("1")));
    BOOST_TEST(testAS(S(), "12345678901234567890", 3, S("123")));
    BOOST_TEST(testAS(S(), "12345678901234567890", 20, S("12345678901234567890")));

    BOOST_TEST(testAS(S("12345"), "", 0, S()));
    BOOST_TEST(testAS(S("12345"), "12345", 5, S("12345")));
    BOOST_TEST(testAS(S("12345"), "1234567890", 10, S("1234567890")));

    BOOST_TEST(testAS(S("12345678901234567890"), "", 0, S()));
    BOOST_TEST(testAS(S("12345678901234567890"), "12345", 5, S("12345")));
    BOOST_TEST(testAS(S("12345678901234567890"), "12345678901234567890", 20,
               S("12345678901234567890")));
    BOOST_TEST(testAS(S(), "", 0, S()));
    BOOST_TEST(testAS(S(), "12345", 3, S("123")));
    BOOST_TEST(testAS(S(), "12345", 4, S("1234")));
    BOOST_TEST(testAS(S(), "12345678901234567890", 0, S()));
    BOOST_TEST(testAS(S(), "12345678901234567890", 1, S("1")));
    BOOST_TEST(testAS(S(), "12345678901234567890", 3, S("123")));
    BOOST_TEST(testAS(S(), "12345678901234567890", 20, S("12345678901234567890")));

    BOOST_TEST(testAS(S("12345"), "", 0, S()));
    BOOST_TEST(testAS(S("12345"), "12345", 5, S("12345")));
    BOOST_TEST(testAS(S("12345"), "1234567890", 10, S("1234567890")));

    BOOST_TEST(testAS(S("12345678901234567890"), "", 0, S()));
    BOOST_TEST(testAS(S("12345678901234567890"), "12345", 5, S("12345")));
    BOOST_TEST(testAS(S("12345678901234567890"), "12345678901234567890", 20,
               S("12345678901234567890")));

    S s_short = "123/";
    S s_long = "Lorem ipsum dolor sit amet, consectetur/";

    s_short.assign(s_short.data(), s_short.size());
    BOOST_TEST(s_short == "123/");
    s_short.assign(s_short.data() + 2, s_short.size() - 2);
    BOOST_TEST(s_short == "3/");

    s_long.assign(s_long.data(), s_long.size());
    BOOST_TEST(s_long == "Lorem ipsum dolor sit amet, consectetur/");

    s_long.assign(s_long.data() + 2, 8);
    BOOST_TEST(s_long == "rem ipsu");
}

// done
static
void
testElements()
{
    using cfs3 = static_string<3> const;

    // at(size_type pos)
    BOOST_TEST(static_string<3>{"abc"}.at(0) == 'a');
    BOOST_TEST(static_string<3>{"abc"}.at(2) == 'c');
    BOOST_TEST_THROWS(static_string<3>{""}.at(0), std::out_of_range);
    BOOST_TEST_THROWS(static_string<3>{"abc"}.at(4), std::out_of_range);
    
    // at(size_type pos) const
    BOOST_TEST(cfs3{"abc"}.at(0) == 'a');
    BOOST_TEST(cfs3{"abc"}.at(2) == 'c');
    BOOST_TEST_THROWS(cfs3{""}.at(0), std::out_of_range);
    BOOST_TEST_THROWS(cfs3{"abc"}.at(4), std::out_of_range);

    // operator[](size_type pos)
    BOOST_TEST(static_string<3>{"abc"}[0] == 'a');
    BOOST_TEST(static_string<3>{"abc"}[2] == 'c');
    BOOST_TEST(static_string<3>{"abc"}[3] == 0);
    BOOST_TEST(static_string<3>{""}[0] == 0);

    // operator[](size_type pos) const
    BOOST_TEST(cfs3{"abc"}[0] == 'a');
    BOOST_TEST(cfs3{"abc"}[2] == 'c');
    BOOST_TEST(cfs3{"abc"}[3] == 0);
    BOOST_TEST(cfs3{""}[0] == 0);

    // front()
    BOOST_TEST(static_string<3>{"a"}.front() == 'a');
    BOOST_TEST(static_string<3>{"abc"}.front() == 'a');

    // front() const
    BOOST_TEST(cfs3{"a"}.front() == 'a');
    BOOST_TEST(cfs3{"abc"}.front() == 'a');

    // back()
    BOOST_TEST(static_string<3>{"a"}.back() == 'a');
    BOOST_TEST(static_string<3>{"abc"}.back() == 'c');

    // back() const 
    BOOST_TEST(cfs3{"a"}.back() == 'a');
    BOOST_TEST(cfs3{"abc"}.back() == 'c');

    // data() noexcept
    // data() const noexcept
    // c_str() const noexcept
    // operator string_view_type() const noexcept

    //---

    {
        static_string<5> s("12345");
        BOOST_TEST(s.at(1) == '2');
        BOOST_TEST(s.at(4) == '5');
        BOOST_TEST_THROWS(
            s.at(5) = 0,
            std::out_of_range);
    }
    {
        static_string<5> const s("12345");
        BOOST_TEST(s.at(1) == '2');
        BOOST_TEST(s.at(4) == '5');
        BOOST_TEST_THROWS(
            s.at(5),
            std::out_of_range);
    }
    {
        static_string<5> s("12345");
        BOOST_TEST(s[1] == '2');
        BOOST_TEST(s[4] == '5');
        s[1] = '_';
        BOOST_TEST(s == "1_345");
    }
    {
        static_string<5> const s("12345");
        BOOST_TEST(s[1] == '2');
        BOOST_TEST(s[4] == '5');
        BOOST_TEST(s[5] == 0);
    }
    {
        static_string<3> s("123");
        BOOST_TEST(s.front() == '1');
        BOOST_TEST(s.back() == '3');
        s.front() = '_';
        BOOST_TEST(s == "_23");
        s.back() = '_';
        BOOST_TEST(s == "_2_");
    }
    {
        static_string<3> const s("123");
        BOOST_TEST(s.front() == '1');
        BOOST_TEST(s.back() == '3');
    }
    {
        static_string<3> s("123");
        BOOST_TEST(std::memcmp(
            s.data(), "123", 3) == 0);
    }
    {
        static_string<3> const s("123");
        BOOST_TEST(std::memcmp(
            s.data(), "123", 3) == 0);
    }
    {
        static_string<3> s("123");
        BOOST_TEST(std::memcmp(
            s.c_str(), "123\0", 4) == 0);
    }
    {
        static_string<3> s("123");
        string_view sv = s;
        BOOST_TEST(static_string<5>(sv) == "123");
    }
}

// done
static
void
testIterators()
{
    {
        static_string<3> s;
        BOOST_TEST(std::distance(s.begin(), s.end()) == 0);
        BOOST_TEST(std::distance(s.rbegin(), s.rend()) == 0);
        s = "123";
        BOOST_TEST(std::distance(s.begin(), s.end()) == 3);
        BOOST_TEST(std::distance(s.rbegin(), s.rend()) == 3);
    }
    {
        static_string<3> const s("123");
        BOOST_TEST(std::distance(s.begin(), s.end()) == 3);
        BOOST_TEST(std::distance(s.cbegin(), s.cend()) == 3);
        BOOST_TEST(std::distance(s.rbegin(), s.rend()) == 3);
        BOOST_TEST(std::distance(s.crbegin(), s.crend()) == 3);
    }
}

// done
static
void
testCapacity()
{
    // empty()
    BOOST_TEST(static_string<0>{}.empty());
    BOOST_TEST(static_string<1>{}.empty());
    BOOST_TEST(! static_string<1>{"a"}.empty());
    BOOST_TEST(! static_string<3>{"abc"}.empty());

    // size()
    BOOST_TEST(static_string<0>{}.size() == 0);
    BOOST_TEST(static_string<1>{}.size() == 0);
    BOOST_TEST(static_string<1>{"a"}.size() == 1);
    BOOST_TEST(static_string<3>{"abc"}.size() == 3);
    BOOST_TEST(static_string<5>{"abc"}.size() == 3);

    // length()
    BOOST_TEST(static_string<0>{}.length() == 0);
    BOOST_TEST(static_string<1>{}.length() == 0);
    BOOST_TEST(static_string<1>{"a"}.length() == 1);
    BOOST_TEST(static_string<3>{"abc"}.length() == 3);
    BOOST_TEST(static_string<5>{"abc"}.length() == 3);

    // max_size()
    BOOST_TEST(static_string<0>{}.max_size() == 0);
    BOOST_TEST(static_string<1>{}.max_size() == 1);
    BOOST_TEST(static_string<1>{"a"}.max_size() == 1);
    BOOST_TEST(static_string<3>{"abc"}.max_size() == 3);
    BOOST_TEST(static_string<5>{"abc"}.max_size() == 5);

    // reserve(std::size_t n)
    static_string<3>{}.reserve(0);
    static_string<3>{}.reserve(1);
    static_string<3>{}.reserve(3);
    BOOST_TEST_THROWS(static_string<0>{}.reserve(1), std::length_error);
    BOOST_TEST_THROWS(static_string<3>{}.reserve(4), std::length_error);

    // capacity()
    BOOST_TEST(static_string<0>{}.capacity() == 0);
    BOOST_TEST(static_string<1>{}.capacity() == 1);
    BOOST_TEST(static_string<1>{"a"}.capacity() == 1);
    BOOST_TEST(static_string<3>{"abc"}.capacity() == 3);
    BOOST_TEST(static_string<5>{"abc"}.capacity() == 5);

    //---

    static_string<3> s;
    BOOST_TEST(s.empty());
    BOOST_TEST(s.size() == 0);
    BOOST_TEST(s.length() == 0);
    BOOST_TEST(s.max_size() == 3);
    BOOST_TEST(s.capacity() == 3);
    s = "123";
    BOOST_TEST(! s.empty());
    BOOST_TEST(s.size() == 3);
    BOOST_TEST(s.length() == 3);
    s.reserve(0);
    s.reserve(3);
    BOOST_TEST_THROWS(
        s.reserve(4),
        std::length_error);
    s.shrink_to_fit();
    BOOST_TEST(! s.empty());
    BOOST_TEST(s.size() == 3);
    BOOST_TEST(s.length() == 3);
    BOOST_TEST(*s.end() == 0);
}

// done
static
void
testClear()
{
    // clear()
    static_string<3> s("123");
    s.clear();
    BOOST_TEST(s.empty());
    BOOST_TEST(*s.end() == 0);
}

// done
static
void
testInsert()
{
    using sv = string_view;
    using S = static_string<100>;

    // insert(size_type index, size_type count, CharT ch)
    // The overload resolution is ambiguous
    // here because 0 is also a pointer type
    //BOOST_TEST(static_string<3>{"bc"}.insert(0, 1, 'a') == "abc");
    BOOST_TEST(static_string<3>{"bc"}.insert(std::size_t(0), 1, 'a') == "abc");
    BOOST_TEST(static_string<3>{"ac"}.insert(1, 1, 'b') == "abc");
    BOOST_TEST(static_string<3>{"ab"}.insert(2, 1, 'c') == "abc");
    BOOST_TEST_THROWS(static_string<4>{"abc"}.insert(4, 1, '*'), std::out_of_range);
    BOOST_TEST_THROWS(static_string<3>{"abc"}.insert(1, 1, '*'), std::length_error);

    // insert(size_type index, CharT const* s)
    BOOST_TEST(static_string<3>{"bc"}.insert(0, "a") == "abc");
    BOOST_TEST_THROWS(static_string<4>{"abc"}.insert(4, "*"), std::out_of_range);
    BOOST_TEST_THROWS(static_string<3>{"abc"}.insert(1, "*"), std::length_error);

    // insert(size_type index, CharT const* s, size_type count)
    BOOST_TEST(static_string<4>{"ad"}.insert(1, "bcd", 2) == "abcd");
    BOOST_TEST_THROWS(static_string<4>{"abc"}.insert(4, "*"), std::out_of_range);
    BOOST_TEST_THROWS(static_string<3>{"abc"}.insert(1, "*"), std::length_error);
    
    // insert(size_type index, string_view_type sv)
    BOOST_TEST(static_string<3>{"ac"}.insert(1, sv{"b"}) == "abc");
    BOOST_TEST_THROWS(static_string<4>{"abc"}.insert(4, sv{"*"}), std::out_of_range);
    BOOST_TEST_THROWS(static_string<3>{"abc"}.insert(1, sv{"*"}), std::length_error);

    // insert(size_type index, string_view_type sv, size_type index_str, size_type count = npos)
    BOOST_TEST(static_string<4>{"ad"}.insert(1, sv{"abcd"}, 1, 2) == "abcd");
    BOOST_TEST(static_string<4>{"ad"}.insert(1, sv{"abc"}, 1) == "abcd");
    BOOST_TEST_THROWS((static_string<4>{"ad"}.insert(1, sv{"bc"}, 3, 0)), std::out_of_range);
    BOOST_TEST_THROWS((static_string<3>{"ad"}.insert(1, sv{"bc"}, 0, 2)), std::length_error);

    // insert(const_iterator pos, CharT ch)
    {
        static_string<3> s{"ac"};
        BOOST_TEST(s.insert(s.begin() + 1, 'b') == s.begin() + 1);
        BOOST_TEST(s == "abc");
        BOOST_TEST_THROWS(s.insert(s.begin() + 1, '*'), std::length_error);
    }

    // insert(const_iterator pos, size_type count, CharT ch)
    {
        static_string<4> s{"ac"};
        BOOST_TEST(s.insert(s.begin() + 1, 2, 'b') == s.begin() + 1);
        BOOST_TEST(s == "abbc");
        BOOST_TEST_THROWS(s.insert(s.begin() + 1, 2, '*'), std::length_error);
    }

    // insert(const_iterator pos, InputIt first, InputIt last)
    {
        static_string<4> const cs{"abcd"};
        static_string<4> s{"ad"};
        BOOST_TEST(s.insert(s.begin() + 1, cs.begin() + 1, cs.begin() + 3) == s.begin() + 1);
        BOOST_TEST(s == "abcd");
    }

    // insert(const_iterator pos, std::initializer_list<CharT> ilist)
    {
        static_string<4> s{"ad"};
        BOOST_TEST(s.insert(s.begin() + 1, {'b', 'c'}) == s.begin() + 1);
        BOOST_TEST(s == "abcd");
    }

    // insert(size_type, static_string) checked
    {
        static_string<10> s1 = "ad";
        static_string<10> s2 = "bc";
        BOOST_TEST(s1.insert(1, s2) == "abcd");
    }

     // insert(size_type, static_string, size_type, size_type)
    {
        static_string<10> s1 = "ad";
        static_string<10> s2 = "abcd";
        BOOST_TEST(s1.insert(1, s2, 1, 2) == "abcd");
    }

    // insert(size_type index, T const& t)
    {
        struct T
        {
            operator string_view() const noexcept
            {
                return "b";
            }
        };
        BOOST_TEST(static_string<3>{"ac"}.insert(1, T{}) == "abc");
        BOOST_TEST_THROWS(static_string<4>{"abc"}.insert(4, T{}), std::out_of_range);
        BOOST_TEST_THROWS(static_string<3>{"abc"}.insert(1, T{}), std::length_error);
    }

    // insert(size_type index, T const& t, size_type index_str, size_type count = npos)
    {
        struct T
        {
            operator string_view() const noexcept
            {
                return "abcd";
            }
        };
        BOOST_TEST(static_string<6>{"ae"}.insert(1, T{}, 1) == "abcde");
        BOOST_TEST(static_string<6>{"abe"}.insert(2, T{}, 2) == "abcde");
        BOOST_TEST(static_string<4>{"ac"}.insert(1, T{}, 1, 1) == "abc");
        BOOST_TEST(static_string<4>{"ad"}.insert(1, T{}, 1, 2) == "abcd");
        BOOST_TEST_THROWS(static_string<4>{"abc"}.insert(4, T{}), std::out_of_range);
        BOOST_TEST_THROWS(static_string<3>{"abc"}.insert(1, T{}), std::length_error);
    }
    // insert(const_iterator, InputIterator, InputIterator)
    {
      std::stringstream a("defghi");
      static_string<30> b = "abcjklmnop";
      b.insert(b.begin() + 3,
        std::istream_iterator<char>(a),
        std::istream_iterator<char>());
      BOOST_TEST(b == "abcdefghijklmnop");
    }
    //---

    {
        // Using 7 as the size causes a miscompile in MSVC14.2 x64 Release
        static_string<8> s1("12345");
        s1.insert(2, 2, '_');
        BOOST_TEST(s1 == "12__345");
        BOOST_TEST(*s1.end() == 0);
        static_string<6> s2("12345");
        BOOST_TEST_THROWS(
            (s2.insert(2, 2, '_')),
            std::length_error);
        static_string<6> s3("12345");
        BOOST_TEST_THROWS(
            (s3.insert(6, 2, '_')),
            std::out_of_range);
    }
    {
        static_string<7> s1("12345");
        s1.insert(2, "__");
        BOOST_TEST(s1 == "12__345");
        BOOST_TEST(*s1.end() == 0);
        static_string<6> s2("12345");
        BOOST_TEST_THROWS(
            (s2.insert(2, "__")),
            std::length_error);
        static_string<6> s3("12345");
        BOOST_TEST_THROWS(
            (s2.insert(6, "__")),
            std::out_of_range);
    }
    {
        static_string<7> s1("12345");
        s1.insert(2, "TUV", 2);
        BOOST_TEST(s1 == "12TU345");
        BOOST_TEST(*s1.end() == 0);
        static_string<6> s2("12345");
        BOOST_TEST_THROWS(
            (s2.insert(2, "TUV", 2)),
            std::length_error);
        static_string<6> s3("12345");
        BOOST_TEST_THROWS(
            (s3.insert(6, "TUV", 2)),
            std::out_of_range);
    }
    {
        static_string<7> s1("12345");
        s1.insert(2, static_string<3>("TU"));
        BOOST_TEST(s1 == "12TU345");
        BOOST_TEST(*s1.end() == 0);
        static_string<6> s2("12345");
        BOOST_TEST_THROWS(
            (s2.insert(2, static_string<3>("TUV"))),
            std::length_error);
        static_string<6> s3("12345");
        BOOST_TEST_THROWS(
            (s3.insert(6, static_string<3>("TUV"))),
            std::out_of_range);
    }
    {
        static_string<7> s1("12345");
        s1.insert(2, static_string<3>("TUV"), 1);
        BOOST_TEST(s1 == "12UV345");
        BOOST_TEST(*s1.end() == 0);
        s1 = "12345";
        s1.insert(2, static_string<3>("TUV"), 1, 1);
        BOOST_TEST(s1 == "12U345");
        BOOST_TEST(*s1.end() == 0);
        static_string<6> s2("12345");
        BOOST_TEST_THROWS(
            (s2.insert(2, static_string<3>("TUV"), 1, 2)),
            std::length_error);
        static_string<6> s3("12345");
        BOOST_TEST_THROWS(
            (s3.insert(6, static_string<3>("TUV"), 1, 2)),
            std::out_of_range);
    }
    {
        static_string<4> s1("123");
        s1.insert(s1.begin() + 1, '_');
        BOOST_TEST(s1 == "1_23");
        BOOST_TEST(*s1.end() == 0);
        static_string<3> s2("123");
        BOOST_TEST_THROWS(
            (s2.insert(s2.begin() + 1, '_')),
            std::length_error);
    }
    {
        static_string<4> s1("12");
        s1.insert(s1.begin() + 1, 2, '_');
        BOOST_TEST(s1 == "1__2");
        BOOST_TEST(*s1.end() == 0);
        static_string<4> s2("123");
        BOOST_TEST_THROWS(
            (s2.insert(s2.begin() + 1, 2, ' ')),
            std::length_error);
    }
    {
        static_string<3> s1("123");
        static_string<5> s2("UV");
        s2.insert(s2.begin() + 1, s1.begin(), s1.end());
        BOOST_TEST(s2 == "U123V");
        BOOST_TEST(*s2.end() == 0);
        static_string<4> s3("UV");
        BOOST_TEST_THROWS(
            (s3.insert(s3.begin() + 1, s1.begin(), s1.end())),
            std::length_error);
    }
    {
        static_string<5> s1("123");
        s1.insert(1, string_view("UV"));
        BOOST_TEST(s1 == "1UV23");
        BOOST_TEST(*s1.end() == 0);
        static_string<4> s2("123");
        BOOST_TEST_THROWS(
            (s2.insert(1, string_view("UV"))),
            std::length_error);
        static_string<5> s3("123");
        BOOST_TEST_THROWS(
            (s3.insert(5, string_view("UV"))),
            std::out_of_range);
    }
    {
        static_string<5> s1("123");
        s1.insert(1, std::string("UV"));
        BOOST_TEST(s1 == "1UV23");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            (s1.insert(1, std::string("UV"))),
            std::length_error);
    }
    {
        static_string<6> s1("123");
        s1.insert(1, std::string("UVX"), 1);
        BOOST_TEST(s1 == "1VX23");
        BOOST_TEST(*s1.end() == 0);
        s1.insert(4, std::string("PQR"), 1, 1);
        BOOST_TEST(s1 == "1VX2Q3");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            (s1.insert(4, std::string("PQR"), 1, 1)),
            std::length_error);
    }
    // test insert with source inside self

    {
      static_string<30> fs1 = "0123456789";
      BOOST_TEST(fs1.insert(0, fs1.data(), 4) == "01230123456789");
    }
    {
      static_string<30> fs1 = "0123456789";
      BOOST_TEST(fs1.insert(5, fs1.data(), 4) == "01234012356789");
    }
    {
      static_string<30> fs1 = "0123456789";
      BOOST_TEST(fs1.insert(5, fs1.data(), 10) == "01234012345678956789");
    }
    {
      static_string<30> fs1 = "0123456789";
      BOOST_TEST(fs1.insert(5, fs1.data() + 6, 3) == "0123467856789");
    }

    S s_short = "123/";
    S s_long = "Lorem ipsum dolor sit amet, consectetur/";

    BOOST_TEST(s_short.insert(0, s_short.data(), s_short.size()) == "123/123/");
    BOOST_TEST(s_short.insert(0, s_short.data(), s_short.size()) == "123/123/123/123/");
    BOOST_TEST(s_short.insert(0, s_short.data(), s_short.size()) == "123/123/123/123/123/123/123/123/");
    BOOST_TEST(s_long.insert(0, s_long.data(), s_long.size()) == "Lorem ipsum dolor sit amet, consectetur/Lorem ipsum dolor sit amet, consectetur/");

    BOOST_TEST(testI(S("abcde"), 6, "12345", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 2, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 4, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 9, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 19, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 20, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 1, S("1abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 2, S("12abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 4, S("1234abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 5, S("12345abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 1, S("1abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 5, S("12345abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 9, S("123456789abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 10, S("1234567890abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 1, S("1abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 10, S("1234567890abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 19, S("1234567890123456789abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 20, S("12345678901234567890abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 1, S("a1bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 2, S("a12bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 4, S("a1234bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 5, S("a12345bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 1, S("a1bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 5, S("a12345bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 9, S("a123456789bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 10, S("a1234567890bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 1, S("a1bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 10, S("a1234567890bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 19, S("a1234567890123456789bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 20, S("a12345678901234567890bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 1, S("abcde1fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 2, S("abcde12fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 4, S("abcde1234fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 5, S("abcde12345fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 1, S("abcde1fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 5, S("abcde12345fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 9, S("abcde123456789fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 10, S("abcde1234567890fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 1, S("abcde1fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 10, S("abcde1234567890fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 19, S("abcde1234567890123456789fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 20, S("abcde12345678901234567890fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 1, S("abcdefghi1j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 2, S("abcdefghi12j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 4, S("abcdefghi1234j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 5, S("abcdefghi12345j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 1, S("abcdefghi1j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 5, S("abcdefghi12345j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 9, S("abcdefghi123456789j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 10, S("abcdefghi1234567890j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 1, S("abcdefghi1j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 10, S("abcdefghi1234567890j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 19, S("abcdefghi1234567890123456789j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 20, S("abcdefghi12345678901234567890j")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 1, S("abcdefghij1")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 2, S("abcdefghij12")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 4, S("abcdefghij1234")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 5, S("abcdefghij12345")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 1, S("abcdefghij1")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 5, S("abcdefghij12345")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 9, S("abcdefghij123456789")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 10, S("abcdefghij1234567890")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 1, S("abcdefghij1")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 10, S("abcdefghij1234567890")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 19, S("abcdefghij1234567890123456789")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 20, S("abcdefghij12345678901234567890")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 2, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 4, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 9, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 19, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 20, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 1, S("1abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 2, S("12abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 4, S("1234abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 5, S("12345abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 1, S("1abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 5, S("12345abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 9, S("123456789abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 10, S("1234567890abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 1, S("1abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 10, S("1234567890abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 19, S("1234567890123456789abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 20, S("12345678901234567890abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 1, S("a1bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 2, S("a12bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 4, S("a1234bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 5, S("a12345bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 1, S("a1bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 5, S("a12345bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 9, S("a123456789bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 10, S("a1234567890bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 1, S("a1bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 10, S("a1234567890bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 19, S("a1234567890123456789bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 20, S("a12345678901234567890bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 1, S("abcdefghij1klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 2, S("abcdefghij12klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 4, S("abcdefghij1234klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 5, S("abcdefghij12345klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 1, S("abcdefghij1klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 5, S("abcdefghij12345klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 9, S("abcdefghij123456789klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 10, S("abcdefghij1234567890klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 1, S("abcdefghij1klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 10, S("abcdefghij1234567890klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 19, S("abcdefghij1234567890123456789klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 20, S("abcdefghij12345678901234567890klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 1, S("abcdefghijklmnopqrs1t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 2, S("abcdefghijklmnopqrs12t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 4, S("abcdefghijklmnopqrs1234t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 5, S("abcdefghijklmnopqrs12345t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 1, S("abcdefghijklmnopqrs1t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 5, S("abcdefghijklmnopqrs12345t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 9, S("abcdefghijklmnopqrs123456789t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 10, S("abcdefghijklmnopqrs1234567890t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 1, S("abcdefghijklmnopqrs1t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 10, S("abcdefghijklmnopqrs1234567890t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 19, S("abcdefghijklmnopqrs1234567890123456789t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 20, S("abcdefghijklmnopqrs12345678901234567890t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 1, S("abcdefghijklmnopqrst1")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 2, S("abcdefghijklmnopqrst12")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 4, S("abcdefghijklmnopqrst1234")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 5, S("abcdefghijklmnopqrst12345")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 1, S("abcdefghijklmnopqrst1")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 5, S("abcdefghijklmnopqrst12345")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 9, S("abcdefghijklmnopqrst123456789")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 10, S("abcdefghijklmnopqrst1234567890")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 1, S("abcdefghijklmnopqrst1")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 10, S("abcdefghijklmnopqrst1234567890")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 19, S("abcdefghijklmnopqrst1234567890123456789")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 20, S("abcdefghijklmnopqrst12345678901234567890")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 2, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 4, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 9, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 19, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 20, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 2, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 4, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 9, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 19, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 20, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 1, S("1abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 2, S("12abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 4, S("1234abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345", 5, S("12345abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 1, S("1abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 5, S("12345abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 9, S("123456789abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "1234567890", 10, S("1234567890abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 1, S("1abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 10, S("1234567890abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 19, S("1234567890123456789abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 0, "12345678901234567890", 20, S("12345678901234567890abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 1, S("a1bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 2, S("a12bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 4, S("a1234bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345", 5, S("a12345bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 1, S("a1bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 5, S("a12345bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 9, S("a123456789bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "1234567890", 10, S("a1234567890bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 1, S("a1bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 10, S("a1234567890bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 19, S("a1234567890123456789bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 1, "12345678901234567890", 20, S("a12345678901234567890bcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 1, S("abcde1fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 2, S("abcde12fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 4, S("abcde1234fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345", 5, S("abcde12345fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 1, S("abcde1fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 5, S("abcde12345fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 9, S("abcde123456789fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "1234567890", 10, S("abcde1234567890fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 1, S("abcde1fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 10, S("abcde1234567890fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 19, S("abcde1234567890123456789fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 5, "12345678901234567890", 20, S("abcde12345678901234567890fghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 1, S("abcdefghi1j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 2, S("abcdefghi12j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 4, S("abcdefghi1234j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345", 5, S("abcdefghi12345j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 1, S("abcdefghi1j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 5, S("abcdefghi12345j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 9, S("abcdefghi123456789j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "1234567890", 10, S("abcdefghi1234567890j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 1, S("abcdefghi1j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 10, S("abcdefghi1234567890j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 19, S("abcdefghi1234567890123456789j")));
    BOOST_TEST(testI(S("abcdefghij"), 9, "12345678901234567890", 20, S("abcdefghi12345678901234567890j")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 1, S("abcdefghij1")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 2, S("abcdefghij12")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 4, S("abcdefghij1234")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345", 5, S("abcdefghij12345")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 1, S("abcdefghij1")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 5, S("abcdefghij12345")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 9, S("abcdefghij123456789")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "1234567890", 10, S("abcdefghij1234567890")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 0, S("abcdefghij")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 1, S("abcdefghij1")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 10, S("abcdefghij1234567890")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 19, S("abcdefghij1234567890123456789")));
    BOOST_TEST(testI(S("abcdefghij"), 10, "12345678901234567890", 20, S("abcdefghij12345678901234567890")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 2, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 4, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 9, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 19, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 20, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 1, S("1abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 2, S("12abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 4, S("1234abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345", 5, S("12345abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 1, S("1abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 5, S("12345abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 9, S("123456789abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "1234567890", 10, S("1234567890abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 1, S("1abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 10, S("1234567890abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 19, S("1234567890123456789abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 0, "12345678901234567890", 20, S("12345678901234567890abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 1, S("a1bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 2, S("a12bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 4, S("a1234bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345", 5, S("a12345bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 1, S("a1bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 5, S("a12345bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 9, S("a123456789bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "1234567890", 10, S("a1234567890bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 1, S("a1bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 10, S("a1234567890bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 19, S("a1234567890123456789bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 1, "12345678901234567890", 20, S("a12345678901234567890bcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 1, S("abcdefghij1klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 2, S("abcdefghij12klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 4, S("abcdefghij1234klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345", 5, S("abcdefghij12345klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 1, S("abcdefghij1klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 5, S("abcdefghij12345klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 9, S("abcdefghij123456789klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "1234567890", 10, S("abcdefghij1234567890klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 1, S("abcdefghij1klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 10, S("abcdefghij1234567890klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 19, S("abcdefghij1234567890123456789klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 10, "12345678901234567890", 20, S("abcdefghij12345678901234567890klmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 1, S("abcdefghijklmnopqrs1t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 2, S("abcdefghijklmnopqrs12t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 4, S("abcdefghijklmnopqrs1234t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345", 5, S("abcdefghijklmnopqrs12345t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 1, S("abcdefghijklmnopqrs1t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 5, S("abcdefghijklmnopqrs12345t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 9, S("abcdefghijklmnopqrs123456789t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "1234567890", 10, S("abcdefghijklmnopqrs1234567890t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 1, S("abcdefghijklmnopqrs1t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 10, S("abcdefghijklmnopqrs1234567890t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 19, S("abcdefghijklmnopqrs1234567890123456789t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 19, "12345678901234567890", 20, S("abcdefghijklmnopqrs12345678901234567890t")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 1, S("abcdefghijklmnopqrst1")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 2, S("abcdefghijklmnopqrst12")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 4, S("abcdefghijklmnopqrst1234")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345", 5, S("abcdefghijklmnopqrst12345")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 1, S("abcdefghijklmnopqrst1")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 5, S("abcdefghijklmnopqrst12345")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 9, S("abcdefghijklmnopqrst123456789")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "1234567890", 10, S("abcdefghijklmnopqrst1234567890")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 1, S("abcdefghijklmnopqrst1")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 10, S("abcdefghijklmnopqrst1234567890")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 19, S("abcdefghijklmnopqrst1234567890123456789")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 20, "12345678901234567890", 20, S("abcdefghijklmnopqrst12345678901234567890")));
    BOOST_TEST(testI(S("abcde"), 6, "", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 2, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 4, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 9, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "1234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 19, S("can't happen")));
    BOOST_TEST(testI(S("abcde"), 6, "12345678901234567890", 20, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 2, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 4, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 9, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "1234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 19, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghij"), 11, "12345678901234567890", 20, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 2, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 4, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 5, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 9, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "1234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 0, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 1, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 10, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 19, S("can't happen")));
    BOOST_TEST(testI(S("abcdefghijklmnopqrst"), 21, "12345678901234567890", 20, S("can't happen")));
}

// done 
static
void
testErase()
{
    // erase(size_type index = 0, size_type count = npos)
    BOOST_TEST(static_string<3>{"abc"}.erase() == "");
    BOOST_TEST(static_string<3>{"abc"}.erase(1) == "a");
    BOOST_TEST(static_string<3>{"abc"}.erase(2) == "ab");
    BOOST_TEST(static_string<3>{"abc"}.erase(1, 1) == "ac");
    BOOST_TEST(static_string<3>{"abc"}.erase(0, 2) == "c");
    BOOST_TEST(static_string<3>{"abc"}.erase(3, 0) == "abc");
    BOOST_TEST(static_string<3>{"abc"}.erase(3, 4) == "abc");
    BOOST_TEST_THROWS(static_string<3>{"abc"}.erase(4, 0), std::out_of_range);

    // erase(const_iterator pos)
    {
        static_string<3> s{"abc"};
        BOOST_TEST(s.erase(s.begin() + 1) == s.begin() + 1);
        BOOST_TEST(s == "ac");
    }

    // erase(const_iterator first, const_iterator last)
    {
        static_string<4> s{"abcd"};
        BOOST_TEST(s.erase(s.begin() + 1, s.begin() + 3) == s.begin() + 1);
        BOOST_TEST(s == "ad");
    }

    //---

    {
        static_string<9> s1("123456789");
        BOOST_TEST(s1.erase(1, 1) == "13456789");
        BOOST_TEST(s1 == "13456789");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST(s1.erase(5) == "13456");
        BOOST_TEST(s1 == "13456");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            s1.erase(7),
            std::out_of_range);
    }
    {
        static_string<9> s1("123456789");
        BOOST_TEST(*s1.erase(s1.begin() + 5) == '7');
        BOOST_TEST(s1 == "12345789");
        BOOST_TEST(*s1.end() == 0);
    }
    {
        static_string<9> s1("123456789");
        BOOST_TEST(*s1.erase(
            s1.begin() + 5, s1.begin() + 7) == '8');
        BOOST_TEST(s1 == "1234589");
        BOOST_TEST(*s1.end() == 0);
    }

    using S = static_string<400>;

    BOOST_TEST(testE(S(""), 0, 0, S("")));
    BOOST_TEST(testE(S(""), 0, 1, S("")));
    BOOST_TEST(testE(S(""), 1, 0, S("can't happen")));
    BOOST_TEST(testE(S("abcde"), 0, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 0, 1, S("bcde")));
    BOOST_TEST(testE(S("abcde"), 0, 2, S("cde")));
    BOOST_TEST(testE(S("abcde"), 0, 4, S("e")));
    BOOST_TEST(testE(S("abcde"), 0, 5, S("")));
    BOOST_TEST(testE(S("abcde"), 0, 6, S("")));
    BOOST_TEST(testE(S("abcde"), 1, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 1, 1, S("acde")));
    BOOST_TEST(testE(S("abcde"), 1, 2, S("ade")));
    BOOST_TEST(testE(S("abcde"), 1, 3, S("ae")));
    BOOST_TEST(testE(S("abcde"), 1, 4, S("a")));
    BOOST_TEST(testE(S("abcde"), 1, 5, S("a")));
    BOOST_TEST(testE(S("abcde"), 2, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 2, 1, S("abde")));
    BOOST_TEST(testE(S("abcde"), 2, 2, S("abe")));
    BOOST_TEST(testE(S("abcde"), 2, 3, S("ab")));
    BOOST_TEST(testE(S("abcde"), 2, 4, S("ab")));
    BOOST_TEST(testE(S("abcde"), 4, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 4, 1, S("abcd")));
    BOOST_TEST(testE(S("abcde"), 4, 2, S("abcd")));
    BOOST_TEST(testE(S("abcde"), 5, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 5, 1, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 6, 0, S("can't happen")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 1, S("bcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 5, S("fghij")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 9, S("j")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 10, S("")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 11, S("")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 1, S("acdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 4, S("afghij")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 8, S("aj")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 9, S("a")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 10, S("a")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 1, S("abcdeghij")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 2, S("abcdehij")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 4, S("abcdej")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 5, S("abcde")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 6, S("abcde")));
    BOOST_TEST(testE(S("abcdefghij"), 9, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 9, 1, S("abcdefghi")));
    BOOST_TEST(testE(S("abcdefghij"), 9, 2, S("abcdefghi")));
    BOOST_TEST(testE(S("abcdefghij"), 10, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 10, 1, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 11, 0, S("can't happen")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 1, S("bcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 10, S("klmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 19, S("t")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 20, S("")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 21, S("")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 1, S("acdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 9, S("aklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 18, S("at")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 19, S("a")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 20, S("a")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 1, S("abcdefghijlmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 5, S("abcdefghijpqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 9, S("abcdefghijt")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 10, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 11, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 19, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 19, 1, S("abcdefghijklmnopqrs")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 19, 2, S("abcdefghijklmnopqrs")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 20, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 20, 1, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 21, 0, S("can't happen")));

    BOOST_TEST(testE(S(""), 0, 0, S("")));
    BOOST_TEST(testE(S(""), 0, 1, S("")));
    BOOST_TEST(testE(S(""), 1, 0, S("can't happen")));
    BOOST_TEST(testE(S("abcde"), 0, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 0, 1, S("bcde")));
    BOOST_TEST(testE(S("abcde"), 0, 2, S("cde")));
    BOOST_TEST(testE(S("abcde"), 0, 4, S("e")));
    BOOST_TEST(testE(S("abcde"), 0, 5, S("")));
    BOOST_TEST(testE(S("abcde"), 0, 6, S("")));
    BOOST_TEST(testE(S("abcde"), 1, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 1, 1, S("acde")));
    BOOST_TEST(testE(S("abcde"), 1, 2, S("ade")));
    BOOST_TEST(testE(S("abcde"), 1, 3, S("ae")));
    BOOST_TEST(testE(S("abcde"), 1, 4, S("a")));
    BOOST_TEST(testE(S("abcde"), 1, 5, S("a")));
    BOOST_TEST(testE(S("abcde"), 2, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 2, 1, S("abde")));
    BOOST_TEST(testE(S("abcde"), 2, 2, S("abe")));
    BOOST_TEST(testE(S("abcde"), 2, 3, S("ab")));
    BOOST_TEST(testE(S("abcde"), 2, 4, S("ab")));
    BOOST_TEST(testE(S("abcde"), 4, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 4, 1, S("abcd")));
    BOOST_TEST(testE(S("abcde"), 4, 2, S("abcd")));
    BOOST_TEST(testE(S("abcde"), 5, 0, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 5, 1, S("abcde")));
    BOOST_TEST(testE(S("abcde"), 6, 0, S("can't happen")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 1, S("bcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 5, S("fghij")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 9, S("j")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 10, S("")));
    BOOST_TEST(testE(S("abcdefghij"), 0, 11, S("")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 1, S("acdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 4, S("afghij")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 8, S("aj")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 9, S("a")));
    BOOST_TEST(testE(S("abcdefghij"), 1, 10, S("a")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 1, S("abcdeghij")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 2, S("abcdehij")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 4, S("abcdej")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 5, S("abcde")));
    BOOST_TEST(testE(S("abcdefghij"), 5, 6, S("abcde")));
    BOOST_TEST(testE(S("abcdefghij"), 9, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 9, 1, S("abcdefghi")));
    BOOST_TEST(testE(S("abcdefghij"), 9, 2, S("abcdefghi")));
    BOOST_TEST(testE(S("abcdefghij"), 10, 0, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 10, 1, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghij"), 11, 0, S("can't happen")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 1, S("bcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 10, S("klmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 19, S("t")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 20, S("")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 0, 21, S("")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 1, S("acdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 9, S("aklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 18, S("at")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 19, S("a")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 1, 20, S("a")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 1, S("abcdefghijlmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 5, S("abcdefghijpqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 9, S("abcdefghijt")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 10, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 10, 11, S("abcdefghij")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 19, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 19, 1, S("abcdefghijklmnopqrs")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 19, 2, S("abcdefghijklmnopqrs")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 20, 0, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 20, 1, S("abcdefghijklmnopqrst")));
    BOOST_TEST(testE(S("abcdefghijklmnopqrst"), 21, 0, S("can't happen")));
}

// done
static
void
testEraseIf()
{
    // erase_if(static_string& str, UnaryPredicate pred)

    {
        static_string<3> s{""};
        BOOST_TEST(erase_if(s, [](char c) { return c == 'a'; }) == 0);
        BOOST_TEST(s == "");
        BOOST_TEST(*s.end() == 0);
    }
    {
        static_string<3> s{"aaa"};
        BOOST_TEST(erase_if(s, [](char c) { return c == 'a'; }) == 3);
        BOOST_TEST(s == "");
        BOOST_TEST(*s.end() == 0);
    }
    {
        static_string<3> s{"abc"};
        BOOST_TEST(erase_if(s, [](char c) { return c == 'a'; }) == 1);
        BOOST_TEST(s == "bc");
        BOOST_TEST(*s.end() == 0);
    }
    {
        static_string<3> s{"abc"};
        BOOST_TEST(erase_if(s, [](char c) { return c == 'b'; }) == 1);
        BOOST_TEST(s == "ac");
        BOOST_TEST(*s.end() == 0);
    }
    {
        static_string<3> s{"abc"};
        BOOST_TEST(erase_if(s, [](char c) { return c == 'c'; }) == 1);
        BOOST_TEST(s == "ab");
        BOOST_TEST(*s.end() == 0);
    }
    {
        static_string<3> s{"abc"};
        BOOST_TEST(erase_if(s, [](char c) { return c == 'd'; }) == 0);
        BOOST_TEST(s == "abc");
        BOOST_TEST(*s.end() == 0);
    }
}

// done
static
void
testPushBack()
{
    // push_back(CharT ch);
    {
        static_string<2> s;
        s.push_back('a');
        BOOST_TEST(s == "a");
        s.push_back('b');
        BOOST_TEST(s == "ab");
        BOOST_TEST_THROWS(s.push_back('c'), std::length_error);
    }

    //---

    {
        static_string<3> s1("12");
        s1.push_back('3');
        BOOST_TEST(s1 == "123");
        BOOST_TEST_THROWS(
            s1.push_back('4'),
            std::length_error);
        static_string<0> s2;
        BOOST_TEST_THROWS(
            s2.push_back('_'),
            std::length_error);
    }
}

// done
static
void
testPopBack()
{
    // pop_back()
    {
        static_string<3> s{"abc"};
        BOOST_TEST(*s.end() == 0);
        s.pop_back();
        BOOST_TEST(s == "ab");
        BOOST_TEST(*s.end() == 0);
        s.pop_back();
        BOOST_TEST(s == "a");
        BOOST_TEST(*s.end() == 0);
        s.pop_back();
        BOOST_TEST(s.empty());
        BOOST_TEST(*s.end() == 0);
    }

    //---

    {
        static_string<3> s1("123");
        s1.pop_back();
        BOOST_TEST(s1 == "12");
        BOOST_TEST(*s1.end() == 0);
        s1.pop_back();
        BOOST_TEST(s1 == "1");
        BOOST_TEST(*s1.end() == 0);
        s1.pop_back();
        BOOST_TEST(s1.empty());
        BOOST_TEST(*s1.end() == 0);
    }
}

// done
static
void
testAppend()
{
  using S = static_string<400>;
  using sv = string_view;

  // append(size_type count, CharT ch)
  BOOST_TEST(static_string<1>{}.append(1, 'a') == "a");
  BOOST_TEST(static_string<2>{}.append(2, 'a') == "aa");
  BOOST_TEST(static_string<2>{"a"}.append(1, 'b') == "ab");
  BOOST_TEST_THROWS(static_string<2>{"ab"}.append(1, 'c'), std::length_error);

  // append(string_view_type sv)
  BOOST_TEST(static_string<3>{"a"}.append(sv{"bc"}) == "abc");
  BOOST_TEST(static_string<3>{"ab"}.append(sv{"c"}) == "abc");
  BOOST_TEST_THROWS(static_string<3>{"abc"}.append(sv{"*"}), std::length_error);

  // append(string_view_type sv, size_type pos, size_type count = npos)
  BOOST_TEST(static_string<3>{"a"}.append(sv{"abc"}, 1) == "abc");
  BOOST_TEST(static_string<3>{"a"}.append(sv{"abc"}, 1, 2) == "abc");
  BOOST_TEST_THROWS(static_string<3>{"abc"}.append(sv{"a"}, 2, 1), std::out_of_range);
  BOOST_TEST_THROWS(static_string<3>{"abc"}.append(sv{"abcd"}, 1, 2), std::length_error);

  // append(CharT const* s, size_type count)
  BOOST_TEST(static_string<3>{"a"}.append("bc", 0) == "a");
  BOOST_TEST(static_string<3>{"a"}.append("bc", 2) == "abc");
  BOOST_TEST_THROWS(static_string<3>{"abc"}.append("bc", 2), std::length_error);

  // append(CharT const* s)
  BOOST_TEST(static_string<3>{"a"}.append("bc") == "abc");
  BOOST_TEST_THROWS(static_string<3>{"abc"}.append("bc"), std::length_error);

  // append(InputIt first, InputIt last)
  {
      static_string<4> const cs{"abcd"};
      static_string<4> s{"ad"};
      BOOST_TEST(static_string<4>{"ab"}.append(
          cs.begin() + 2, cs.begin() + 4) == "abcd");
      BOOST_TEST_THROWS(static_string<2>{"ab"}.append(
          cs.begin() + 2, cs.begin() + 4), std::length_error);
  }

  // append(std::initializer_list<CharT> ilist)
  BOOST_TEST(static_string<4>{"ab"}.append({'c', 'd'}) == "abcd");
  BOOST_TEST_THROWS(static_string<3>{"ab"}.append({'c', 'd'}), std::length_error);

  // append(T const& t)
  {
      struct T
      {
          operator string_view() const noexcept
          {
              return "c";
          }
      };
      BOOST_TEST(static_string<3>{"ab"}.append(T{}) == "abc");
      BOOST_TEST_THROWS(static_string<3>{"abc"}.append(T{}), std::length_error);
  }

  // append(T const& t, size_type pos, size_type count = npos)
  {
      struct T
      {
          operator string_view() const noexcept
          {
              return "abcd";
          }
      };
      BOOST_TEST(static_string<4>{"ab"}.append(T{}, 2) == "abcd");
      BOOST_TEST(static_string<3>{"a"}.append(T{}, 1, 2) == "abc");
      BOOST_TEST_THROWS(static_string<4>{"abc"}.append(T{}, 5), std::out_of_range);
      BOOST_TEST_THROWS(static_string<3>{"abc"}.append(T{}, 3, 1), std::length_error);
  }

  //---

  {
      static_string<3> s1("1");
      s1.append(2, '_');
      BOOST_TEST(s1 == "1__");
      BOOST_TEST(*s1.end() == 0);
      static_string<2> s2("1");
      BOOST_TEST_THROWS(
          (s2.append(2, '_')),
          std::length_error);
  }
  {
      static_string<2> s1("__");
      static_string<3> s2("1");
      s2.append(s1);
      BOOST_TEST(s2 == "1__");
      BOOST_TEST(*s2.end() == 0);
      static_string<2> s3("1");
      BOOST_TEST_THROWS(
          s3.append(s1),
          std::length_error);
  }
  {
      static_string<3> s1("XYZ");
      static_string<4> s2("12");
      s2.append(s1, 1);
      BOOST_TEST(s2 == "12YZ");
      BOOST_TEST(*s2.end() == 0);
      static_string<3> s3("12");
      s3.append(s1, 1, 1);
      BOOST_TEST(s3 == "12Y");
      BOOST_TEST(*s3.end() == 0);
      static_string<3> s4("12");
      BOOST_TEST_THROWS(
          (s4.append(s1, 4)),
          std::out_of_range);
      static_string<3> s5("12");
      BOOST_TEST_THROWS(
          (s5.append(s1, 1, 2)),
          std::length_error);
  }
  {
      static_string<4> s1("12");
      s1.append("XYZ", 2);
      BOOST_TEST(s1 == "12XY");
      BOOST_TEST(*s1.end() == 0);
      static_string<3> s3("12");
      BOOST_TEST_THROWS(
          (s3.append("XYZ", 2)),
          std::length_error);
  }
  {
      static_string<5> s1("12");
      s1.append("XYZ");
      BOOST_TEST(s1 == "12XYZ");
      BOOST_TEST(*s1.end() == 0);
      static_string<4> s2("12");
      BOOST_TEST_THROWS(
          s2.append("XYZ"),
          std::length_error);
  }
  {
      static_string<3> s1("XYZ");
      static_string<5> s2("12");
      s2.append(s1.begin(), s1.end());
      BOOST_TEST(s2 == "12XYZ");
      BOOST_TEST(*s2.end() == 0);
      static_string<4> s3("12");
      BOOST_TEST_THROWS(
          s3.append(s1.begin(), s1.end()),
          std::length_error);
  }
  {
      static_string<5> s1("123");
      s1.append({'X', 'Y'});
      BOOST_TEST(s1 == "123XY");
      BOOST_TEST(*s1.end() == 0);
      static_string<4> s2("123");
      BOOST_TEST_THROWS(
          s2.append({'X', 'Y'}),
          std::length_error);
  }
  {
      string_view s1("XYZ");
      static_string<5> s2("12");
      s2.append(s1);
      BOOST_TEST(s2 == "12XYZ");
      BOOST_TEST(*s2.end() == 0);
      static_string<4> s3("12");
      BOOST_TEST_THROWS(
          s3.append(s1),
          std::length_error);
  }
  {
      static_string<6> s1("123");
      s1.append(std::string("UVX"), 1);
      BOOST_TEST(s1 == "123VX");
      BOOST_TEST(*s1.end() == 0);
      s1.append(std::string("PQR"), 1, 1);
      BOOST_TEST(s1 == "123VXQ");
      BOOST_TEST(*s1.end() == 0);
      static_string<3> s2("123");
      BOOST_TEST_THROWS(
          (s2.append(std::string("PQR"), 1, 1)),
          std::length_error);
  }
  BOOST_TEST(testA(S(), "", 0, S()));
  BOOST_TEST(testA(S(), "12345", 3, S("123")));
  BOOST_TEST(testA(S(), "12345", 4, S("1234")));
  BOOST_TEST(testA(S(), "12345678901234567890", 0, S()));
  BOOST_TEST(testA(S(), "12345678901234567890", 1, S("1")));
  BOOST_TEST(testA(S(), "12345678901234567890", 3, S("123")));
  BOOST_TEST(testA(S(), "12345678901234567890", 20, S("12345678901234567890")));

  BOOST_TEST(testA(S("12345"), "", 0, S("12345")));
  BOOST_TEST(testA(S("12345"), "12345", 5, S("1234512345")));
  BOOST_TEST(testA(S("12345"), "1234567890", 10, S("123451234567890")));

  BOOST_TEST(testA(S("12345678901234567890"), "", 0, S("12345678901234567890")));
  BOOST_TEST(testA(S("12345678901234567890"), "12345", 5, S("1234567890123456789012345")));
  BOOST_TEST(testA(S("12345678901234567890"), "12345678901234567890", 20,
              S("1234567890123456789012345678901234567890")));

  S s_short = "123/";

  s_short.append(s_short.data(), s_short.size());
  BOOST_TEST(s_short == "123/123/");
  s_short.append(s_short.data(), s_short.size());
  BOOST_TEST(s_short == "123/123/123/123/");
  s_short.append(s_short.data(), s_short.size());
  BOOST_TEST(s_short == "123/123/123/123/123/123/123/123/");
}

// done
static
void
testPlusEquals()
{
    using sv = string_view;

    // operator+=(CharT ch)
    BOOST_TEST((static_string<3>{"ab"} += 'c') == "abc");
    BOOST_TEST_THROWS((static_string<3>{"abc"} += '*'), std::length_error);

    // operator+=(CharT const* s)
    BOOST_TEST((static_string<3>{"a"} += "bc") == "abc");
    BOOST_TEST_THROWS((static_string<3>{"abc"} += "*"), std::length_error);

    // operator+=(std::initializer_list<CharT> init)
    BOOST_TEST((static_string<3>{"a"} += {'b', 'c'}) == "abc");
    BOOST_TEST_THROWS((static_string<3>{"abc"} += {'*', '*'}), std::length_error);

    // operator+=(string_view_type const& s)
    BOOST_TEST((static_string<3>{"a"} += sv{"bc"}) == "abc");
    BOOST_TEST_THROWS((static_string<3>{"abc"} += sv{"*"}), std::length_error);

    //---

    {
        static_string<2> s1("__");
        static_string<3> s2("1");
        s2 += s1;
        BOOST_TEST(s2 == "1__");
        BOOST_TEST(*s2.end() == 0);
        static_string<2> s3("1");
        BOOST_TEST_THROWS(
            s3 += s1,
            std::length_error);
    }
    {
        static_string<3> s1("12");
        s1 += '3';
        BOOST_TEST(s1 == "123");
        BOOST_TEST_THROWS(
            s1 += '4',
            std::length_error);
    }
    {
        static_string<4> s1("12");
        s1 += "34";
        BOOST_TEST(s1 == "1234");
        BOOST_TEST_THROWS(
            s1 += "5",
            std::length_error);
    }
    {
        static_string<4> s1("12");
        s1 += {'3', '4'};
        BOOST_TEST(s1 == "1234");
        BOOST_TEST_THROWS(
            (s1 += {'5'}),
            std::length_error);
    }
    {
        string_view s1("34");
        static_string<4> s2("12");
        s2 += s1;
        BOOST_TEST(s2 == "1234");
        BOOST_TEST_THROWS(
            s2 += s1,
            std::length_error);
    }
}

// done
void
testCompare()
{
    using str1 = static_string<1>;
    using str2 = static_string<2>;
    {
        str1 s1;
        str2 s2;
        s1 = "1";
        s2 = "22";
        BOOST_TEST(s1.compare(s2) < 0);
        BOOST_TEST(s2.compare(s1) > 0);

        BOOST_TEST(s1.compare(0, 1, s2) < 0);
        BOOST_TEST(s2.compare(0, 2, s1) > 0);

        BOOST_TEST(s1.compare(0, 2, s2, 0, 1) < 0);
        BOOST_TEST(s2.compare(0, 1, s1, 0, 2) > 0);

        BOOST_TEST(s1.compare(s2.data()) < 0);
        BOOST_TEST(s2.compare(s1.data()) > 0);

        BOOST_TEST(s1.compare(0, 2, s2.data()) < 0);
        BOOST_TEST(s2.compare(0, 1, s1.data()) > 0);

        BOOST_TEST(s1.compare(s2.subview()) < 0);
        BOOST_TEST(s2.compare(s1.subview()) > 0);

        BOOST_TEST(s1.compare(0, 2, s2.subview()) < 0);
        BOOST_TEST(s2.compare(0, 1, s1.subview()) > 0);

        BOOST_TEST(s1.compare(0, 2, s2.subview(), 0, 1) < 0);
        BOOST_TEST(s2.compare(0, 1, s1.subview(), 0, 2) > 0);

        BOOST_TEST(s1 < "10");
        BOOST_TEST(s2 > "1");
        BOOST_TEST("10" > s1);
        BOOST_TEST("1" < s2);
        BOOST_TEST(s1 < "20");
        BOOST_TEST(s2 > "1");
        BOOST_TEST(s2 > "2");
    }
    {
        str2 s1("x");
        str2 s2("x");
        BOOST_TEST(s1 == s2);
        BOOST_TEST(s1 <= s2);
        BOOST_TEST(s1 >= s2);
        BOOST_TEST(! (s1 < s2));
        BOOST_TEST(! (s1 > s2));
        BOOST_TEST(! (s1 != s2));
    }
    {
        str1 s1("x");
        str2 s2("x");
        BOOST_TEST(s1 == s2);
        BOOST_TEST(s1 <= s2);
        BOOST_TEST(s1 >= s2);
        BOOST_TEST(! (s1 < s2));
        BOOST_TEST(! (s1 > s2));
        BOOST_TEST(! (s1 != s2));
    }
    {
        str2 s("x");
        BOOST_TEST(s == "x");
        BOOST_TEST(s <= "x");
        BOOST_TEST(s >= "x");
        BOOST_TEST(! (s < "x"));
        BOOST_TEST(! (s > "x"));
        BOOST_TEST(! (s != "x"));
        BOOST_TEST("x" == s);
        BOOST_TEST("x" <= s);
        BOOST_TEST("x" >= s);
        BOOST_TEST(! ("x" < s));
        BOOST_TEST(! ("x" > s));
        BOOST_TEST(! ("x" != s));
    }
    {
        str2 s("x");
        BOOST_TEST(s <= "y");
        BOOST_TEST(s < "y");
        BOOST_TEST(s != "y");
        BOOST_TEST(! (s == "y"));
        BOOST_TEST(! (s >= "y"));
        BOOST_TEST(! (s > "x"));
        BOOST_TEST("y" >= s);
        BOOST_TEST("y" > s);
        BOOST_TEST("y" != s);
        BOOST_TEST(! ("y" == s));
        BOOST_TEST(! ("y" <= s));
        BOOST_TEST(! ("y" < s));
    }
    {
        str1 s1("x");
        str2 s2("y");
        BOOST_TEST(s1 <= s2);
        BOOST_TEST(s1 < s2);
        BOOST_TEST(s1 != s2);
        BOOST_TEST(! (s1 == s2));
        BOOST_TEST(! (s1 >= s2));
        BOOST_TEST(! (s1 > s2));
    }
    {
        str1 s1("x");
        str2 s2("xx");
        BOOST_TEST(s1 < s2);
        BOOST_TEST(s2 > s1);
    }
    {
        str1 s1("x");
        str2 s2("yy");
        BOOST_TEST(s1 < s2);
        BOOST_TEST(s2 > s1);
    }

    using S = static_string<400>;
    BOOST_TEST(testC(S(""), 0, 0, "", 0, 0));
    BOOST_TEST(testC(S(""), 0, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S(""), 0, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S(""), 0, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S(""), 0, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S(""), 0, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S(""), 0, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S(""), 0, 1, "", 0, 0));
    BOOST_TEST(testC(S(""), 0, 1, "abcde", 0, 0));
    BOOST_TEST(testC(S(""), 0, 1, "abcde", 1, -1));
    BOOST_TEST(testC(S(""), 0, 1, "abcde", 2, -2));
    BOOST_TEST(testC(S(""), 0, 1, "abcde", 4, -4));
    BOOST_TEST(testC(S(""), 0, 1, "abcde", 5, -5));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S(""), 0, 1, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S(""), 1, 0, "", 0, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcde", 1, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcde", 2, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcde", 4, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcde", 5, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghij", 1, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghij", 5, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghij", 9, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghij", 10, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghijklmnopqrst", 1, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghijklmnopqrst", 10, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghijklmnopqrst", 19, 0));
    BOOST_TEST(testC(S(""), 1, 0, "abcdefghijklmnopqrst", 20, 0));
    BOOST_TEST(testC(S("abcde"), 0, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcde"), 0, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcde"), 0, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcde", 1, 0));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcde", 2, -1));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcde", 4, -3));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcde", 5, -4));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghij", 1, 0));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghij", 5, -4));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghij", 9, -8));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghij", 10, -9));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghijklmnopqrst", 1, 0));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghijklmnopqrst", 10, -9));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghijklmnopqrst", 19, -18));
    BOOST_TEST(testC(S("abcde"), 0, 1, "abcdefghijklmnopqrst", 20, -19));
    BOOST_TEST(testC(S("abcde"), 0, 2, "", 0, 2));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcde", 0, 2));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcde", 2, 0));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcde", 4, -2));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcde", 5, -3));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghij", 0, 2));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghij", 5, -3));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghij", 9, -7));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghij", 10, -8));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghijklmnopqrst", 0, 2));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghijklmnopqrst", 10, -8));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghijklmnopqrst", 19, -17));
    BOOST_TEST(testC(S("abcde"), 0, 2, "abcdefghijklmnopqrst", 20, -18));
    BOOST_TEST(testC(S("abcde"), 0, 4, "", 0, 4));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcde", 0, 4));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcde", 1, 3));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcde", 2, 2));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcde", 4, 0));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcde", 5, -1));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghij", 0, 4));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghij", 1, 3));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghij", 5, -1));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghij", 9, -5));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghij", 10, -6));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghijklmnopqrst", 0, 4));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghijklmnopqrst", 1, 3));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghijklmnopqrst", 10, -6));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghijklmnopqrst", 19, -15));
    BOOST_TEST(testC(S("abcde"), 0, 4, "abcdefghijklmnopqrst", 20, -16));
    BOOST_TEST(testC(S("abcde"), 0, 5, "", 0, 5));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcde", 0, 5));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcde", 1, 4));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcde", 2, 3));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcde", 5, 0));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghij", 0, 5));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghij", 1, 4));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghij", 5, 0));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghij", 9, -4));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghij", 10, -5));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghijklmnopqrst", 0, 5));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghijklmnopqrst", 1, 4));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghijklmnopqrst", 10, -5));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghijklmnopqrst", 19, -14));
    BOOST_TEST(testC(S("abcde"), 0, 5, "abcdefghijklmnopqrst", 20, -15));
    BOOST_TEST(testC(S("abcde"), 0, 6, "", 0, 5));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcde", 0, 5));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcde", 1, 4));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcde", 2, 3));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcde", 5, 0));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghij", 0, 5));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghij", 1, 4));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghij", 5, 0));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghij", 9, -4));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghij", 10, -5));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghijklmnopqrst", 0, 5));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghijklmnopqrst", 1, 4));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghijklmnopqrst", 10, -5));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghijklmnopqrst", 19, -14));
    BOOST_TEST(testC(S("abcde"), 0, 6, "abcdefghijklmnopqrst", 20, -15));
    BOOST_TEST(testC(S("abcde"), 1, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcde"), 1, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcde"), 1, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcde"), 1, 1, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "", 0, 2));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcde", 0, 2));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghij", 0, 2));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghijklmnopqrst", 0, 2));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcde"), 1, 2, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "", 0, 3));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcde", 0, 3));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghij", 0, 3));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghijklmnopqrst", 0, 3));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcde"), 1, 3, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "", 0, 4));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcde", 0, 4));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghij", 0, 4));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghijklmnopqrst", 0, 4));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcde"), 1, 4, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "", 0, 4));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcde", 0, 4));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghij", 0, 4));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghijklmnopqrst", 0, 4));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcde"), 1, 5, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcde"), 2, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcde"), 2, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcde"), 2, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcde", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcde", 2, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcde", 4, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcde", 5, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghij", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghij", 5, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghij", 9, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghij", 10, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghijklmnopqrst", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghijklmnopqrst", 10, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghijklmnopqrst", 19, 2));
    BOOST_TEST(testC(S("abcde"), 2, 1, "abcdefghijklmnopqrst", 20, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "", 0, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcde", 0, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcde", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcde", 2, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcde", 4, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcde", 5, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghij", 0, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghij", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghij", 5, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghij", 9, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghij", 10, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghijklmnopqrst", 0, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghijklmnopqrst", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghijklmnopqrst", 10, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghijklmnopqrst", 19, 2));
    BOOST_TEST(testC(S("abcde"), 2, 2, "abcdefghijklmnopqrst", 20, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "", 0, 3));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcde", 0, 3));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcde", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcde", 2, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcde", 4, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcde", 5, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghij", 0, 3));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghij", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghij", 5, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghij", 9, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghij", 10, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghijklmnopqrst", 0, 3));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghijklmnopqrst", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghijklmnopqrst", 10, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghijklmnopqrst", 19, 2));
    BOOST_TEST(testC(S("abcde"), 2, 3, "abcdefghijklmnopqrst", 20, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "", 0, 3));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcde", 0, 3));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcde", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcde", 2, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcde", 4, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcde", 5, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghij", 0, 3));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghij", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghij", 5, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghij", 9, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghij", 10, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghijklmnopqrst", 0, 3));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghijklmnopqrst", 1, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghijklmnopqrst", 10, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghijklmnopqrst", 19, 2));
    BOOST_TEST(testC(S("abcde"), 2, 4, "abcdefghijklmnopqrst", 20, 2));
    BOOST_TEST(testC(S("abcde"), 4, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcde"), 4, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcde"), 4, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcde", 1, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcde", 2, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcde", 4, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcde", 5, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghij", 1, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghij", 5, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghij", 9, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghij", 10, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghijklmnopqrst", 1, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghijklmnopqrst", 10, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghijklmnopqrst", 19, 4));
    BOOST_TEST(testC(S("abcde"), 4, 1, "abcdefghijklmnopqrst", 20, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "", 0, 1));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcde", 1, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcde", 2, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcde", 4, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcde", 5, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghij", 1, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghij", 5, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghij", 9, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghij", 10, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghijklmnopqrst", 1, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghijklmnopqrst", 10, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghijklmnopqrst", 19, 4));
    BOOST_TEST(testC(S("abcde"), 4, 2, "abcdefghijklmnopqrst", 20, 4));
    BOOST_TEST(testC(S("abcde"), 5, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcde"), 5, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcde"), 5, 1, "", 0, 0));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcde"), 5, 1, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcde"), 6, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcde", 1, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcde", 2, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcde", 4, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcde", 5, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghij", 1, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghij", 5, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghij", 9, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghij", 10, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghijklmnopqrst", 1, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghijklmnopqrst", 10, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghijklmnopqrst", 19, 0));
    BOOST_TEST(testC(S("abcde"), 6, 0, "abcdefghijklmnopqrst", 20, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghij"), 0, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcde", 1, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcde", 2, -1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcde", 4, -3));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcde", 5, -4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghij", 1, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghij", 5, -4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghij", 9, -8));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghij", 10, -9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghijklmnopqrst", 1, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghijklmnopqrst", 10, -9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghijklmnopqrst", 19, -18));
    BOOST_TEST(testC(S("abcdefghij"), 0, 1, "abcdefghijklmnopqrst", 20, -19));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcde", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcde", 1, 4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcde", 2, 3));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcde", 5, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghij", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghij", 1, 4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghij", 5, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghij", 9, -4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghij", 10, -5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghijklmnopqrst", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghijklmnopqrst", 1, 4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghijklmnopqrst", 10, -5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghijklmnopqrst", 19, -14));
    BOOST_TEST(testC(S("abcdefghij"), 0, 5, "abcdefghijklmnopqrst", 20, -15));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcde", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcde", 1, 8));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcde", 2, 7));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcde", 4, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcde", 5, 4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghij", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghij", 1, 8));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghij", 5, 4));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghij", 9, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghij", 10, -1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghijklmnopqrst", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghijklmnopqrst", 1, 8));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghijklmnopqrst", 10, -1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghijklmnopqrst", 19, -10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 9, "abcdefghijklmnopqrst", 20, -11));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "", 0, 10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcde", 0, 10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcde", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcde", 2, 8));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcde", 4, 6));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcde", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghij", 0, 10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghij", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghij", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghij", 10, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghijklmnopqrst", 0, 10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghijklmnopqrst", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghijklmnopqrst", 10, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghijklmnopqrst", 19, -9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 10, "abcdefghijklmnopqrst", 20, -10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "", 0, 10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcde", 0, 10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcde", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcde", 2, 8));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcde", 4, 6));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcde", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghij", 0, 10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghij", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghij", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghij", 10, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghijklmnopqrst", 0, 10));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghijklmnopqrst", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghijklmnopqrst", 10, 0));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghijklmnopqrst", 19, -9));
    BOOST_TEST(testC(S("abcdefghij"), 0, 11, "abcdefghijklmnopqrst", 20, -10));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghij"), 1, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 1, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "", 0, 4));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcde", 0, 4));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghij", 0, 4));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghijklmnopqrst", 0, 4));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 4, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "", 0, 8));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcde", 0, 8));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghij", 0, 8));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghijklmnopqrst", 0, 8));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 8, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcde", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghij", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghijklmnopqrst", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 9, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcde", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghij", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghijklmnopqrst", 0, 9));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghij"), 1, 10, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghij"), 5, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcde", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcde", 2, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcde", 4, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcde", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghij", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghij", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghij", 9, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghij", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghijklmnopqrst", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghijklmnopqrst", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghijklmnopqrst", 19, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 1, "abcdefghijklmnopqrst", 20, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "", 0, 2));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcde", 0, 2));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcde", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcde", 2, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcde", 4, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcde", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghij", 0, 2));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghij", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghij", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghij", 9, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghij", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghijklmnopqrst", 0, 2));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghijklmnopqrst", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghijklmnopqrst", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghijklmnopqrst", 19, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 2, "abcdefghijklmnopqrst", 20, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "", 0, 4));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcde", 0, 4));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcde", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcde", 2, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcde", 4, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcde", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghij", 0, 4));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghij", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghij", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghij", 9, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghij", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghijklmnopqrst", 0, 4));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghijklmnopqrst", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghijklmnopqrst", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghijklmnopqrst", 19, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 4, "abcdefghijklmnopqrst", 20, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcde", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcde", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcde", 2, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcde", 4, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcde", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghij", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghij", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghij", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghij", 9, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghij", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghijklmnopqrst", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghijklmnopqrst", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghijklmnopqrst", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghijklmnopqrst", 19, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 5, "abcdefghijklmnopqrst", 20, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcde", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcde", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcde", 2, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcde", 4, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcde", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghij", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghij", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghij", 5, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghij", 9, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghij", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghijklmnopqrst", 0, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghijklmnopqrst", 1, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghijklmnopqrst", 10, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghijklmnopqrst", 19, 5));
    BOOST_TEST(testC(S("abcdefghij"), 5, 6, "abcdefghijklmnopqrst", 20, 5));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghij"), 9, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcde", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcde", 2, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcde", 4, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcde", 5, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghij", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghij", 5, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghij", 9, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghij", 10, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghijklmnopqrst", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghijklmnopqrst", 10, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghijklmnopqrst", 19, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 1, "abcdefghijklmnopqrst", 20, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcde", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcde", 2, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcde", 4, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcde", 5, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghij", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghij", 5, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghij", 9, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghij", 10, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghijklmnopqrst", 1, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghijklmnopqrst", 10, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghijklmnopqrst", 19, 9));
    BOOST_TEST(testC(S("abcdefghij"), 9, 2, "abcdefghijklmnopqrst", 20, 9));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghij"), 10, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghij"), 10, 1, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcde", 1, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcde", 2, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcde", 4, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcde", 5, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghij", 1, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghij", 5, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghij", 9, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghij", 10, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghijklmnopqrst", 1, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghijklmnopqrst", 10, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghijklmnopqrst", 19, 0));
    BOOST_TEST(testC(S("abcdefghij"), 11, 0, "abcdefghijklmnopqrst", 20, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcde", 1, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcde", 2, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcde", 4, -3));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcde", 5, -4));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghij", 1, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghij", 5, -4));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghij", 9, -8));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghij", 10, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghijklmnopqrst", 1, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghijklmnopqrst", 10, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghijklmnopqrst", 19, -18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 1, "abcdefghijklmnopqrst", 20, -19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcde", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcde", 1, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcde", 2, 8));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcde", 4, 6));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcde", 5, 5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghij", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghij", 1, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghij", 5, 5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghij", 10, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghijklmnopqrst", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghijklmnopqrst", 1, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghijklmnopqrst", 10, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghijklmnopqrst", 19, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 10, "abcdefghijklmnopqrst", 20, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcde", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcde", 1, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcde", 2, 17));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcde", 4, 15));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcde", 5, 14));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghij", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghij", 1, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghij", 5, 14));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghij", 9, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghij", 10, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghijklmnopqrst", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghijklmnopqrst", 1, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghijklmnopqrst", 10, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghijklmnopqrst", 19, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 19, "abcdefghijklmnopqrst", 20, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "", 0, 20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcde", 0, 20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcde", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcde", 2, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcde", 4, 16));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcde", 5, 15));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghij", 0, 20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghij", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghij", 5, 15));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghij", 9, 11));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghij", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghijklmnopqrst", 0, 20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghijklmnopqrst", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghijklmnopqrst", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 20, "abcdefghijklmnopqrst", 20, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "", 0, 20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcde", 0, 20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcde", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcde", 2, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcde", 4, 16));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcde", 5, 15));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghij", 0, 20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghij", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghij", 5, 15));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghij", 9, 11));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghij", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghijklmnopqrst", 0, 20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghijklmnopqrst", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghijklmnopqrst", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 0, 21, "abcdefghijklmnopqrst", 20, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 1, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "", 0, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcde", 0, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghij", 0, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghijklmnopqrst", 0, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 9, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "", 0, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcde", 0, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghij", 0, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghijklmnopqrst", 0, 18));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 18, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcde", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghij", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghijklmnopqrst", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 19, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcde", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcde", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcde", 2, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcde", 4, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcde", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghij", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghij", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghij", 5, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghij", 9, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghij", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghijklmnopqrst", 0, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghijklmnopqrst", 1, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghijklmnopqrst", 10, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghijklmnopqrst", 19, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 1, 20, "abcdefghijklmnopqrst", 20, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcde", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcde", 2, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcde", 4, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcde", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghij", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghij", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghij", 9, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghij", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghijklmnopqrst", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghijklmnopqrst", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghijklmnopqrst", 19, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 1, "abcdefghijklmnopqrst", 20, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "", 0, 5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcde", 0, 5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcde", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcde", 2, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcde", 4, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcde", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghij", 0, 5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghij", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghij", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghij", 9, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghij", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghijklmnopqrst", 0, 5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghijklmnopqrst", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghijklmnopqrst", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghijklmnopqrst", 19, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 5, "abcdefghijklmnopqrst", 20, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "", 0, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcde", 0, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcde", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcde", 2, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcde", 4, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcde", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghij", 0, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghij", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghij", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghij", 9, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghij", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghijklmnopqrst", 0, 9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghijklmnopqrst", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghijklmnopqrst", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghijklmnopqrst", 19, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 9, "abcdefghijklmnopqrst", 20, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcde", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcde", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcde", 2, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcde", 4, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcde", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghij", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghij", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghij", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghij", 9, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghij", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghijklmnopqrst", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghijklmnopqrst", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghijklmnopqrst", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghijklmnopqrst", 19, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 10, "abcdefghijklmnopqrst", 20, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcde", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcde", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcde", 2, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcde", 4, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcde", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghij", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghij", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghij", 5, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghij", 9, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghij", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghijklmnopqrst", 0, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghijklmnopqrst", 1, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghijklmnopqrst", 10, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghijklmnopqrst", 19, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 10, 11, "abcdefghijklmnopqrst", 20, 10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcde", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcde", 2, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcde", 4, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcde", 5, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghij", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghij", 5, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghij", 9, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghij", 10, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghijklmnopqrst", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghijklmnopqrst", 10, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghijklmnopqrst", 19, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 1, "abcdefghijklmnopqrst", 20, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcde", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcde", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcde", 2, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcde", 4, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcde", 5, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghij", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghij", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghij", 5, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghij", 9, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghij", 10, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghijklmnopqrst", 0, 1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghijklmnopqrst", 1, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghijklmnopqrst", 10, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghijklmnopqrst", 19, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 19, 2, "abcdefghijklmnopqrst", 20, 19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 0, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcde", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcde", 2, -2));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcde", 4, -4));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcde", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghij", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghij", 5, -5));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghij", 9, -9));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghij", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghijklmnopqrst", 1, -1));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghijklmnopqrst", 10, -10));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghijklmnopqrst", 19, -19));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 20, 1, "abcdefghijklmnopqrst", 20, -20));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcde", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcde", 1, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcde", 2, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcde", 4, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcde", 5, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghij", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghij", 1, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghij", 5, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghij", 9, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghij", 10, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghijklmnopqrst", 0, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghijklmnopqrst", 1, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghijklmnopqrst", 10, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghijklmnopqrst", 19, 0));
    BOOST_TEST(testC(S("abcdefghijklmnopqrst"), 21, 0, "abcdefghijklmnopqrst", 20, 0));
}

// done
void
testSwap()
{
    {
        static_string<3> s1("123");
        static_string<3> s2("XYZ");
        swap(s1, s2);
        BOOST_TEST(s1 == "XYZ");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST(s2 == "123");
        BOOST_TEST(*s2.end() == 0);
        static_string<3> s3("UV");
        swap(s2, s3);
        BOOST_TEST(s2 == "UV");
        BOOST_TEST(*s2.end() == 0);
        BOOST_TEST(s3 == "123");
        BOOST_TEST(*s3.end() == 0);
    }
    {
        static_string<5> s1("123");
        static_string<7> s2("XYZ");
        swap(s1, s2);
        BOOST_TEST(s1 == "XYZ");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST(s2 == "123");
        BOOST_TEST(*s2.end() == 0);
        static_string<3> s3("UV");
        swap(s2, s3);
        BOOST_TEST(s2 == "UV");
        BOOST_TEST(*s2.end() == 0);
        BOOST_TEST(s3 == "123");
        BOOST_TEST(*s3.end() == 0);
        {
            static_string<5> s4("12345");
            static_string<3> s5("XYZ");
            BOOST_TEST_THROWS(
                (swap(s4, s5)),
                std::length_error);
        }
        {
            static_string<3> s4("XYZ");
            static_string<5> s5("12345");
            BOOST_TEST_THROWS(
                (swap(s4, s5)),
                std::length_error);
        }
    }
}

void
testGeneral()
{
    using str1 = static_string<1>;
    using str2 = static_string<2>;
    {
        str1 s1;
        BOOST_TEST(s1 == "");
        BOOST_TEST(s1.empty());
        BOOST_TEST(s1.size() == 0);
        BOOST_TEST(s1.max_size() == 1);
        BOOST_TEST(s1.capacity() == 1);
        BOOST_TEST(s1.begin() == s1.end());
        BOOST_TEST(s1.cbegin() == s1.cend());
        BOOST_TEST(s1.rbegin() == s1.rend());
        BOOST_TEST(s1.crbegin() == s1.crend());
        BOOST_TEST_THROWS(
            s1.at(0),
            std::out_of_range);
        BOOST_TEST(s1.data()[0] == 0);
        BOOST_TEST(*s1.c_str() == 0);
        BOOST_TEST(std::distance(s1.begin(), s1.end()) == 0);
        BOOST_TEST(std::distance(s1.cbegin(), s1.cend()) == 0);
        BOOST_TEST(std::distance(s1.rbegin(), s1.rend()) == 0);
        BOOST_TEST(std::distance(s1.crbegin(), s1.crend()) == 0);
        BOOST_TEST(s1.compare(s1) == 0);
    }
    {
        str1 const s1;
        BOOST_TEST(s1 == "");
        BOOST_TEST(s1.empty());
        BOOST_TEST(s1.size() == 0);
        BOOST_TEST(s1.max_size() == 1);
        BOOST_TEST(s1.capacity() == 1);
        BOOST_TEST(s1.begin() == s1.end());
        BOOST_TEST(s1.cbegin() == s1.cend());
        BOOST_TEST(s1.rbegin() == s1.rend());
        BOOST_TEST(s1.crbegin() == s1.crend());
        BOOST_TEST_THROWS(
            s1.at(0),
            std::out_of_range);
        BOOST_TEST(s1.data()[0] == 0);
        BOOST_TEST(*s1.c_str() == 0);
        BOOST_TEST(std::distance(s1.begin(), s1.end()) == 0);
        BOOST_TEST(std::distance(s1.cbegin(), s1.cend()) == 0);
        BOOST_TEST(std::distance(s1.rbegin(), s1.rend()) == 0);
        BOOST_TEST(std::distance(s1.crbegin(), s1.crend()) == 0);
        BOOST_TEST(s1.compare(s1) == 0);
    }
    {
        str1 s1;
        str1 s2("x");
        BOOST_TEST(s2 == "x");
        BOOST_TEST(s2[0] == 'x');
        BOOST_TEST(s2.at(0) == 'x');
        BOOST_TEST(s2.front() == 'x');
        BOOST_TEST(s2.back() == 'x');
        str1 const s3(s2);
        BOOST_TEST(s3 == "x");
        BOOST_TEST(s3[0] == 'x');
        BOOST_TEST(s3.at(0) == 'x');
        BOOST_TEST(s3.front() == 'x');
        BOOST_TEST(s3.back() == 'x');
        s2 = "y";
        BOOST_TEST(s2 == "y");
        BOOST_TEST(s3 == "x");
        s1 = s2;
        BOOST_TEST(s1 == "y");
        s1.clear();
        BOOST_TEST(s1.empty());
        BOOST_TEST(s1.size() == 0);
    }
    {
        str2 s1("x");
        str1 s2(s1);
        BOOST_TEST(s2 == "x");
        str1 s3;
        s3 = s2;
        BOOST_TEST(s3 == "x");
        s1 = "xy";
        BOOST_TEST(s1.size() == 2);
        BOOST_TEST(s1[0] == 'x');
        BOOST_TEST(s1[1] == 'y');
        BOOST_TEST(s1.at(0) == 'x');
        BOOST_TEST(s1.at(1) == 'y');
        BOOST_TEST(s1.front() == 'x');
        BOOST_TEST(s1.back() == 'y');
        auto const s4 = s1;
        BOOST_TEST(s4[0] == 'x');
        BOOST_TEST(s4[1] == 'y');
        BOOST_TEST(s4.at(0) == 'x');
        BOOST_TEST(s4.at(1) == 'y');
        BOOST_TEST(s4.front() == 'x');
        BOOST_TEST(s4.back() == 'y');
        BOOST_TEST_THROWS(
            s3 = s1,
            std::length_error);
        BOOST_TEST_THROWS(
            str1{s1},
            std::length_error);
    }
    {
        str1 s1("x");
        str2 s2;
        s2 = s1;
        BOOST_TEST_THROWS(
            s1.resize(2),
            std::length_error);
    }
    // copy
    {
      {
        static_string<20> str = "helloworld";
        char arr[20]{};
        BOOST_TEST(str.copy(arr, str.size()) ==
          str.size());
        BOOST_TEST(str == arr);
        BOOST_TEST_THROWS(
          str.copy(arr, str.size(), str.size() + 1),
          std::out_of_range);
      }
      {
        static_string<20> str = "helloworld";
        char arr[20]{};
        BOOST_TEST(str.copy(arr, 2, 2) == 2);
        BOOST_TEST(arr[0] == 'l' && arr[1] == 'l');
      }
    }
}

// done
void
testToStaticString()
{
    BOOST_TEST(testTS(0, "0", L"0", true));
    BOOST_TEST(testTS(0u, "0", L"0", true));
    BOOST_TEST(testTS(0xffff, "65535", L"65535", true));
    BOOST_TEST(testTS(0x10000, "65536", L"65536", true));
    BOOST_TEST(testTS(0xffffffff, "4294967295", L"4294967295", true));
    BOOST_TEST(testTS(-65535, "-65535", L"-65535", true));
    BOOST_TEST(testTS(-65536, "-65536", L"-65536", true));
    BOOST_TEST(testTS(-4294967295ll, "-4294967295", L"-4294967295", true));
    BOOST_TEST(testTS(1, "1", L"1", true));
    BOOST_TEST(testTS(-1, "-1", L"-1", true));
    BOOST_TEST(testTS(0.1));
    BOOST_TEST(testTS(0.0000001));
    BOOST_TEST(testTS(-0.0000001));
    BOOST_TEST(testTS(-0.1));
    BOOST_TEST(testTS(1234567890.0001));
    BOOST_TEST(testTS(1.123456789012345));
    BOOST_TEST(testTS(-1234567890.1234));
    BOOST_TEST(testTS(-1.123456789012345));

    BOOST_TEST(testTS(std::numeric_limits<long long>::max()));
    BOOST_TEST(testTS(std::numeric_limits<long long>::min()));
    BOOST_TEST(testTS(std::numeric_limits<unsigned long long>::max()));
    BOOST_TEST(testTS(std::numeric_limits<unsigned long long>::max()));
    BOOST_TEST(testTS(std::numeric_limits<long double>::min()));
    BOOST_TEST(testTS(std::numeric_limits<float>::min()));
    
    // these tests technically are not portable, but they will work
    // 99% of the time.
    {
      auto str = to_static_string(std::numeric_limits<float>::max());
      BOOST_TEST(str.find('e') != static_string<0>::npos || str.find('.') !=
        static_string<0>::npos || str == "infinity" || str == "inf");
    }
    {
      auto str = to_static_string(std::numeric_limits<double>::max());
      BOOST_TEST(str.find('e') != static_string<0>::npos || str.find('.') !=
        static_string<0>::npos || str == "infinity" || str == "inf");
    }
    {
      auto str = to_static_string(std::numeric_limits<long double>::max());
      BOOST_TEST(str.find('e') != static_string<0>::npos || str.find('.') !=
        static_string<0>::npos || str == "infinity" || str == "inf");
    }
    {
      auto str = to_static_wstring(std::numeric_limits<float>::max());
      BOOST_TEST(str.find('e') != static_string<0>::npos || str.find('.') !=
        static_string<0>::npos || str == L"infinity" || str == L"inf");
    }
    {
      auto str = to_static_wstring(std::numeric_limits<double>::max());
      BOOST_TEST(str.find('e') != static_string<0>::npos || str.find('.') !=
        static_string<0>::npos || str == L"infinity" || str == L"inf");
    }
    {
      auto str = to_static_wstring(std::numeric_limits<long double>::max());
      BOOST_TEST(str.find('e') != static_string<0>::npos || str.find('.') !=
        static_string<0>::npos || str == L"infinity" || str == L"inf");
    }
}

// done
void
testFind()
{
  const char* cs1 = "12345";
  const char* cs2 = "2345";
  string_view v1 = cs1;
  string_view v2 = cs2;
  static_string<5> fs1 = cs1;
  static_string<4> fs2 = cs2;
  using S = static_string<400>;


  // find
  BOOST_TEST(fs1.find(v1) == 0);
  BOOST_TEST(fs1.find(v2) == 1);
  BOOST_TEST(fs1.find(fs1) == 0);
  BOOST_TEST(fs1.find(fs2) == 1);

  BOOST_TEST(fs1.find(cs1) == 0);
  BOOST_TEST(fs1.find(cs2) == 1);

  BOOST_TEST(fs1.find(cs1, 0) == 0);
  BOOST_TEST(fs1.find(cs2, 0) == 1);

  BOOST_TEST(fs1.find(cs2, 0, 2) == 1);

  BOOST_TEST(fs1.find(cs1, 4) == S::npos);
  BOOST_TEST(fs1.find(cs2, 4) == S::npos);

  BOOST_TEST(fs1.find('1') == 0);
  BOOST_TEST(fs1.find('1', 4) == S::npos);

  BOOST_TEST(testF(S(""), "", 0, 0, 0));
  BOOST_TEST(testF(S(""), "abcde", 0, 0, 0));
  BOOST_TEST(testF(S(""), "abcde", 0, 1, S::npos));
  BOOST_TEST(testF(S(""), "abcde", 0, 2, S::npos));
  BOOST_TEST(testF(S(""), "abcde", 0, 4, S::npos));
  BOOST_TEST(testF(S(""), "abcde", 0, 5, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 0, 0, 0));
  BOOST_TEST(testF(S(""), "abcdeabcde", 0, 1, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 0, 5, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 0, 9, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 0, 10, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 0, 0, 0));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 0, 1, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 0, 10, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 0, 19, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 0, 20, S::npos));
  BOOST_TEST(testF(S(""), "", 1, 0, S::npos));
  BOOST_TEST(testF(S(""), "abcde", 1, 0, S::npos));
  BOOST_TEST(testF(S(""), "abcde", 1, 1, S::npos));
  BOOST_TEST(testF(S(""), "abcde", 1, 2, S::npos));
  BOOST_TEST(testF(S(""), "abcde", 1, 4, S::npos));
  BOOST_TEST(testF(S(""), "abcde", 1, 5, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 1, 0, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 1, 1, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 1, 5, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 1, 9, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 1, 0, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 1, 1, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 1, 19, S::npos));
  BOOST_TEST(testF(S(""), "abcdeabcdeabcdeabcde", 1, 20, S::npos));
  BOOST_TEST(testF(S("abcde"), "", 0, 0, 0));
  BOOST_TEST(testF(S("abcde"), "abcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcde"), "abcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcde"), "abcde", 0, 2, 0));
  BOOST_TEST(testF(S("abcde"), "abcde", 0, 4, 0));
  BOOST_TEST(testF(S("abcde"), "abcde", 0, 5, 0));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 0, 5, 0));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 0, 9, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 0, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 19, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 20, S::npos));
  BOOST_TEST(testF(S("abcde"), "", 1, 0, 1));
  BOOST_TEST(testF(S("abcde"), "abcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcde"), "abcde", 1, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 1, 2, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 1, 4, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 1, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 1, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 1, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 1, 9, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 19, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 20, S::npos));
  BOOST_TEST(testF(S("abcde"), "", 2, 0, 2));
  BOOST_TEST(testF(S("abcde"), "abcde", 2, 0, 2));
  BOOST_TEST(testF(S("abcde"), "abcde", 2, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 2, 2, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 2, 4, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 2, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 2, 0, 2));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 2, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 2, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 2, 9, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 2, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 0, 2));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 19, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 20, S::npos));
  BOOST_TEST(testF(S("abcde"), "", 4, 0, 4));
  BOOST_TEST(testF(S("abcde"), "abcde", 4, 0, 4));
  BOOST_TEST(testF(S("abcde"), "abcde", 4, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 4, 2, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 4, 4, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 4, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 4, 0, 4));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 4, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 4, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 4, 9, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 4, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 0, 4));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 19, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 20, S::npos));
  BOOST_TEST(testF(S("abcde"), "", 5, 0, 5));
  BOOST_TEST(testF(S("abcde"), "abcde", 5, 0, 5));
  BOOST_TEST(testF(S("abcde"), "abcde", 5, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 5, 2, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 5, 4, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 5, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 5, 0, 5));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 5, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 5, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 5, 9, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 5, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 0, 5));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 19, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 20, S::npos));
  BOOST_TEST(testF(S("abcde"), "", 6, 0, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 6, 0, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 6, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 6, 2, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 6, 4, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcde", 6, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 6, 0, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 6, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 6, 5, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 6, 9, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcde", 6, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 0, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 1, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 10, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 19, S::npos));
  BOOST_TEST(testF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "", 0, 0, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 0, 2, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 0, 4, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 0, 5, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 0, 5, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 0, 9, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 0, 10, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 10, 0));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "", 1, 0, 1));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 1, 1, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 1, 2, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 1, 4, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 1, 5, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 1, 1, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 1, 5, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 1, 9, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 1, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "", 5, 0, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 5, 0, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 5, 1, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 5, 2, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 5, 4, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 5, 5, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 5, 0, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 5, 1, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 5, 5, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 5, 9, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 5, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 0, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 1, 5));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "", 9, 0, 9));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 9, 0, 9));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 9, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 9, 2, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 9, 4, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 9, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 9, 0, 9));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 9, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 9, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 9, 9, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 9, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 0, 9));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "", 10, 0, 10));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 10, 0, 10));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 10, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 10, 2, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 10, 4, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 10, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 10, 0, 10));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 10, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 10, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 10, 9, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 10, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 0, 10));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "", 11, 0, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 11, 0, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 11, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 11, 2, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 11, 4, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcde", 11, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 11, 0, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 11, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 11, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 11, 9, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcde", 11, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 0, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "", 0, 0, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 2, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 4, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 5, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 5, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 9, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 10, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 0, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 1, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 10, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 19, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 20, 0));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "", 1, 0, 1));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 1, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 2, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 4, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 5, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 1, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 5, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 9, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 10, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 0, 1));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 1, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 10, 5));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "", 10, 0, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 0, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 1, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 2, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 4, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 5, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 0, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 1, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 5, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 9, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 10, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 0, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 1, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 10, 10));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "", 19, 0, 19));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 0, 19));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 2, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 4, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 0, 19));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 9, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 0, 19));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 19, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 20, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "", 20, 0, 20));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 0, 20));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 2, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 4, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 0, 20));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 1, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 5, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 9, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 10, S::npos));
  BOOST_TEST(testF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 20, 0, 20));

  // rfind

  BOOST_TEST(fs1.rfind(v1) == 0);
  BOOST_TEST(fs1.rfind(v2) == 1);

  BOOST_TEST(fs1.rfind(fs1) == 0);
  BOOST_TEST(fs1.rfind(fs2) == 1);

  BOOST_TEST(fs1.rfind(cs1) == 0);
  BOOST_TEST(fs1.rfind(cs2) == 1);

  BOOST_TEST(fs1.rfind(cs1, 0) == 0);
  BOOST_TEST(fs1.rfind(cs2, 0) == S::npos);
  
  BOOST_TEST(fs1.rfind(cs2, 0, 2) == S::npos);
  BOOST_TEST(fs1.rfind(cs1, 4) == 0);

  BOOST_TEST(fs1.rfind('1') == 0);
  BOOST_TEST(fs1.rfind('1', 4) == 0);


  BOOST_TEST(testRF(S(""), "", 0, 0, 0));
  BOOST_TEST(testRF(S(""), "abcde", 0, 0, 0));
  BOOST_TEST(testRF(S(""), "abcde", 0, 1, S::npos));
  BOOST_TEST(testRF(S(""), "abcde", 0, 2, S::npos));
  BOOST_TEST(testRF(S(""), "abcde", 0, 4, S::npos));
  BOOST_TEST(testRF(S(""), "abcde", 0, 5, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 0, 0, 0));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 0, 1, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 0, 5, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 0, 9, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 0, 10, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 0, 0, 0));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 0, 1, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 0, 10, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 0, 19, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 0, 20, S::npos));
  BOOST_TEST(testRF(S(""), "", 1, 0, 0));
  BOOST_TEST(testRF(S(""), "abcde", 1, 0, 0));
  BOOST_TEST(testRF(S(""), "abcde", 1, 1, S::npos));
  BOOST_TEST(testRF(S(""), "abcde", 1, 2, S::npos));
  BOOST_TEST(testRF(S(""), "abcde", 1, 4, S::npos));
  BOOST_TEST(testRF(S(""), "abcde", 1, 5, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 1, 0, 0));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 1, 1, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 1, 5, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 1, 9, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 1, 0, 0));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 1, 1, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 1, 19, S::npos));
  BOOST_TEST(testRF(S(""), "abcdeabcdeabcdeabcde", 1, 20, S::npos));
  BOOST_TEST(testRF(S("abcde"), "", 0, 0, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 0, 2, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 0, 4, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 0, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 0, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 0, 9, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 0, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 19, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 0, 20, S::npos));
  BOOST_TEST(testRF(S("abcde"), "", 1, 0, 1));
  BOOST_TEST(testRF(S("abcde"), "abcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcde"), "abcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 1, 2, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 1, 4, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 1, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 1, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 1, 9, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 19, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 1, 20, S::npos));
  BOOST_TEST(testRF(S("abcde"), "", 2, 0, 2));
  BOOST_TEST(testRF(S("abcde"), "abcde", 2, 0, 2));
  BOOST_TEST(testRF(S("abcde"), "abcde", 2, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 2, 2, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 2, 4, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 2, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 2, 0, 2));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 2, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 2, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 2, 9, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 2, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 0, 2));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 19, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 2, 20, S::npos));
  BOOST_TEST(testRF(S("abcde"), "", 4, 0, 4));
  BOOST_TEST(testRF(S("abcde"), "abcde", 4, 0, 4));
  BOOST_TEST(testRF(S("abcde"), "abcde", 4, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 4, 2, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 4, 4, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 4, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 4, 0, 4));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 4, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 4, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 4, 9, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 4, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 0, 4));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 19, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 4, 20, S::npos));
  BOOST_TEST(testRF(S("abcde"), "", 5, 0, 5));
  BOOST_TEST(testRF(S("abcde"), "abcde", 5, 0, 5));
  BOOST_TEST(testRF(S("abcde"), "abcde", 5, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 5, 2, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 5, 4, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 5, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 5, 0, 5));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 5, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 5, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 5, 9, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 5, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 0, 5));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 19, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 5, 20, S::npos));
  BOOST_TEST(testRF(S("abcde"), "", 6, 0, 5));
  BOOST_TEST(testRF(S("abcde"), "abcde", 6, 0, 5));
  BOOST_TEST(testRF(S("abcde"), "abcde", 6, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 6, 2, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 6, 4, 0));
  BOOST_TEST(testRF(S("abcde"), "abcde", 6, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 6, 0, 5));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 6, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 6, 5, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 6, 9, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcde", 6, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 0, 5));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 1, 0));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 10, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 19, S::npos));
  BOOST_TEST(testRF(S("abcde"), "abcdeabcdeabcdeabcde", 6, 20, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "", 0, 0, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 0, 2, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 0, 4, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 0, 5, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 0, 5, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 0, 9, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 0, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 19, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 0, 20, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "", 1, 0, 1));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 1, 2, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 1, 4, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 1, 5, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 1, 5, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 1, 9, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 1, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 19, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 1, 20, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "", 5, 0, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 5, 0, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 5, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 5, 2, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 5, 4, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 5, 5, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 5, 0, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 5, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 5, 5, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 5, 9, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 5, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 0, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 19, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 5, 20, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "", 9, 0, 9));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 9, 0, 9));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 9, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 9, 2, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 9, 4, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 9, 5, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 9, 0, 9));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 9, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 9, 5, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 9, 9, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 9, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 0, 9));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 19, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 9, 20, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "", 10, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 10, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 10, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 10, 2, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 10, 4, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 10, 5, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 10, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 10, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 10, 5, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 10, 9, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 10, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 19, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 10, 20, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "", 11, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 11, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 11, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 11, 2, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 11, 4, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcde", 11, 5, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 11, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 11, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 11, 5, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 11, 9, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcde", 11, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 1, 5));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 19, S::npos));
  BOOST_TEST(testRF(S("abcdeabcde"), "abcdeabcdeabcdeabcde", 11, 20, S::npos));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "", 0, 0, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 2, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 4, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 0, 5, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 5, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 9, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 0, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 0, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 19, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 0, 20, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "", 1, 0, 1));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 2, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 4, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 1, 5, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 5, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 9, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 1, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 0, 1));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 1, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 10, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 19, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 1, 20, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "", 10, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 1, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 2, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 4, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 10, 5, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 1, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 5, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 9, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 10, 10, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 0, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 1, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 10, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 19, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 10, 20, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "", 19, 0, 19));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 0, 19));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 1, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 2, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 4, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 19, 5, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 0, 19));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 1, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 5, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 9, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 19, 10, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 0, 19));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 1, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 10, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 19, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 19, 20, 0));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "", 20, 0, 20));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 0, 20));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 1, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 2, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 4, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcde", 20, 5, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 0, 20));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 1, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 5, 15));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 9, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcde", 20, 10, 10));
  BOOST_TEST(testRF(S("abcdeabcdeabcdeabcde"), "abcdeabcdeabcdeabcde", 20, 0, 20));

  // find_first_of

  BOOST_TEST(fs1.find_first_of(v1) == 0);
  BOOST_TEST(fs1.find_first_of(v2) == 1);
  BOOST_TEST(fs1.find_first_of(fs1) == 0);
  BOOST_TEST(fs1.find_first_of(fs2) == 1);

  BOOST_TEST(fs1.find_first_of(cs1) == 0);
  BOOST_TEST(fs1.find_first_of(cs2) == 1);

  BOOST_TEST(fs1.find_first_of(cs1, 0) == 0);
  BOOST_TEST(fs1.find_first_of(cs2, 0) == 1);

  BOOST_TEST(fs1.find_first_of(cs2, 0, 2) == 1);

  BOOST_TEST(fs1.find_first_of(cs1, 4) == 4);
  BOOST_TEST(fs1.find_first_of(cs2, 4) == 4);

  BOOST_TEST(fs1.find_first_of('1') == 0);
  BOOST_TEST(fs1.find_first_of('1', 4) == S::npos);

  BOOST_TEST(testFF(S(""), "", 0, 0, S::npos));
  BOOST_TEST(testFF(S(""), "irkhs", 0, 0, S::npos));
  BOOST_TEST(testFF(S(""), "kante", 0, 1, S::npos));
  BOOST_TEST(testFF(S(""), "oknlr", 0, 2, S::npos));
  BOOST_TEST(testFF(S(""), "pcdro", 0, 4, S::npos));
  BOOST_TEST(testFF(S(""), "bnrpe", 0, 5, S::npos));
  BOOST_TEST(testFF(S(""), "jtdaefblso", 0, 0, S::npos));
  BOOST_TEST(testFF(S(""), "oselktgbca", 0, 1, S::npos));
  BOOST_TEST(testFF(S(""), "eqgaplhckj", 0, 5, S::npos));
  BOOST_TEST(testFF(S(""), "bjahtcmnlp", 0, 9, S::npos));
  BOOST_TEST(testFF(S(""), "hjlcmgpket", 0, 10, S::npos));
  BOOST_TEST(testFF(S(""), "htaobedqikfplcgjsmrn", 0, 0, S::npos));
  BOOST_TEST(testFF(S(""), "hpqiarojkcdlsgnmfetb", 0, 1, S::npos));
  BOOST_TEST(testFF(S(""), "dfkaprhjloqetcsimnbg", 0, 10, S::npos));
  BOOST_TEST(testFF(S(""), "ihqrfebgadntlpmjksoc", 0, 19, S::npos));
  BOOST_TEST(testFF(S(""), "ngtjfcalbseiqrphmkdo", 0, 20, S::npos));
  BOOST_TEST(testFF(S(""), "", 1, 0, S::npos));
  BOOST_TEST(testFF(S(""), "lbtqd", 1, 0, S::npos));
  BOOST_TEST(testFF(S(""), "tboim", 1, 1, S::npos));
  BOOST_TEST(testFF(S(""), "slcer", 1, 2, S::npos));
  BOOST_TEST(testFF(S(""), "cbjfs", 1, 4, S::npos));
  BOOST_TEST(testFF(S(""), "aqibs", 1, 5, S::npos));
  BOOST_TEST(testFF(S(""), "gtfblmqinc", 1, 0, S::npos));
  BOOST_TEST(testFF(S(""), "mkqpbtdalg", 1, 1, S::npos));
  BOOST_TEST(testFF(S(""), "kphatlimcd", 1, 5, S::npos));
  BOOST_TEST(testFF(S(""), "pblasqogic", 1, 9, S::npos));
  BOOST_TEST(testFF(S(""), "arosdhcfme", 1, 10, S::npos));
  BOOST_TEST(testFF(S(""), "blkhjeogicatqfnpdmsr", 1, 0, S::npos));
  BOOST_TEST(testFF(S(""), "bmhineprjcoadgstflqk", 1, 1, S::npos));
  BOOST_TEST(testFF(S(""), "djkqcmetslnghpbarfoi", 1, 10, S::npos));
  BOOST_TEST(testFF(S(""), "lgokshjtpbemarcdqnfi", 1, 19, S::npos));
  BOOST_TEST(testFF(S(""), "bqjhtkfepimcnsgrlado", 1, 20, S::npos));
  BOOST_TEST(testFF(S("eaint"), "", 0, 0, S::npos));
  BOOST_TEST(testFF(S("binja"), "gfsrt", 0, 0, S::npos));
  BOOST_TEST(testFF(S("latkm"), "pfsoc", 0, 1, S::npos));
  BOOST_TEST(testFF(S("lecfr"), "tpflm", 0, 2, S::npos));
  BOOST_TEST(testFF(S("eqkst"), "sgkec", 0, 4, 0));
  BOOST_TEST(testFF(S("cdafr"), "romds", 0, 5, 1));
  BOOST_TEST(testFF(S("prbhe"), "qhjistlgmr", 0, 0, S::npos));
  BOOST_TEST(testFF(S("lbisk"), "pedfirsglo", 0, 1, S::npos));
  BOOST_TEST(testFF(S("hrlpd"), "aqcoslgrmk", 0, 5, S::npos));
  BOOST_TEST(testFF(S("ehmja"), "dabckmepqj", 0, 9, 0));
  BOOST_TEST(testFF(S("mhqgd"), "pqscrjthli", 0, 10, 1));
  BOOST_TEST(testFF(S("tgklq"), "kfphdcsjqmobliagtren", 0, 0, S::npos));
  BOOST_TEST(testFF(S("bocjs"), "rokpefncljibsdhqtagm", 0, 1, S::npos));
  BOOST_TEST(testFF(S("grbsd"), "afionmkphlebtcjqsgrd", 0, 10, S::npos));
  BOOST_TEST(testFF(S("ofjqr"), "aenmqplidhkofrjbctsg", 0, 19, 0));
  BOOST_TEST(testFF(S("btlfi"), "osjmbtcadhiklegrpqnf", 0, 20, 0));
  BOOST_TEST(testFF(S("clrgb"), "", 1, 0, S::npos));
  BOOST_TEST(testFF(S("tjmek"), "osmia", 1, 0, S::npos));
  BOOST_TEST(testFF(S("bgstp"), "ckonl", 1, 1, S::npos));
  BOOST_TEST(testFF(S("hstrk"), "ilcaj", 1, 2, S::npos));
  BOOST_TEST(testFF(S("kmspj"), "lasiq", 1, 4, 2));
  BOOST_TEST(testFF(S("tjboh"), "kfqmr", 1, 5, S::npos));
  BOOST_TEST(testFF(S("ilbcj"), "klnitfaobg", 1, 0, S::npos));
  BOOST_TEST(testFF(S("jkngf"), "gjhmdlqikp", 1, 1, 3));
  BOOST_TEST(testFF(S("gfcql"), "skbgtahqej", 1, 5, S::npos));
  BOOST_TEST(testFF(S("dqtlg"), "bjsdgtlpkf", 1, 9, 2));
  BOOST_TEST(testFF(S("bthpg"), "bjgfmnlkio", 1, 10, 4));
  BOOST_TEST(testFF(S("dgsnq"), "lbhepotfsjdqigcnamkr", 1, 0, S::npos));
  BOOST_TEST(testFF(S("rmfhp"), "tebangckmpsrqdlfojhi", 1, 1, S::npos));
  BOOST_TEST(testFF(S("jfdam"), "joflqbdkhtegimscpanr", 1, 10, 1));
  BOOST_TEST(testFF(S("edapb"), "adpmcohetfbsrjinlqkg", 1, 19, 1));
  BOOST_TEST(testFF(S("brfsm"), "iacldqjpfnogbsrhmetk", 1, 20, 1));
  BOOST_TEST(testFF(S("ndrhl"), "", 2, 0, S::npos));
  BOOST_TEST(testFF(S("mrecp"), "otkgb", 2, 0, S::npos));
  BOOST_TEST(testFF(S("qlasf"), "cqsjl", 2, 1, S::npos));
  BOOST_TEST(testFF(S("smaqd"), "dpifl", 2, 2, 4));
  BOOST_TEST(testFF(S("hjeni"), "oapht", 2, 4, S::npos));
  BOOST_TEST(testFF(S("ocmfj"), "cifts", 2, 5, 3));
  BOOST_TEST(testFF(S("hmftq"), "nmsckbgalo", 2, 0, S::npos));
  BOOST_TEST(testFF(S("fklad"), "tpksqhamle", 2, 1, S::npos));
  BOOST_TEST(testFF(S("dirnm"), "tpdrchmkji", 2, 5, 2));
  BOOST_TEST(testFF(S("hrgdc"), "ijagfkblst", 2, 9, 2));
  BOOST_TEST(testFF(S("ifakg"), "kpocsignjb", 2, 10, 3));
  BOOST_TEST(testFF(S("ebrgd"), "pecqtkjsnbdrialgmohf", 2, 0, S::npos));
  BOOST_TEST(testFF(S("rcjml"), "aiortphfcmkjebgsndql", 2, 1, S::npos));
  BOOST_TEST(testFF(S("peqmt"), "sdbkeamglhipojqftrcn", 2, 10, 3));
  BOOST_TEST(testFF(S("frehn"), "ljqncehgmfktroapidbs", 2, 19, 2));
  BOOST_TEST(testFF(S("tqolf"), "rtcfodilamkbenjghqps", 2, 20, 2));
  BOOST_TEST(testFF(S("cjgao"), "", 4, 0, S::npos));
  BOOST_TEST(testFF(S("kjplq"), "mabns", 4, 0, S::npos));
  BOOST_TEST(testFF(S("herni"), "bdnrp", 4, 1, S::npos));
  BOOST_TEST(testFF(S("tadrb"), "scidp", 4, 2, S::npos));
  BOOST_TEST(testFF(S("pkfeo"), "agbjl", 4, 4, S::npos));
  BOOST_TEST(testFF(S("hoser"), "jfmpr", 4, 5, 4));
  BOOST_TEST(testFF(S("kgrsp"), "rbpefghsmj", 4, 0, S::npos));
  BOOST_TEST(testFF(S("pgejb"), "apsfntdoqc", 4, 1, S::npos));
  BOOST_TEST(testFF(S("thlnq"), "ndkjeisgcl", 4, 5, S::npos));
  BOOST_TEST(testFF(S("nbmit"), "rnfpqatdeo", 4, 9, 4));
  BOOST_TEST(testFF(S("jgmib"), "bntjlqrfik", 4, 10, 4));
  BOOST_TEST(testFF(S("ncrfj"), "kcrtmpolnaqejghsfdbi", 4, 0, S::npos));
  BOOST_TEST(testFF(S("ncsik"), "lobheanpkmqidsrtcfgj", 4, 1, S::npos));
  BOOST_TEST(testFF(S("sgbfh"), "athdkljcnreqbgpmisof", 4, 10, 4));
  BOOST_TEST(testFF(S("dktbn"), "qkdmjialrscpbhefgont", 4, 19, 4));
  BOOST_TEST(testFF(S("fthqm"), "dmasojntqleribkgfchp", 4, 20, 4));
  BOOST_TEST(testFF(S("klopi"), "", 5, 0, S::npos));
  BOOST_TEST(testFF(S("dajhn"), "psthd", 5, 0, S::npos));
  BOOST_TEST(testFF(S("jbgno"), "rpmjd", 5, 1, S::npos));
  BOOST_TEST(testFF(S("hkjae"), "dfsmk", 5, 2, S::npos));
  BOOST_TEST(testFF(S("gbhqo"), "skqne", 5, 4, S::npos));
  BOOST_TEST(testFF(S("ktdor"), "kipnf", 5, 5, S::npos));
  BOOST_TEST(testFF(S("ldprn"), "hmrnqdgifl", 5, 0, S::npos));
  BOOST_TEST(testFF(S("egmjk"), "fsmjcdairn", 5, 1, S::npos));
  BOOST_TEST(testFF(S("armql"), "pcdgltbrfj", 5, 5, S::npos));
  BOOST_TEST(testFF(S("cdhjo"), "aekfctpirg", 5, 9, S::npos));
  BOOST_TEST(testFF(S("jcons"), "ledihrsgpf", 5, 10, S::npos));
  BOOST_TEST(testFF(S("cbrkp"), "mqcklahsbtirgopefndj", 5, 0, S::npos));
  BOOST_TEST(testFF(S("fhgna"), "kmlthaoqgecrnpdbjfis", 5, 1, S::npos));
  BOOST_TEST(testFF(S("ejfcd"), "sfhbamcdptojlkrenqgi", 5, 10, S::npos));
  BOOST_TEST(testFF(S("kqjhe"), "pbniofmcedrkhlstgaqj", 5, 19, S::npos));
  BOOST_TEST(testFF(S("pbdjl"), "mongjratcskbhqiepfdl", 5, 20, S::npos));
  BOOST_TEST(testFF(S("gajqn"), "", 6, 0, S::npos));
  BOOST_TEST(testFF(S("stedk"), "hrnat", 6, 0, S::npos));
  BOOST_TEST(testFF(S("tjkaf"), "gsqdt", 6, 1, S::npos));
  BOOST_TEST(testFF(S("dthpe"), "bspkd", 6, 2, S::npos));
  BOOST_TEST(testFF(S("klhde"), "ohcmb", 6, 4, S::npos));
  BOOST_TEST(testFF(S("bhlki"), "heatr", 6, 5, S::npos));
  BOOST_TEST(testFF(S("lqmoh"), "pmblckedfn", 6, 0, S::npos));
  BOOST_TEST(testFF(S("mtqin"), "aceqmsrbik", 6, 1, S::npos));
  BOOST_TEST(testFF(S("dpqbr"), "lmbtdehjrn", 6, 5, S::npos));
  BOOST_TEST(testFF(S("kdhmo"), "teqmcrlgib", 6, 9, S::npos));
  BOOST_TEST(testFF(S("jblqp"), "njolbmspac", 6, 10, S::npos));
  BOOST_TEST(testFF(S("qmjgl"), "pofnhidklamecrbqjgst", 6, 0, S::npos));
  BOOST_TEST(testFF(S("rothp"), "jbhckmtgrqnosafedpli", 6, 1, S::npos));
  BOOST_TEST(testFF(S("ghknq"), "dobntpmqklicsahgjerf", 6, 10, S::npos));
  BOOST_TEST(testFF(S("eopfi"), "tpdshainjkbfoemlrgcq", 6, 19, S::npos));
  BOOST_TEST(testFF(S("dsnmg"), "oldpfgeakrnitscbjmqh", 6, 20, S::npos));
  BOOST_TEST(testFF(S("jnkrfhotgl"), "", 0, 0, S::npos));
  BOOST_TEST(testFF(S("dltjfngbko"), "rqegt", 0, 0, S::npos));
  BOOST_TEST(testFF(S("bmjlpkiqde"), "dashm", 0, 1, 8));
  BOOST_TEST(testFF(S("skrflobnqm"), "jqirk", 0, 2, 8));
  BOOST_TEST(testFF(S("jkpldtshrm"), "rckeg", 0, 4, 1));
  BOOST_TEST(testFF(S("ghasdbnjqo"), "jscie", 0, 5, 3));
  BOOST_TEST(testFF(S("igrkhpbqjt"), "efsphndliq", 0, 0, S::npos));
  BOOST_TEST(testFF(S("ikthdgcamf"), "gdicosleja", 0, 1, 5));
  BOOST_TEST(testFF(S("pcofgeniam"), "qcpjibosfl", 0, 5, 0));
  BOOST_TEST(testFF(S("rlfjgesqhc"), "lrhmefnjcq", 0, 9, 0));
  BOOST_TEST(testFF(S("itphbqsker"), "dtablcrseo", 0, 10, 1));
  BOOST_TEST(testFF(S("skjafcirqm"), "apckjsftedbhgomrnilq", 0, 0, S::npos));
  BOOST_TEST(testFF(S("tcqomarsfd"), "pcbrgflehjtiadnsokqm", 0, 1, S::npos));
  BOOST_TEST(testFF(S("rocfeldqpk"), "nsiadegjklhobrmtqcpf", 0, 10, 4));
  BOOST_TEST(testFF(S("cfpegndlkt"), "cpmajdqnolikhgsbretf", 0, 19, 0));
  BOOST_TEST(testFF(S("fqbtnkeasj"), "jcflkntmgiqrphdosaeb", 0, 20, 0));
  BOOST_TEST(testFF(S("shbcqnmoar"), "", 1, 0, S::npos));
  BOOST_TEST(testFF(S("bdoshlmfin"), "ontrs", 1, 0, S::npos));
  BOOST_TEST(testFF(S("khfrebnsgq"), "pfkna", 1, 1, S::npos));
  BOOST_TEST(testFF(S("getcrsaoji"), "ekosa", 1, 2, 1));
  BOOST_TEST(testFF(S("fjiknedcpq"), "anqhk", 1, 4, 4));
  BOOST_TEST(testFF(S("tkejgnafrm"), "jekca", 1, 5, 1));
  BOOST_TEST(testFF(S("jnakolqrde"), "ikemsjgacf", 1, 0, S::npos));
  BOOST_TEST(testFF(S("lcjptsmgbe"), "arolgsjkhm", 1, 1, S::npos));
  BOOST_TEST(testFF(S("itfsmcjorl"), "oftkbldhre", 1, 5, 1));
  BOOST_TEST(testFF(S("omchkfrjea"), "gbkqdoeftl", 1, 9, 4));
  BOOST_TEST(testFF(S("cigfqkated"), "sqcflrgtim", 1, 10, 1));
  BOOST_TEST(testFF(S("tscenjikml"), "fmhbkislrjdpanogqcet", 1, 0, S::npos));
  BOOST_TEST(testFF(S("qcpaemsinf"), "rnioadktqlgpbcjsmhef", 1, 1, S::npos));
  BOOST_TEST(testFF(S("gltkojeipd"), "oakgtnldpsefihqmjcbr", 1, 10, 1));
  BOOST_TEST(testFF(S("qistfrgnmp"), "gbnaelosidmcjqktfhpr", 1, 19, 1));
  BOOST_TEST(testFF(S("bdnpfcqaem"), "akbripjhlosndcmqgfet", 1, 20, 1));
  BOOST_TEST(testFF(S("ectnhskflp"), "", 5, 0, S::npos));
  BOOST_TEST(testFF(S("fgtianblpq"), "pijag", 5, 0, S::npos));
  BOOST_TEST(testFF(S("mfeqklirnh"), "jrckd", 5, 1, S::npos));
  BOOST_TEST(testFF(S("astedncjhk"), "qcloh", 5, 2, 6));
  BOOST_TEST(testFF(S("fhlqgcajbr"), "thlmp", 5, 4, S::npos));
  BOOST_TEST(testFF(S("epfhocmdng"), "qidmo", 5, 5, 6));
  BOOST_TEST(testFF(S("apcnsibger"), "lnegpsjqrd", 5, 0, S::npos));
  BOOST_TEST(testFF(S("aqkocrbign"), "rjqdablmfs", 5, 1, 5));
  BOOST_TEST(testFF(S("ijsmdtqgce"), "enkgpbsjaq", 5, 5, 7));
  BOOST_TEST(testFF(S("clobgsrken"), "kdsgoaijfh", 5, 9, 5));
  BOOST_TEST(testFF(S("jbhcfposld"), "trfqgmckbe", 5, 10, S::npos));
  BOOST_TEST(testFF(S("oqnpblhide"), "igetsracjfkdnpoblhqm", 5, 0, S::npos));
  BOOST_TEST(testFF(S("lroeasctif"), "nqctfaogirshlekbdjpm", 5, 1, S::npos));
  BOOST_TEST(testFF(S("bpjlgmiedh"), "csehfgomljdqinbartkp", 5, 10, 5));
  BOOST_TEST(testFF(S("pamkeoidrj"), "qahoegcmplkfsjbdnitr", 5, 19, 5));
  BOOST_TEST(testFF(S("espogqbthk"), "dpteiajrqmsognhlfbkc", 5, 20, 5));
  BOOST_TEST(testFF(S("shoiedtcjb"), "", 9, 0, S::npos));
  BOOST_TEST(testFF(S("ebcinjgads"), "tqbnh", 9, 0, S::npos));
  BOOST_TEST(testFF(S("dqmregkcfl"), "akmle", 9, 1, S::npos));
  BOOST_TEST(testFF(S("ngcrieqajf"), "iqfkm", 9, 2, S::npos));
  BOOST_TEST(testFF(S("qosmilgnjb"), "tqjsr", 9, 4, S::npos));
  BOOST_TEST(testFF(S("ikabsjtdfl"), "jplqg", 9, 5, 9));
  BOOST_TEST(testFF(S("ersmicafdh"), "oilnrbcgtj", 9, 0, S::npos));
  BOOST_TEST(testFF(S("fdnplotmgh"), "morkglpesn", 9, 1, S::npos));
  BOOST_TEST(testFF(S("fdbicojerm"), "dmicerngat", 9, 5, 9));
  BOOST_TEST(testFF(S("mbtafndjcq"), "radgeskbtc", 9, 9, S::npos));
  BOOST_TEST(testFF(S("mlenkpfdtc"), "ljikprsmqo", 9, 10, S::npos));
  BOOST_TEST(testFF(S("ahlcifdqgs"), "trqihkcgsjamfdbolnpe", 9, 0, S::npos));
  BOOST_TEST(testFF(S("bgjemaltks"), "lqmthbsrekajgnofcipd", 9, 1, S::npos));
  BOOST_TEST(testFF(S("pdhslbqrfc"), "jtalmedribkgqsopcnfh", 9, 10, S::npos));
  BOOST_TEST(testFF(S("dirhtsnjkc"), "spqfoiclmtagejbndkrh", 9, 19, 9));
  BOOST_TEST(testFF(S("dlroktbcja"), "nmotklspigjrdhcfaebq", 9, 20, 9));
  BOOST_TEST(testFF(S("ncjpmaekbs"), "", 10, 0, S::npos));
  BOOST_TEST(testFF(S("hlbosgmrak"), "hpmsd", 10, 0, S::npos));
  BOOST_TEST(testFF(S("pqfhsgilen"), "qnpor", 10, 1, S::npos));
  BOOST_TEST(testFF(S("gqtjsbdckh"), "otdma", 10, 2, S::npos));
  BOOST_TEST(testFF(S("cfkqpjlegi"), "efhjg", 10, 4, S::npos));
  BOOST_TEST(testFF(S("beanrfodgj"), "odpte", 10, 5, S::npos));
  BOOST_TEST(testFF(S("adtkqpbjfi"), "bctdgfmolr", 10, 0, S::npos));
  BOOST_TEST(testFF(S("iomkfthagj"), "oaklidrbqg", 10, 1, S::npos));
  BOOST_TEST(testFF(S("sdpcilonqj"), "dnjfsagktr", 10, 5, S::npos));
  BOOST_TEST(testFF(S("gtfbdkqeml"), "nejaktmiqg", 10, 9, S::npos));
  BOOST_TEST(testFF(S("bmeqgcdorj"), "pjqonlebsf", 10, 10, S::npos));
  BOOST_TEST(testFF(S("etqlcanmob"), "dshmnbtolcjepgaikfqr", 10, 0, S::npos));
  BOOST_TEST(testFF(S("roqmkbdtia"), "iogfhpabtjkqlrnemcds", 10, 1, S::npos));
  BOOST_TEST(testFF(S("kadsithljf"), "ngridfabjsecpqltkmoh", 10, 10, S::npos));
  BOOST_TEST(testFF(S("sgtkpbfdmh"), "athmknplcgofrqejsdib", 10, 19, S::npos));
  BOOST_TEST(testFF(S("qgmetnabkl"), "ldobhmqcafnjtkeisgrp", 10, 20, S::npos));
  BOOST_TEST(testFF(S("cqjohampgd"), "", 11, 0, S::npos));
  BOOST_TEST(testFF(S("hobitmpsan"), "aocjb", 11, 0, S::npos));
  BOOST_TEST(testFF(S("tjehkpsalm"), "jbrnk", 11, 1, S::npos));
  BOOST_TEST(testFF(S("ngfbojitcl"), "tqedg", 11, 2, S::npos));
  BOOST_TEST(testFF(S("rcfkdbhgjo"), "nqskp", 11, 4, S::npos));
  BOOST_TEST(testFF(S("qghptonrea"), "eaqkl", 11, 5, S::npos));
  BOOST_TEST(testFF(S("hnprfgqjdl"), "reaoicljqm", 11, 0, S::npos));
  BOOST_TEST(testFF(S("hlmgabenti"), "lsftgajqpm", 11, 1, S::npos));
  BOOST_TEST(testFF(S("ofcjanmrbs"), "rlpfogmits", 11, 5, S::npos));
  BOOST_TEST(testFF(S("jqedtkornm"), "shkncmiaqj", 11, 9, S::npos));
  BOOST_TEST(testFF(S("rfedlasjmg"), "fpnatrhqgs", 11, 10, S::npos));
  BOOST_TEST(testFF(S("talpqjsgkm"), "sjclemqhnpdbgikarfot", 11, 0, S::npos));
  BOOST_TEST(testFF(S("lrkcbtqpie"), "otcmedjikgsfnqbrhpla", 11, 1, S::npos));
  BOOST_TEST(testFF(S("cipogdskjf"), "bonsaefdqiprkhlgtjcm", 11, 10, S::npos));
  BOOST_TEST(testFF(S("nqedcojahi"), "egpscmahijlfnkrodqtb", 11, 19, S::npos));
  BOOST_TEST(testFF(S("hefnrkmctj"), "kmqbfepjthgilscrndoa", 11, 20, S::npos));
  BOOST_TEST(testFF(S("atqirnmekfjolhpdsgcb"), "", 0, 0, S::npos));
  BOOST_TEST(testFF(S("echfkmlpribjnqsaogtd"), "prboq", 0, 0, S::npos));
  BOOST_TEST(testFF(S("qnhiftdgcleajbpkrosm"), "fjcqh", 0, 1, 4));
  BOOST_TEST(testFF(S("chamfknorbedjitgslpq"), "fmosa", 0, 2, 3));
  BOOST_TEST(testFF(S("njhqpibfmtlkaecdrgso"), "qdbok", 0, 4, 3));
  BOOST_TEST(testFF(S("ebnghfsqkprmdcljoiat"), "amslg", 0, 5, 3));
  BOOST_TEST(testFF(S("letjomsgihfrpqbkancd"), "smpltjneqb", 0, 0, S::npos));
  BOOST_TEST(testFF(S("nblgoipcrqeaktshjdmf"), "flitskrnge", 0, 1, 19));
  BOOST_TEST(testFF(S("cehkbngtjoiflqapsmrd"), "pgqihmlbef", 0, 5, 2));
  BOOST_TEST(testFF(S("mignapfoklbhcqjetdrs"), "cfpdqjtgsb", 0, 9, 2));
  BOOST_TEST(testFF(S("ceatbhlsqjgpnokfrmdi"), "htpsiaflom", 0, 10, 2));
  BOOST_TEST(testFF(S("ocihkjgrdelpfnmastqb"), "kpjfiaceghsrdtlbnomq", 0, 0, S::npos));
  BOOST_TEST(testFF(S("noelgschdtbrjfmiqkap"), "qhtbomidljgafneksprc", 0, 1, 16));
  BOOST_TEST(testFF(S("dkclqfombepritjnghas"), "nhtjobkcefldimpsaqgr", 0, 10, 1));
  BOOST_TEST(testFF(S("miklnresdgbhqcojftap"), "prabcjfqnoeskilmtgdh", 0, 19, 0));
  BOOST_TEST(testFF(S("htbcigojaqmdkfrnlsep"), "dtrgmchilkasqoebfpjn", 0, 20, 0));
  BOOST_TEST(testFF(S("febhmqtjanokscdirpgl"), "", 1, 0, S::npos));
  BOOST_TEST(testFF(S("loakbsqjpcrdhftniegm"), "sqome", 1, 0, S::npos));
  BOOST_TEST(testFF(S("reagphsqflbitdcjmkno"), "smfte", 1, 1, 6));
  BOOST_TEST(testFF(S("jitlfrqemsdhkopncabg"), "ciboh", 1, 2, 1));
  BOOST_TEST(testFF(S("mhtaepscdnrjqgbkifol"), "haois", 1, 4, 1));
  BOOST_TEST(testFF(S("tocesrfmnglpbjihqadk"), "abfki", 1, 5, 6));
  BOOST_TEST(testFF(S("lpfmctjrhdagneskbqoi"), "frdkocntmq", 1, 0, S::npos));
  BOOST_TEST(testFF(S("lsmqaepkdhncirbtjfgo"), "oasbpedlnr", 1, 1, 19));
  BOOST_TEST(testFF(S("epoiqmtldrabnkjhcfsg"), "kltqmhgand", 1, 5, 4));
  BOOST_TEST(testFF(S("emgasrilpknqojhtbdcf"), "gdtfjchpmr", 1, 9, 1));
  BOOST_TEST(testFF(S("hnfiagdpcklrjetqbsom"), "ponmcqblet", 1, 10, 1));
  BOOST_TEST(testFF(S("nsdfebgajhmtricpoklq"), "sgphqdnofeiklatbcmjr", 1, 0, S::npos));
  BOOST_TEST(testFF(S("atjgfsdlpobmeiqhncrk"), "ljqprsmigtfoneadckbh", 1, 1, 7));
  BOOST_TEST(testFF(S("sitodfgnrejlahcbmqkp"), "ligeojhafnkmrcsqtbdp", 1, 10, 1));
  BOOST_TEST(testFF(S("fraghmbiceknltjpqosd"), "lsimqfnjarbopedkhcgt", 1, 19, 1));
  BOOST_TEST(testFF(S("pmafenlhqtdbkirjsogc"), "abedmfjlghniorcqptks", 1, 20, 1));
  BOOST_TEST(testFF(S("pihgmoeqtnakrjslcbfd"), "", 10, 0, S::npos));
  BOOST_TEST(testFF(S("gjdkeprctqblnhiafsom"), "hqtoa", 10, 0, S::npos));
  BOOST_TEST(testFF(S("mkpnblfdsahrcqijteog"), "cahif", 10, 1, 12));
  BOOST_TEST(testFF(S("gckarqnelodfjhmbptis"), "kehis", 10, 2, S::npos));
  BOOST_TEST(testFF(S("gqpskidtbclomahnrjfe"), "kdlmh", 10, 4, 10));
  BOOST_TEST(testFF(S("pkldjsqrfgitbhmaecno"), "paeql", 10, 5, 15));
  BOOST_TEST(testFF(S("aftsijrbeklnmcdqhgop"), "aghoqiefnb", 10, 0, S::npos));
  BOOST_TEST(testFF(S("mtlgdrhafjkbiepqnsoc"), "jrbqaikpdo", 10, 1, S::npos));
  BOOST_TEST(testFF(S("pqgirnaefthokdmbsclj"), "smjonaeqcl", 10, 5, 11));
  BOOST_TEST(testFF(S("kpdbgjmtherlsfcqoina"), "eqbdrkcfah", 10, 9, 10));
  BOOST_TEST(testFF(S("jrlbothiknqmdgcfasep"), "kapmsienhf", 10, 10, 11));
  BOOST_TEST(testFF(S("mjogldqferckabinptsh"), "jpqotrlenfcsbhkaimdg", 10, 0, S::npos));
  BOOST_TEST(testFF(S("apoklnefbhmgqcdrisjt"), "jlbmhnfgtcqprikeados", 10, 1, 18));
  BOOST_TEST(testFF(S("ifeopcnrjbhkdgatmqls"), "stgbhfmdaljnpqoicker", 10, 10, 10));
  BOOST_TEST(testFF(S("ckqhaiesmjdnrgolbtpf"), "oihcetflbjagdsrkmqpn", 10, 19, 10));
  BOOST_TEST(testFF(S("bnlgapfimcoterskqdjh"), "adtclebmnpjsrqfkigoh", 10, 20, 10));
  BOOST_TEST(testFF(S("kgdlrobpmjcthqsafeni"), "", 19, 0, S::npos));
  BOOST_TEST(testFF(S("dfkechomjapgnslbtqir"), "beafg", 19, 0, S::npos));
  BOOST_TEST(testFF(S("rloadknfbqtgmhcsipje"), "iclat", 19, 1, S::npos));
  BOOST_TEST(testFF(S("mgjhkolrnadqbpetcifs"), "rkhnf", 19, 2, S::npos));
  BOOST_TEST(testFF(S("cmlfakiojdrgtbsphqen"), "clshq", 19, 4, S::npos));
  BOOST_TEST(testFF(S("kghbfipeomsntdalrqjc"), "dtcoj", 19, 5, 19));
  BOOST_TEST(testFF(S("eldiqckrnmtasbghjfpo"), "rqosnjmfth", 19, 0, S::npos));
  BOOST_TEST(testFF(S("abqjcfedgotihlnspkrm"), "siatdfqglh", 19, 1, S::npos));
  BOOST_TEST(testFF(S("qfbadrtjsimkolcenhpg"), "mrlshtpgjq", 19, 5, S::npos));
  BOOST_TEST(testFF(S("abseghclkjqifmtodrnp"), "adlcskgqjt", 19, 9, S::npos));
  BOOST_TEST(testFF(S("ibmsnlrjefhtdokacqpg"), "drshcjknaf", 19, 10, S::npos));
  BOOST_TEST(testFF(S("mrkfciqjebaponsthldg"), "etsaqroinghpkjdlfcbm", 19, 0, S::npos));
  BOOST_TEST(testFF(S("mjkticdeoqshpalrfbgn"), "sgepdnkqliambtrocfhj", 19, 1, S::npos));
  BOOST_TEST(testFF(S("rqnoclbdejgiphtfsakm"), "nlmcjaqgbsortfdihkpe", 19, 10, 19));
  BOOST_TEST(testFF(S("plkqbhmtfaeodjcrsing"), "racfnpmosldibqkghjet", 19, 19, 19));
  BOOST_TEST(testFF(S("oegalhmstjrfickpbndq"), "fjhdsctkqeiolagrnmbp", 19, 20, 19));
  BOOST_TEST(testFF(S("rdtgjcaohpblniekmsfq"), "", 20, 0, S::npos));
  BOOST_TEST(testFF(S("ofkqbnjetrmsaidphglc"), "ejanp", 20, 0, S::npos));
  BOOST_TEST(testFF(S("grkpahljcftesdmonqib"), "odife", 20, 1, S::npos));
  BOOST_TEST(testFF(S("jimlgbhfqkteospardcn"), "okaqd", 20, 2, S::npos));
  BOOST_TEST(testFF(S("gftenihpmslrjkqadcob"), "lcdbi", 20, 4, S::npos));
  BOOST_TEST(testFF(S("bmhldogtckrfsanijepq"), "fsqbj", 20, 5, S::npos));
  BOOST_TEST(testFF(S("nfqkrpjdesabgtlcmoih"), "bigdomnplq", 20, 0, S::npos));
  BOOST_TEST(testFF(S("focalnrpiqmdkstehbjg"), "apiblotgcd", 20, 1, S::npos));
  BOOST_TEST(testFF(S("rhqdspkmebiflcotnjga"), "acfhdenops", 20, 5, S::npos));
  BOOST_TEST(testFF(S("rahdtmsckfboqlpniegj"), "jopdeamcrk", 20, 9, S::npos));
  BOOST_TEST(testFF(S("fbkeiopclstmdqranjhg"), "trqncbkgmh", 20, 10, S::npos));
  BOOST_TEST(testFF(S("lifhpdgmbconstjeqark"), "tomglrkencbsfjqpihda", 20, 0, S::npos));

  // find_last_of

  BOOST_TEST(fs1.find_last_of(v1) == 4);
  BOOST_TEST(fs1.find_last_of(v2) == 4);
  BOOST_TEST(fs1.find_last_of(fs1) == 4);
  BOOST_TEST(fs1.find_last_of(fs2) == 4);

  BOOST_TEST(fs1.find_last_of(cs1) == 4);
  BOOST_TEST(fs1.find_last_of(cs2) == 4);

  BOOST_TEST(fs1.find_last_of(cs1, 0) == 0);
  BOOST_TEST(fs1.find_last_of(cs2, 0) == S::npos);

  BOOST_TEST(fs1.find_last_of(cs2, 0, 2) == S::npos);

  BOOST_TEST(fs1.find_last_of(cs1, 4) == 4);
  BOOST_TEST(fs1.find_last_of(cs2, 4) == 4);

  BOOST_TEST(fs1.find_last_of('1') == 0);
  BOOST_TEST(fs1.find_last_of('5', 3) == S::npos);

  BOOST_TEST(testFL(S(""), "", 0, 0, S::npos));
  BOOST_TEST(testFL(S(""), "irkhs", 0, 0, S::npos));
  BOOST_TEST(testFL(S(""), "kante", 0, 1, S::npos));
  BOOST_TEST(testFL(S(""), "oknlr", 0, 2, S::npos));
  BOOST_TEST(testFL(S(""), "pcdro", 0, 4, S::npos));
  BOOST_TEST(testFL(S(""), "bnrpe", 0, 5, S::npos));
  BOOST_TEST(testFL(S(""), "jtdaefblso", 0, 0, S::npos));
  BOOST_TEST(testFL(S(""), "oselktgbca", 0, 1, S::npos));
  BOOST_TEST(testFL(S(""), "eqgaplhckj", 0, 5, S::npos));
  BOOST_TEST(testFL(S(""), "bjahtcmnlp", 0, 9, S::npos));
  BOOST_TEST(testFL(S(""), "hjlcmgpket", 0, 10, S::npos));
  BOOST_TEST(testFL(S(""), "htaobedqikfplcgjsmrn", 0, 0, S::npos));
  BOOST_TEST(testFL(S(""), "hpqiarojkcdlsgnmfetb", 0, 1, S::npos));
  BOOST_TEST(testFL(S(""), "dfkaprhjloqetcsimnbg", 0, 10, S::npos));
  BOOST_TEST(testFL(S(""), "ihqrfebgadntlpmjksoc", 0, 19, S::npos));
  BOOST_TEST(testFL(S(""), "ngtjfcalbseiqrphmkdo", 0, 20, S::npos));
  BOOST_TEST(testFL(S(""), "", 1, 0, S::npos));
  BOOST_TEST(testFL(S(""), "lbtqd", 1, 0, S::npos));
  BOOST_TEST(testFL(S(""), "tboim", 1, 1, S::npos));
  BOOST_TEST(testFL(S(""), "slcer", 1, 2, S::npos));
  BOOST_TEST(testFL(S(""), "cbjfs", 1, 4, S::npos));
  BOOST_TEST(testFL(S(""), "aqibs", 1, 5, S::npos));
  BOOST_TEST(testFL(S(""), "gtfblmqinc", 1, 0, S::npos));
  BOOST_TEST(testFL(S(""), "mkqpbtdalg", 1, 1, S::npos));
  BOOST_TEST(testFL(S(""), "kphatlimcd", 1, 5, S::npos));
  BOOST_TEST(testFL(S(""), "pblasqogic", 1, 9, S::npos));
  BOOST_TEST(testFL(S(""), "arosdhcfme", 1, 10, S::npos));
  BOOST_TEST(testFL(S(""), "blkhjeogicatqfnpdmsr", 1, 0, S::npos));
  BOOST_TEST(testFL(S(""), "bmhineprjcoadgstflqk", 1, 1, S::npos));
  BOOST_TEST(testFL(S(""), "djkqcmetslnghpbarfoi", 1, 10, S::npos));
  BOOST_TEST(testFL(S(""), "lgokshjtpbemarcdqnfi", 1, 19, S::npos));
  BOOST_TEST(testFL(S(""), "bqjhtkfepimcnsgrlado", 1, 20, S::npos));
  BOOST_TEST(testFL(S("eaint"), "", 0, 0, S::npos));
  BOOST_TEST(testFL(S("binja"), "gfsrt", 0, 0, S::npos));
  BOOST_TEST(testFL(S("latkm"), "pfsoc", 0, 1, S::npos));
  BOOST_TEST(testFL(S("lecfr"), "tpflm", 0, 2, S::npos));
  BOOST_TEST(testFL(S("eqkst"), "sgkec", 0, 4, 0));
  BOOST_TEST(testFL(S("cdafr"), "romds", 0, 5, S::npos));
  BOOST_TEST(testFL(S("prbhe"), "qhjistlgmr", 0, 0, S::npos));
  BOOST_TEST(testFL(S("lbisk"), "pedfirsglo", 0, 1, S::npos));
  BOOST_TEST(testFL(S("hrlpd"), "aqcoslgrmk", 0, 5, S::npos));
  BOOST_TEST(testFL(S("ehmja"), "dabckmepqj", 0, 9, 0));
  BOOST_TEST(testFL(S("mhqgd"), "pqscrjthli", 0, 10, S::npos));
  BOOST_TEST(testFL(S("tgklq"), "kfphdcsjqmobliagtren", 0, 0, S::npos));
  BOOST_TEST(testFL(S("bocjs"), "rokpefncljibsdhqtagm", 0, 1, S::npos));
  BOOST_TEST(testFL(S("grbsd"), "afionmkphlebtcjqsgrd", 0, 10, S::npos));
  BOOST_TEST(testFL(S("ofjqr"), "aenmqplidhkofrjbctsg", 0, 19, 0));
  BOOST_TEST(testFL(S("btlfi"), "osjmbtcadhiklegrpqnf", 0, 20, 0));
  BOOST_TEST(testFL(S("clrgb"), "", 1, 0, S::npos));
  BOOST_TEST(testFL(S("tjmek"), "osmia", 1, 0, S::npos));
  BOOST_TEST(testFL(S("bgstp"), "ckonl", 1, 1, S::npos));
  BOOST_TEST(testFL(S("hstrk"), "ilcaj", 1, 2, S::npos));
  BOOST_TEST(testFL(S("kmspj"), "lasiq", 1, 4, S::npos));
  BOOST_TEST(testFL(S("tjboh"), "kfqmr", 1, 5, S::npos));
  BOOST_TEST(testFL(S("ilbcj"), "klnitfaobg", 1, 0, S::npos));
  BOOST_TEST(testFL(S("jkngf"), "gjhmdlqikp", 1, 1, S::npos));
  BOOST_TEST(testFL(S("gfcql"), "skbgtahqej", 1, 5, 0));
  BOOST_TEST(testFL(S("dqtlg"), "bjsdgtlpkf", 1, 9, 0));
  BOOST_TEST(testFL(S("bthpg"), "bjgfmnlkio", 1, 10, 0));
  BOOST_TEST(testFL(S("dgsnq"), "lbhepotfsjdqigcnamkr", 1, 0, S::npos));
  BOOST_TEST(testFL(S("rmfhp"), "tebangckmpsrqdlfojhi", 1, 1, S::npos));
  BOOST_TEST(testFL(S("jfdam"), "joflqbdkhtegimscpanr", 1, 10, 1));
  BOOST_TEST(testFL(S("edapb"), "adpmcohetfbsrjinlqkg", 1, 19, 1));
  BOOST_TEST(testFL(S("brfsm"), "iacldqjpfnogbsrhmetk", 1, 20, 1));
  BOOST_TEST(testFL(S("ndrhl"), "", 2, 0, S::npos));
  BOOST_TEST(testFL(S("mrecp"), "otkgb", 2, 0, S::npos));
  BOOST_TEST(testFL(S("qlasf"), "cqsjl", 2, 1, S::npos));
  BOOST_TEST(testFL(S("smaqd"), "dpifl", 2, 2, S::npos));
  BOOST_TEST(testFL(S("hjeni"), "oapht", 2, 4, 0));
  BOOST_TEST(testFL(S("ocmfj"), "cifts", 2, 5, 1));
  BOOST_TEST(testFL(S("hmftq"), "nmsckbgalo", 2, 0, S::npos));
  BOOST_TEST(testFL(S("fklad"), "tpksqhamle", 2, 1, S::npos));
  BOOST_TEST(testFL(S("dirnm"), "tpdrchmkji", 2, 5, 2));
  BOOST_TEST(testFL(S("hrgdc"), "ijagfkblst", 2, 9, 2));
  BOOST_TEST(testFL(S("ifakg"), "kpocsignjb", 2, 10, 0));
  BOOST_TEST(testFL(S("ebrgd"), "pecqtkjsnbdrialgmohf", 2, 0, S::npos));
  BOOST_TEST(testFL(S("rcjml"), "aiortphfcmkjebgsndql", 2, 1, S::npos));
  BOOST_TEST(testFL(S("peqmt"), "sdbkeamglhipojqftrcn", 2, 10, 1));
  BOOST_TEST(testFL(S("frehn"), "ljqncehgmfktroapidbs", 2, 19, 2));
  BOOST_TEST(testFL(S("tqolf"), "rtcfodilamkbenjghqps", 2, 20, 2));
  BOOST_TEST(testFL(S("cjgao"), "", 4, 0, S::npos));
  BOOST_TEST(testFL(S("kjplq"), "mabns", 4, 0, S::npos));
  BOOST_TEST(testFL(S("herni"), "bdnrp", 4, 1, S::npos));
  BOOST_TEST(testFL(S("tadrb"), "scidp", 4, 2, S::npos));
  BOOST_TEST(testFL(S("pkfeo"), "agbjl", 4, 4, S::npos));
  BOOST_TEST(testFL(S("hoser"), "jfmpr", 4, 5, 4));
  BOOST_TEST(testFL(S("kgrsp"), "rbpefghsmj", 4, 0, S::npos));
  BOOST_TEST(testFL(S("pgejb"), "apsfntdoqc", 4, 1, S::npos));
  BOOST_TEST(testFL(S("thlnq"), "ndkjeisgcl", 4, 5, 3));
  BOOST_TEST(testFL(S("nbmit"), "rnfpqatdeo", 4, 9, 4));
  BOOST_TEST(testFL(S("jgmib"), "bntjlqrfik", 4, 10, 4));
  BOOST_TEST(testFL(S("ncrfj"), "kcrtmpolnaqejghsfdbi", 4, 0, S::npos));
  BOOST_TEST(testFL(S("ncsik"), "lobheanpkmqidsrtcfgj", 4, 1, S::npos));
  BOOST_TEST(testFL(S("sgbfh"), "athdkljcnreqbgpmisof", 4, 10, 4));
  BOOST_TEST(testFL(S("dktbn"), "qkdmjialrscpbhefgont", 4, 19, 4));
  BOOST_TEST(testFL(S("fthqm"), "dmasojntqleribkgfchp", 4, 20, 4));
  BOOST_TEST(testFL(S("klopi"), "", 5, 0, S::npos));
  BOOST_TEST(testFL(S("dajhn"), "psthd", 5, 0, S::npos));
  BOOST_TEST(testFL(S("jbgno"), "rpmjd", 5, 1, S::npos));
  BOOST_TEST(testFL(S("hkjae"), "dfsmk", 5, 2, S::npos));
  BOOST_TEST(testFL(S("gbhqo"), "skqne", 5, 4, 3));
  BOOST_TEST(testFL(S("ktdor"), "kipnf", 5, 5, 0));
  BOOST_TEST(testFL(S("ldprn"), "hmrnqdgifl", 5, 0, S::npos));
  BOOST_TEST(testFL(S("egmjk"), "fsmjcdairn", 5, 1, S::npos));
  BOOST_TEST(testFL(S("armql"), "pcdgltbrfj", 5, 5, 4));
  BOOST_TEST(testFL(S("cdhjo"), "aekfctpirg", 5, 9, 0));
  BOOST_TEST(testFL(S("jcons"), "ledihrsgpf", 5, 10, 4));
  BOOST_TEST(testFL(S("cbrkp"), "mqcklahsbtirgopefndj", 5, 0, S::npos));
  BOOST_TEST(testFL(S("fhgna"), "kmlthaoqgecrnpdbjfis", 5, 1, S::npos));
  BOOST_TEST(testFL(S("ejfcd"), "sfhbamcdptojlkrenqgi", 5, 10, 4));
  BOOST_TEST(testFL(S("kqjhe"), "pbniofmcedrkhlstgaqj", 5, 19, 4));
  BOOST_TEST(testFL(S("pbdjl"), "mongjratcskbhqiepfdl", 5, 20, 4));
  BOOST_TEST(testFL(S("gajqn"), "", 6, 0, S::npos));
  BOOST_TEST(testFL(S("stedk"), "hrnat", 6, 0, S::npos));
  BOOST_TEST(testFL(S("tjkaf"), "gsqdt", 6, 1, S::npos));
  BOOST_TEST(testFL(S("dthpe"), "bspkd", 6, 2, S::npos));
  BOOST_TEST(testFL(S("klhde"), "ohcmb", 6, 4, 2));
  BOOST_TEST(testFL(S("bhlki"), "heatr", 6, 5, 1));
  BOOST_TEST(testFL(S("lqmoh"), "pmblckedfn", 6, 0, S::npos));
  BOOST_TEST(testFL(S("mtqin"), "aceqmsrbik", 6, 1, S::npos));
  BOOST_TEST(testFL(S("dpqbr"), "lmbtdehjrn", 6, 5, 3));
  BOOST_TEST(testFL(S("kdhmo"), "teqmcrlgib", 6, 9, 3));
  BOOST_TEST(testFL(S("jblqp"), "njolbmspac", 6, 10, 4));
  BOOST_TEST(testFL(S("qmjgl"), "pofnhidklamecrbqjgst", 6, 0, S::npos));
  BOOST_TEST(testFL(S("rothp"), "jbhckmtgrqnosafedpli", 6, 1, S::npos));
  BOOST_TEST(testFL(S("ghknq"), "dobntpmqklicsahgjerf", 6, 10, 4));
  BOOST_TEST(testFL(S("eopfi"), "tpdshainjkbfoemlrgcq", 6, 19, 4));
  BOOST_TEST(testFL(S("dsnmg"), "oldpfgeakrnitscbjmqh", 6, 20, 4));
  BOOST_TEST(testFL(S("jnkrfhotgl"), "", 0, 0, S::npos));
  BOOST_TEST(testFL(S("dltjfngbko"), "rqegt", 0, 0, S::npos));
  BOOST_TEST(testFL(S("bmjlpkiqde"), "dashm", 0, 1, S::npos));
  BOOST_TEST(testFL(S("skrflobnqm"), "jqirk", 0, 2, S::npos));
  BOOST_TEST(testFL(S("jkpldtshrm"), "rckeg", 0, 4, S::npos));
  BOOST_TEST(testFL(S("ghasdbnjqo"), "jscie", 0, 5, S::npos));
  BOOST_TEST(testFL(S("igrkhpbqjt"), "efsphndliq", 0, 0, S::npos));
  BOOST_TEST(testFL(S("ikthdgcamf"), "gdicosleja", 0, 1, S::npos));
  BOOST_TEST(testFL(S("pcofgeniam"), "qcpjibosfl", 0, 5, 0));
  BOOST_TEST(testFL(S("rlfjgesqhc"), "lrhmefnjcq", 0, 9, 0));
  BOOST_TEST(testFL(S("itphbqsker"), "dtablcrseo", 0, 10, S::npos));
  BOOST_TEST(testFL(S("skjafcirqm"), "apckjsftedbhgomrnilq", 0, 0, S::npos));
  BOOST_TEST(testFL(S("tcqomarsfd"), "pcbrgflehjtiadnsokqm", 0, 1, S::npos));
  BOOST_TEST(testFL(S("rocfeldqpk"), "nsiadegjklhobrmtqcpf", 0, 10, S::npos));
  BOOST_TEST(testFL(S("cfpegndlkt"), "cpmajdqnolikhgsbretf", 0, 19, 0));
  BOOST_TEST(testFL(S("fqbtnkeasj"), "jcflkntmgiqrphdosaeb", 0, 20, 0));
  BOOST_TEST(testFL(S("shbcqnmoar"), "", 1, 0, S::npos));
  BOOST_TEST(testFL(S("bdoshlmfin"), "ontrs", 1, 0, S::npos));
  BOOST_TEST(testFL(S("khfrebnsgq"), "pfkna", 1, 1, S::npos));
  BOOST_TEST(testFL(S("getcrsaoji"), "ekosa", 1, 2, 1));
  BOOST_TEST(testFL(S("fjiknedcpq"), "anqhk", 1, 4, S::npos));
  BOOST_TEST(testFL(S("tkejgnafrm"), "jekca", 1, 5, 1));
  BOOST_TEST(testFL(S("jnakolqrde"), "ikemsjgacf", 1, 0, S::npos));
  BOOST_TEST(testFL(S("lcjptsmgbe"), "arolgsjkhm", 1, 1, S::npos));
  BOOST_TEST(testFL(S("itfsmcjorl"), "oftkbldhre", 1, 5, 1));
  BOOST_TEST(testFL(S("omchkfrjea"), "gbkqdoeftl", 1, 9, 0));
  BOOST_TEST(testFL(S("cigfqkated"), "sqcflrgtim", 1, 10, 1));
  BOOST_TEST(testFL(S("tscenjikml"), "fmhbkislrjdpanogqcet", 1, 0, S::npos));
  BOOST_TEST(testFL(S("qcpaemsinf"), "rnioadktqlgpbcjsmhef", 1, 1, S::npos));
  BOOST_TEST(testFL(S("gltkojeipd"), "oakgtnldpsefihqmjcbr", 1, 10, 1));
  BOOST_TEST(testFL(S("qistfrgnmp"), "gbnaelosidmcjqktfhpr", 1, 19, 1));
  BOOST_TEST(testFL(S("bdnpfcqaem"), "akbripjhlosndcmqgfet", 1, 20, 1));
  BOOST_TEST(testFL(S("ectnhskflp"), "", 5, 0, S::npos));
  BOOST_TEST(testFL(S("fgtianblpq"), "pijag", 5, 0, S::npos));
  BOOST_TEST(testFL(S("mfeqklirnh"), "jrckd", 5, 1, S::npos));
  BOOST_TEST(testFL(S("astedncjhk"), "qcloh", 5, 2, S::npos));
  BOOST_TEST(testFL(S("fhlqgcajbr"), "thlmp", 5, 4, 2));
  BOOST_TEST(testFL(S("epfhocmdng"), "qidmo", 5, 5, 4));
  BOOST_TEST(testFL(S("apcnsibger"), "lnegpsjqrd", 5, 0, S::npos));
  BOOST_TEST(testFL(S("aqkocrbign"), "rjqdablmfs", 5, 1, 5));
  BOOST_TEST(testFL(S("ijsmdtqgce"), "enkgpbsjaq", 5, 5, S::npos));
  BOOST_TEST(testFL(S("clobgsrken"), "kdsgoaijfh", 5, 9, 5));
  BOOST_TEST(testFL(S("jbhcfposld"), "trfqgmckbe", 5, 10, 4));
  BOOST_TEST(testFL(S("oqnpblhide"), "igetsracjfkdnpoblhqm", 5, 0, S::npos));
  BOOST_TEST(testFL(S("lroeasctif"), "nqctfaogirshlekbdjpm", 5, 1, S::npos));
  BOOST_TEST(testFL(S("bpjlgmiedh"), "csehfgomljdqinbartkp", 5, 10, 5));
  BOOST_TEST(testFL(S("pamkeoidrj"), "qahoegcmplkfsjbdnitr", 5, 19, 5));
  BOOST_TEST(testFL(S("espogqbthk"), "dpteiajrqmsognhlfbkc", 5, 20, 5));
  BOOST_TEST(testFL(S("shoiedtcjb"), "", 9, 0, S::npos));
  BOOST_TEST(testFL(S("ebcinjgads"), "tqbnh", 9, 0, S::npos));
  BOOST_TEST(testFL(S("dqmregkcfl"), "akmle", 9, 1, S::npos));
  BOOST_TEST(testFL(S("ngcrieqajf"), "iqfkm", 9, 2, 6));
  BOOST_TEST(testFL(S("qosmilgnjb"), "tqjsr", 9, 4, 8));
  BOOST_TEST(testFL(S("ikabsjtdfl"), "jplqg", 9, 5, 9));
  BOOST_TEST(testFL(S("ersmicafdh"), "oilnrbcgtj", 9, 0, S::npos));
  BOOST_TEST(testFL(S("fdnplotmgh"), "morkglpesn", 9, 1, 7));
  BOOST_TEST(testFL(S("fdbicojerm"), "dmicerngat", 9, 5, 9));
  BOOST_TEST(testFL(S("mbtafndjcq"), "radgeskbtc", 9, 9, 6));
  BOOST_TEST(testFL(S("mlenkpfdtc"), "ljikprsmqo", 9, 10, 5));
  BOOST_TEST(testFL(S("ahlcifdqgs"), "trqihkcgsjamfdbolnpe", 9, 0, S::npos));
  BOOST_TEST(testFL(S("bgjemaltks"), "lqmthbsrekajgnofcipd", 9, 1, 6));
  BOOST_TEST(testFL(S("pdhslbqrfc"), "jtalmedribkgqsopcnfh", 9, 10, 7));
  BOOST_TEST(testFL(S("dirhtsnjkc"), "spqfoiclmtagejbndkrh", 9, 19, 9));
  BOOST_TEST(testFL(S("dlroktbcja"), "nmotklspigjrdhcfaebq", 9, 20, 9));
  BOOST_TEST(testFL(S("ncjpmaekbs"), "", 10, 0, S::npos));
  BOOST_TEST(testFL(S("hlbosgmrak"), "hpmsd", 10, 0, S::npos));
  BOOST_TEST(testFL(S("pqfhsgilen"), "qnpor", 10, 1, 1));
  BOOST_TEST(testFL(S("gqtjsbdckh"), "otdma", 10, 2, 2));
  BOOST_TEST(testFL(S("cfkqpjlegi"), "efhjg", 10, 4, 7));
  BOOST_TEST(testFL(S("beanrfodgj"), "odpte", 10, 5, 7));
  BOOST_TEST(testFL(S("adtkqpbjfi"), "bctdgfmolr", 10, 0, S::npos));
  BOOST_TEST(testFL(S("iomkfthagj"), "oaklidrbqg", 10, 1, 1));
  BOOST_TEST(testFL(S("sdpcilonqj"), "dnjfsagktr", 10, 5, 9));
  BOOST_TEST(testFL(S("gtfbdkqeml"), "nejaktmiqg", 10, 9, 8));
  BOOST_TEST(testFL(S("bmeqgcdorj"), "pjqonlebsf", 10, 10, 9));
  BOOST_TEST(testFL(S("etqlcanmob"), "dshmnbtolcjepgaikfqr", 10, 0, S::npos));
  BOOST_TEST(testFL(S("roqmkbdtia"), "iogfhpabtjkqlrnemcds", 10, 1, 8));
  BOOST_TEST(testFL(S("kadsithljf"), "ngridfabjsecpqltkmoh", 10, 10, 9));
  BOOST_TEST(testFL(S("sgtkpbfdmh"), "athmknplcgofrqejsdib", 10, 19, 9));
  BOOST_TEST(testFL(S("qgmetnabkl"), "ldobhmqcafnjtkeisgrp", 10, 20, 9));
  BOOST_TEST(testFL(S("cqjohampgd"), "", 11, 0, S::npos));
  BOOST_TEST(testFL(S("hobitmpsan"), "aocjb", 11, 0, S::npos));
  BOOST_TEST(testFL(S("tjehkpsalm"), "jbrnk", 11, 1, 1));
  BOOST_TEST(testFL(S("ngfbojitcl"), "tqedg", 11, 2, 7));
  BOOST_TEST(testFL(S("rcfkdbhgjo"), "nqskp", 11, 4, 3));
  BOOST_TEST(testFL(S("qghptonrea"), "eaqkl", 11, 5, 9));
  BOOST_TEST(testFL(S("hnprfgqjdl"), "reaoicljqm", 11, 0, S::npos));
  BOOST_TEST(testFL(S("hlmgabenti"), "lsftgajqpm", 11, 1, 1));
  BOOST_TEST(testFL(S("ofcjanmrbs"), "rlpfogmits", 11, 5, 7));
  BOOST_TEST(testFL(S("jqedtkornm"), "shkncmiaqj", 11, 9, 9));
  BOOST_TEST(testFL(S("rfedlasjmg"), "fpnatrhqgs", 11, 10, 9));
  BOOST_TEST(testFL(S("talpqjsgkm"), "sjclemqhnpdbgikarfot", 11, 0, S::npos));
  BOOST_TEST(testFL(S("lrkcbtqpie"), "otcmedjikgsfnqbrhpla", 11, 1, S::npos));
  BOOST_TEST(testFL(S("cipogdskjf"), "bonsaefdqiprkhlgtjcm", 11, 10, 9));
  BOOST_TEST(testFL(S("nqedcojahi"), "egpscmahijlfnkrodqtb", 11, 19, 9));
  BOOST_TEST(testFL(S("hefnrkmctj"), "kmqbfepjthgilscrndoa", 11, 20, 9));
  BOOST_TEST(testFL(S("atqirnmekfjolhpdsgcb"), "", 0, 0, S::npos));
  BOOST_TEST(testFL(S("echfkmlpribjnqsaogtd"), "prboq", 0, 0, S::npos));
  BOOST_TEST(testFL(S("qnhiftdgcleajbpkrosm"), "fjcqh", 0, 1, S::npos));
  BOOST_TEST(testFL(S("chamfknorbedjitgslpq"), "fmosa", 0, 2, S::npos));
  BOOST_TEST(testFL(S("njhqpibfmtlkaecdrgso"), "qdbok", 0, 4, S::npos));
  BOOST_TEST(testFL(S("ebnghfsqkprmdcljoiat"), "amslg", 0, 5, S::npos));
  BOOST_TEST(testFL(S("letjomsgihfrpqbkancd"), "smpltjneqb", 0, 0, S::npos));
  BOOST_TEST(testFL(S("nblgoipcrqeaktshjdmf"), "flitskrnge", 0, 1, S::npos));
  BOOST_TEST(testFL(S("cehkbngtjoiflqapsmrd"), "pgqihmlbef", 0, 5, S::npos));
  BOOST_TEST(testFL(S("mignapfoklbhcqjetdrs"), "cfpdqjtgsb", 0, 9, S::npos));
  BOOST_TEST(testFL(S("ceatbhlsqjgpnokfrmdi"), "htpsiaflom", 0, 10, S::npos));
  BOOST_TEST(testFL(S("ocihkjgrdelpfnmastqb"), "kpjfiaceghsrdtlbnomq", 0, 0, S::npos));
  BOOST_TEST(testFL(S("noelgschdtbrjfmiqkap"), "qhtbomidljgafneksprc", 0, 1, S::npos));
  BOOST_TEST(testFL(S("dkclqfombepritjnghas"), "nhtjobkcefldimpsaqgr", 0, 10, S::npos));
  BOOST_TEST(testFL(S("miklnresdgbhqcojftap"), "prabcjfqnoeskilmtgdh", 0, 19, 0));
  BOOST_TEST(testFL(S("htbcigojaqmdkfrnlsep"), "dtrgmchilkasqoebfpjn", 0, 20, 0));
  BOOST_TEST(testFL(S("febhmqtjanokscdirpgl"), "", 1, 0, S::npos));
  BOOST_TEST(testFL(S("loakbsqjpcrdhftniegm"), "sqome", 1, 0, S::npos));
  BOOST_TEST(testFL(S("reagphsqflbitdcjmkno"), "smfte", 1, 1, S::npos));
  BOOST_TEST(testFL(S("jitlfrqemsdhkopncabg"), "ciboh", 1, 2, 1));
  BOOST_TEST(testFL(S("mhtaepscdnrjqgbkifol"), "haois", 1, 4, 1));
  BOOST_TEST(testFL(S("tocesrfmnglpbjihqadk"), "abfki", 1, 5, S::npos));
  BOOST_TEST(testFL(S("lpfmctjrhdagneskbqoi"), "frdkocntmq", 1, 0, S::npos));
  BOOST_TEST(testFL(S("lsmqaepkdhncirbtjfgo"), "oasbpedlnr", 1, 1, S::npos));
  BOOST_TEST(testFL(S("epoiqmtldrabnkjhcfsg"), "kltqmhgand", 1, 5, S::npos));
  BOOST_TEST(testFL(S("emgasrilpknqojhtbdcf"), "gdtfjchpmr", 1, 9, 1));
  BOOST_TEST(testFL(S("hnfiagdpcklrjetqbsom"), "ponmcqblet", 1, 10, 1));
  BOOST_TEST(testFL(S("nsdfebgajhmtricpoklq"), "sgphqdnofeiklatbcmjr", 1, 0, S::npos));
  BOOST_TEST(testFL(S("atjgfsdlpobmeiqhncrk"), "ljqprsmigtfoneadckbh", 1, 1, S::npos));
  BOOST_TEST(testFL(S("sitodfgnrejlahcbmqkp"), "ligeojhafnkmrcsqtbdp", 1, 10, 1));
  BOOST_TEST(testFL(S("fraghmbiceknltjpqosd"), "lsimqfnjarbopedkhcgt", 1, 19, 1));
  BOOST_TEST(testFL(S("pmafenlhqtdbkirjsogc"), "abedmfjlghniorcqptks", 1, 20, 1));
  BOOST_TEST(testFL(S("pihgmoeqtnakrjslcbfd"), "", 10, 0, S::npos));
  BOOST_TEST(testFL(S("gjdkeprctqblnhiafsom"), "hqtoa", 10, 0, S::npos));
  BOOST_TEST(testFL(S("mkpnblfdsahrcqijteog"), "cahif", 10, 1, S::npos));
  BOOST_TEST(testFL(S("gckarqnelodfjhmbptis"), "kehis", 10, 2, 7));
  BOOST_TEST(testFL(S("gqpskidtbclomahnrjfe"), "kdlmh", 10, 4, 10));
  BOOST_TEST(testFL(S("pkldjsqrfgitbhmaecno"), "paeql", 10, 5, 6));
  BOOST_TEST(testFL(S("aftsijrbeklnmcdqhgop"), "aghoqiefnb", 10, 0, S::npos));
  BOOST_TEST(testFL(S("mtlgdrhafjkbiepqnsoc"), "jrbqaikpdo", 10, 1, 9));
  BOOST_TEST(testFL(S("pqgirnaefthokdmbsclj"), "smjonaeqcl", 10, 5, 5));
  BOOST_TEST(testFL(S("kpdbgjmtherlsfcqoina"), "eqbdrkcfah", 10, 9, 10));
  BOOST_TEST(testFL(S("jrlbothiknqmdgcfasep"), "kapmsienhf", 10, 10, 9));
  BOOST_TEST(testFL(S("mjogldqferckabinptsh"), "jpqotrlenfcsbhkaimdg", 10, 0, S::npos));
  BOOST_TEST(testFL(S("apoklnefbhmgqcdrisjt"), "jlbmhnfgtcqprikeados", 10, 1, S::npos));
  BOOST_TEST(testFL(S("ifeopcnrjbhkdgatmqls"), "stgbhfmdaljnpqoicker", 10, 10, 10));
  BOOST_TEST(testFL(S("ckqhaiesmjdnrgolbtpf"), "oihcetflbjagdsrkmqpn", 10, 19, 10));
  BOOST_TEST(testFL(S("bnlgapfimcoterskqdjh"), "adtclebmnpjsrqfkigoh", 10, 20, 10));
  BOOST_TEST(testFL(S("kgdlrobpmjcthqsafeni"), "", 19, 0, S::npos));
  BOOST_TEST(testFL(S("dfkechomjapgnslbtqir"), "beafg", 19, 0, S::npos));
  BOOST_TEST(testFL(S("rloadknfbqtgmhcsipje"), "iclat", 19, 1, 16));
  BOOST_TEST(testFL(S("mgjhkolrnadqbpetcifs"), "rkhnf", 19, 2, 7));
  BOOST_TEST(testFL(S("cmlfakiojdrgtbsphqen"), "clshq", 19, 4, 16));
  BOOST_TEST(testFL(S("kghbfipeomsntdalrqjc"), "dtcoj", 19, 5, 19));
  BOOST_TEST(testFL(S("eldiqckrnmtasbghjfpo"), "rqosnjmfth", 19, 0, S::npos));
  BOOST_TEST(testFL(S("abqjcfedgotihlnspkrm"), "siatdfqglh", 19, 1, 15));
  BOOST_TEST(testFL(S("qfbadrtjsimkolcenhpg"), "mrlshtpgjq", 19, 5, 17));
  BOOST_TEST(testFL(S("abseghclkjqifmtodrnp"), "adlcskgqjt", 19, 9, 16));
  BOOST_TEST(testFL(S("ibmsnlrjefhtdokacqpg"), "drshcjknaf", 19, 10, 16));
  BOOST_TEST(testFL(S("mrkfciqjebaponsthldg"), "etsaqroinghpkjdlfcbm", 19, 0, S::npos));
  BOOST_TEST(testFL(S("mjkticdeoqshpalrfbgn"), "sgepdnkqliambtrocfhj", 19, 1, 10));
  BOOST_TEST(testFL(S("rqnoclbdejgiphtfsakm"), "nlmcjaqgbsortfdihkpe", 19, 10, 19));
  BOOST_TEST(testFL(S("plkqbhmtfaeodjcrsing"), "racfnpmosldibqkghjet", 19, 19, 19));
  BOOST_TEST(testFL(S("oegalhmstjrfickpbndq"), "fjhdsctkqeiolagrnmbp", 19, 20, 19));
  BOOST_TEST(testFL(S("rdtgjcaohpblniekmsfq"), "", 20, 0, S::npos));
  BOOST_TEST(testFL(S("ofkqbnjetrmsaidphglc"), "ejanp", 20, 0, S::npos));
  BOOST_TEST(testFL(S("grkpahljcftesdmonqib"), "odife", 20, 1, 15));
  BOOST_TEST(testFL(S("jimlgbhfqkteospardcn"), "okaqd", 20, 2, 12));
  BOOST_TEST(testFL(S("gftenihpmslrjkqadcob"), "lcdbi", 20, 4, 19));
  BOOST_TEST(testFL(S("bmhldogtckrfsanijepq"), "fsqbj", 20, 5, 19));
  BOOST_TEST(testFL(S("nfqkrpjdesabgtlcmoih"), "bigdomnplq", 20, 0, S::npos));
  BOOST_TEST(testFL(S("focalnrpiqmdkstehbjg"), "apiblotgcd", 20, 1, 3));
  BOOST_TEST(testFL(S("rhqdspkmebiflcotnjga"), "acfhdenops", 20, 5, 19));
  BOOST_TEST(testFL(S("rahdtmsckfboqlpniegj"), "jopdeamcrk", 20, 9, 19));
  BOOST_TEST(testFL(S("fbkeiopclstmdqranjhg"), "trqncbkgmh", 20, 10, 19));
  BOOST_TEST(testFL(S("lifhpdgmbconstjeqark"), "tomglrkencbsfjqpihda", 20, 0, S::npos));
  BOOST_TEST(testFL(S("pboqganrhedjmltsicfk"), "gbkhdnpoietfcmrslajq", 20, 1, 4));
  BOOST_TEST(testFL(S("klchabsimetjnqgorfpd"), "rtfnmbsglkjaichoqedp", 20, 10, 17));
  BOOST_TEST(testFL(S("sirfgmjqhctndbklaepo"), "ohkmdpfqbsacrtjnlgei", 20, 19, 19));
  BOOST_TEST(testFL(S("rlbdsiceaonqjtfpghkm"), "dlbrteoisgphmkncajfq", 20, 20, 19));
  BOOST_TEST(testFL(S("ecgdanriptblhjfqskom"), "", 21, 0, S::npos));
  BOOST_TEST(testFL(S("fdmiarlpgcskbhoteqjn"), "sjrlo", 21, 0, S::npos));
  BOOST_TEST(testFL(S("rlbstjqopignecmfadkh"), "qjpor", 21, 1, 6));
  BOOST_TEST(testFL(S("grjpqmbshektdolcafni"), "odhfn", 21, 2, 13));
  BOOST_TEST(testFL(S("sakfcohtqnibprjmlged"), "qtfin", 21, 4, 10));
  BOOST_TEST(testFL(S("mjtdglasihqpocebrfkn"), "hpqfo", 21, 5, 17));
  BOOST_TEST(testFL(S("okaplfrntghqbmeicsdj"), "fabmertkos", 21, 0, S::npos));
  BOOST_TEST(testFL(S("sahngemrtcjidqbklfpo"), "brqtgkmaej", 21, 1, 14));
  BOOST_TEST(testFL(S("dlmsipcnekhbgoaftqjr"), "nfrdeihsgl", 21, 5, 19));
  BOOST_TEST(testFL(S("ahegrmqnoiklpfsdbcjt"), "hlfrosekpi", 21, 9, 14));
  BOOST_TEST(testFL(S("hdsjbnmlegtkqripacof"), "atgbkrjdsm", 21, 10, 16));
  BOOST_TEST(testFL(S("pcnedrfjihqbalkgtoms"), "blnrptjgqmaifsdkhoec", 21, 0, S::npos));
  BOOST_TEST(testFL(S("qjidealmtpskrbfhocng"), "ctpmdahebfqjgknloris", 21, 1, 17));
  BOOST_TEST(testFL(S("qeindtagmokpfhsclrbj"), "apnkeqthrmlbfodiscgj", 21, 10, 17));
  BOOST_TEST(testFL(S("kpfegbjhsrnodltqciam"), "jdgictpframeoqlsbknh", 21, 19, 19));
  BOOST_TEST(testFL(S("hnbrcplsjfgiktoedmaq"), "qprlsfojamgndekthibc", 21, 20, 19));






  
  // find_first_not_of

  const char* cs3 = "12456";
  const char* cs4 = "2356";
  string_view v3 = cs3;
  string_view v4 = cs4;
  static_string<5> fs3 = cs3;
  static_string<4> fs4 = cs4;

  BOOST_TEST(fs1.find_first_not_of(v3) == 2);
  BOOST_TEST(fs1.find_first_not_of(v4) == 0);
  BOOST_TEST(fs1.find_first_not_of(fs3) == 2);
  BOOST_TEST(fs1.find_first_not_of(fs4) == 0);

  BOOST_TEST(fs1.find_first_not_of(cs3) == 2);
  BOOST_TEST(fs1.find_first_not_of(cs4) == 0);

  BOOST_TEST(fs1.find_first_not_of(cs3, 0) == 2);
  BOOST_TEST(fs1.find_first_not_of(cs4, 0) == 0);

  BOOST_TEST(fs1.find_first_not_of(cs4, 0, 2) == 0);

  BOOST_TEST(fs1.find_first_not_of(cs3, 4) == S::npos);
  BOOST_TEST(fs1.find_first_not_of(cs4, 4) == S::npos);

  BOOST_TEST(fs1.find_first_not_of('1') == 1);
  BOOST_TEST(fs1.find_first_not_of('1', 3) == 3);





  BOOST_TEST(testFFN(S(""), "", 0, 0, S::npos));
  BOOST_TEST(testFFN(S(""), "irkhs", 0, 0, S::npos));
  BOOST_TEST(testFFN(S(""), "kante", 0, 1, S::npos));
  BOOST_TEST(testFFN(S(""), "oknlr", 0, 2, S::npos));
  BOOST_TEST(testFFN(S(""), "pcdro", 0, 4, S::npos));
  BOOST_TEST(testFFN(S(""), "bnrpe", 0, 5, S::npos));
  BOOST_TEST(testFFN(S(""), "jtdaefblso", 0, 0, S::npos));
  BOOST_TEST(testFFN(S(""), "oselktgbca", 0, 1, S::npos));
  BOOST_TEST(testFFN(S(""), "eqgaplhckj", 0, 5, S::npos));
  BOOST_TEST(testFFN(S(""), "bjahtcmnlp", 0, 9, S::npos));
  BOOST_TEST(testFFN(S(""), "hjlcmgpket", 0, 10, S::npos));
  BOOST_TEST(testFFN(S(""), "htaobedqikfplcgjsmrn", 0, 0, S::npos));
  BOOST_TEST(testFFN(S(""), "hpqiarojkcdlsgnmfetb", 0, 1, S::npos));
  BOOST_TEST(testFFN(S(""), "dfkaprhjloqetcsimnbg", 0, 10, S::npos));
  BOOST_TEST(testFFN(S(""), "ihqrfebgadntlpmjksoc", 0, 19, S::npos));
  BOOST_TEST(testFFN(S(""), "ngtjfcalbseiqrphmkdo", 0, 20, S::npos));
  BOOST_TEST(testFFN(S(""), "", 1, 0, S::npos));
  BOOST_TEST(testFFN(S(""), "lbtqd", 1, 0, S::npos));
  BOOST_TEST(testFFN(S(""), "tboim", 1, 1, S::npos));
  BOOST_TEST(testFFN(S(""), "slcer", 1, 2, S::npos));
  BOOST_TEST(testFFN(S(""), "cbjfs", 1, 4, S::npos));
  BOOST_TEST(testFFN(S(""), "aqibs", 1, 5, S::npos));
  BOOST_TEST(testFFN(S(""), "gtfblmqinc", 1, 0, S::npos));
  BOOST_TEST(testFFN(S(""), "mkqpbtdalg", 1, 1, S::npos));
  BOOST_TEST(testFFN(S(""), "kphatlimcd", 1, 5, S::npos));
  BOOST_TEST(testFFN(S(""), "pblasqogic", 1, 9, S::npos));
  BOOST_TEST(testFFN(S(""), "arosdhcfme", 1, 10, S::npos));
  BOOST_TEST(testFFN(S(""), "blkhjeogicatqfnpdmsr", 1, 0, S::npos));
  BOOST_TEST(testFFN(S(""), "bmhineprjcoadgstflqk", 1, 1, S::npos));
  BOOST_TEST(testFFN(S(""), "djkqcmetslnghpbarfoi", 1, 10, S::npos));
  BOOST_TEST(testFFN(S(""), "lgokshjtpbemarcdqnfi", 1, 19, S::npos));
  BOOST_TEST(testFFN(S(""), "bqjhtkfepimcnsgrlado", 1, 20, S::npos));
  BOOST_TEST(testFFN(S("eaint"), "", 0, 0, 0));
  BOOST_TEST(testFFN(S("binja"), "gfsrt", 0, 0, 0));
  BOOST_TEST(testFFN(S("latkm"), "pfsoc", 0, 1, 0));
  BOOST_TEST(testFFN(S("lecfr"), "tpflm", 0, 2, 0));
  BOOST_TEST(testFFN(S("eqkst"), "sgkec", 0, 4, 1));
  BOOST_TEST(testFFN(S("cdafr"), "romds", 0, 5, 0));
  BOOST_TEST(testFFN(S("prbhe"), "qhjistlgmr", 0, 0, 0));
  BOOST_TEST(testFFN(S("lbisk"), "pedfirsglo", 0, 1, 0));
  BOOST_TEST(testFFN(S("hrlpd"), "aqcoslgrmk", 0, 5, 0));
  BOOST_TEST(testFFN(S("ehmja"), "dabckmepqj", 0, 9, 1));
  BOOST_TEST(testFFN(S("mhqgd"), "pqscrjthli", 0, 10, 0));
  BOOST_TEST(testFFN(S("tgklq"), "kfphdcsjqmobliagtren", 0, 0, 0));
  BOOST_TEST(testFFN(S("bocjs"), "rokpefncljibsdhqtagm", 0, 1, 0));
  BOOST_TEST(testFFN(S("grbsd"), "afionmkphlebtcjqsgrd", 0, 10, 0));
  BOOST_TEST(testFFN(S("ofjqr"), "aenmqplidhkofrjbctsg", 0, 19, S::npos));
  BOOST_TEST(testFFN(S("btlfi"), "osjmbtcadhiklegrpqnf", 0, 20, S::npos));
  BOOST_TEST(testFFN(S("clrgb"), "", 1, 0, 1));
  BOOST_TEST(testFFN(S("tjmek"), "osmia", 1, 0, 1));
  BOOST_TEST(testFFN(S("bgstp"), "ckonl", 1, 1, 1));
  BOOST_TEST(testFFN(S("hstrk"), "ilcaj", 1, 2, 1));
  BOOST_TEST(testFFN(S("kmspj"), "lasiq", 1, 4, 1));
  BOOST_TEST(testFFN(S("tjboh"), "kfqmr", 1, 5, 1));
  BOOST_TEST(testFFN(S("ilbcj"), "klnitfaobg", 1, 0, 1));
  BOOST_TEST(testFFN(S("jkngf"), "gjhmdlqikp", 1, 1, 1));
  BOOST_TEST(testFFN(S("gfcql"), "skbgtahqej", 1, 5, 1));
  BOOST_TEST(testFFN(S("dqtlg"), "bjsdgtlpkf", 1, 9, 1));
  BOOST_TEST(testFFN(S("bthpg"), "bjgfmnlkio", 1, 10, 1));
  BOOST_TEST(testFFN(S("dgsnq"), "lbhepotfsjdqigcnamkr", 1, 0, 1));
  BOOST_TEST(testFFN(S("rmfhp"), "tebangckmpsrqdlfojhi", 1, 1, 1));
  BOOST_TEST(testFFN(S("jfdam"), "joflqbdkhtegimscpanr", 1, 10, 3));
  BOOST_TEST(testFFN(S("edapb"), "adpmcohetfbsrjinlqkg", 1, 19, S::npos));
  BOOST_TEST(testFFN(S("brfsm"), "iacldqjpfnogbsrhmetk", 1, 20, S::npos));
  BOOST_TEST(testFFN(S("ndrhl"), "", 2, 0, 2));
  BOOST_TEST(testFFN(S("mrecp"), "otkgb", 2, 0, 2));
  BOOST_TEST(testFFN(S("qlasf"), "cqsjl", 2, 1, 2));
  BOOST_TEST(testFFN(S("smaqd"), "dpifl", 2, 2, 2));
  BOOST_TEST(testFFN(S("hjeni"), "oapht", 2, 4, 2));
  BOOST_TEST(testFFN(S("ocmfj"), "cifts", 2, 5, 2));
  BOOST_TEST(testFFN(S("hmftq"), "nmsckbgalo", 2, 0, 2));
  BOOST_TEST(testFFN(S("fklad"), "tpksqhamle", 2, 1, 2));
  BOOST_TEST(testFFN(S("dirnm"), "tpdrchmkji", 2, 5, 3));
  BOOST_TEST(testFFN(S("hrgdc"), "ijagfkblst", 2, 9, 3));
  BOOST_TEST(testFFN(S("ifakg"), "kpocsignjb", 2, 10, 2));
  BOOST_TEST(testFFN(S("ebrgd"), "pecqtkjsnbdrialgmohf", 2, 0, 2));
  BOOST_TEST(testFFN(S("rcjml"), "aiortphfcmkjebgsndql", 2, 1, 2));
  BOOST_TEST(testFFN(S("peqmt"), "sdbkeamglhipojqftrcn", 2, 10, 2));
  BOOST_TEST(testFFN(S("frehn"), "ljqncehgmfktroapidbs", 2, 19, S::npos));
  BOOST_TEST(testFFN(S("tqolf"), "rtcfodilamkbenjghqps", 2, 20, S::npos));
  BOOST_TEST(testFFN(S("cjgao"), "", 4, 0, 4));
  BOOST_TEST(testFFN(S("kjplq"), "mabns", 4, 0, 4));
  BOOST_TEST(testFFN(S("herni"), "bdnrp", 4, 1, 4));
  BOOST_TEST(testFFN(S("tadrb"), "scidp", 4, 2, 4));
  BOOST_TEST(testFFN(S("pkfeo"), "agbjl", 4, 4, 4));
  BOOST_TEST(testFFN(S("hoser"), "jfmpr", 4, 5, S::npos));
  BOOST_TEST(testFFN(S("kgrsp"), "rbpefghsmj", 4, 0, 4));
  BOOST_TEST(testFFN(S("pgejb"), "apsfntdoqc", 4, 1, 4));
  BOOST_TEST(testFFN(S("thlnq"), "ndkjeisgcl", 4, 5, 4));
  BOOST_TEST(testFFN(S("nbmit"), "rnfpqatdeo", 4, 9, S::npos));
  BOOST_TEST(testFFN(S("jgmib"), "bntjlqrfik", 4, 10, S::npos));
  BOOST_TEST(testFFN(S("ncrfj"), "kcrtmpolnaqejghsfdbi", 4, 0, 4));
  BOOST_TEST(testFFN(S("ncsik"), "lobheanpkmqidsrtcfgj", 4, 1, 4));
  BOOST_TEST(testFFN(S("sgbfh"), "athdkljcnreqbgpmisof", 4, 10, S::npos));
  BOOST_TEST(testFFN(S("dktbn"), "qkdmjialrscpbhefgont", 4, 19, S::npos));
  BOOST_TEST(testFFN(S("fthqm"), "dmasojntqleribkgfchp", 4, 20, S::npos));
  BOOST_TEST(testFFN(S("klopi"), "", 5, 0, S::npos));
  BOOST_TEST(testFFN(S("dajhn"), "psthd", 5, 0, S::npos));
  BOOST_TEST(testFFN(S("jbgno"), "rpmjd", 5, 1, S::npos));
  BOOST_TEST(testFFN(S("hkjae"), "dfsmk", 5, 2, S::npos));
  BOOST_TEST(testFFN(S("gbhqo"), "skqne", 5, 4, S::npos));
  BOOST_TEST(testFFN(S("ktdor"), "kipnf", 5, 5, S::npos));
  BOOST_TEST(testFFN(S("ldprn"), "hmrnqdgifl", 5, 0, S::npos));
  BOOST_TEST(testFFN(S("egmjk"), "fsmjcdairn", 5, 1, S::npos));
  BOOST_TEST(testFFN(S("armql"), "pcdgltbrfj", 5, 5, S::npos));
  BOOST_TEST(testFFN(S("cdhjo"), "aekfctpirg", 5, 9, S::npos));
  BOOST_TEST(testFFN(S("jcons"), "ledihrsgpf", 5, 10, S::npos));
  BOOST_TEST(testFFN(S("cbrkp"), "mqcklahsbtirgopefndj", 5, 0, S::npos));
  BOOST_TEST(testFFN(S("fhgna"), "kmlthaoqgecrnpdbjfis", 5, 1, S::npos));
  BOOST_TEST(testFFN(S("ejfcd"), "sfhbamcdptojlkrenqgi", 5, 10, S::npos));
  BOOST_TEST(testFFN(S("kqjhe"), "pbniofmcedrkhlstgaqj", 5, 19, S::npos));
  BOOST_TEST(testFFN(S("pbdjl"), "mongjratcskbhqiepfdl", 5, 20, S::npos));
  BOOST_TEST(testFFN(S("gajqn"), "", 6, 0, S::npos));
  BOOST_TEST(testFFN(S("stedk"), "hrnat", 6, 0, S::npos));
  BOOST_TEST(testFFN(S("tjkaf"), "gsqdt", 6, 1, S::npos));
  BOOST_TEST(testFFN(S("dthpe"), "bspkd", 6, 2, S::npos));
  BOOST_TEST(testFFN(S("klhde"), "ohcmb", 6, 4, S::npos));
  BOOST_TEST(testFFN(S("bhlki"), "heatr", 6, 5, S::npos));
  BOOST_TEST(testFFN(S("lqmoh"), "pmblckedfn", 6, 0, S::npos));
  BOOST_TEST(testFFN(S("mtqin"), "aceqmsrbik", 6, 1, S::npos));
  BOOST_TEST(testFFN(S("dpqbr"), "lmbtdehjrn", 6, 5, S::npos));
  BOOST_TEST(testFFN(S("kdhmo"), "teqmcrlgib", 6, 9, S::npos));
  BOOST_TEST(testFFN(S("jblqp"), "njolbmspac", 6, 10, S::npos));
  BOOST_TEST(testFFN(S("qmjgl"), "pofnhidklamecrbqjgst", 6, 0, S::npos));
  BOOST_TEST(testFFN(S("rothp"), "jbhckmtgrqnosafedpli", 6, 1, S::npos));
  BOOST_TEST(testFFN(S("ghknq"), "dobntpmqklicsahgjerf", 6, 10, S::npos));
  BOOST_TEST(testFFN(S("eopfi"), "tpdshainjkbfoemlrgcq", 6, 19, S::npos));
  BOOST_TEST(testFFN(S("dsnmg"), "oldpfgeakrnitscbjmqh", 6, 20, S::npos));
  BOOST_TEST(testFFN(S("jnkrfhotgl"), "", 0, 0, 0));
  BOOST_TEST(testFFN(S("dltjfngbko"), "rqegt", 0, 0, 0));
  BOOST_TEST(testFFN(S("bmjlpkiqde"), "dashm", 0, 1, 0));
  BOOST_TEST(testFFN(S("skrflobnqm"), "jqirk", 0, 2, 0));
  BOOST_TEST(testFFN(S("jkpldtshrm"), "rckeg", 0, 4, 0));
  BOOST_TEST(testFFN(S("ghasdbnjqo"), "jscie", 0, 5, 0));
  BOOST_TEST(testFFN(S("igrkhpbqjt"), "efsphndliq", 0, 0, 0));
  BOOST_TEST(testFFN(S("ikthdgcamf"), "gdicosleja", 0, 1, 0));
  BOOST_TEST(testFFN(S("pcofgeniam"), "qcpjibosfl", 0, 5, 2));
  BOOST_TEST(testFFN(S("rlfjgesqhc"), "lrhmefnjcq", 0, 9, 4));
  BOOST_TEST(testFFN(S("itphbqsker"), "dtablcrseo", 0, 10, 0));
  BOOST_TEST(testFFN(S("skjafcirqm"), "apckjsftedbhgomrnilq", 0, 0, 0));
  BOOST_TEST(testFFN(S("tcqomarsfd"), "pcbrgflehjtiadnsokqm", 0, 1, 0));
  BOOST_TEST(testFFN(S("rocfeldqpk"), "nsiadegjklhobrmtqcpf", 0, 10, 0));
  BOOST_TEST(testFFN(S("cfpegndlkt"), "cpmajdqnolikhgsbretf", 0, 19, 1));
  BOOST_TEST(testFFN(S("fqbtnkeasj"), "jcflkntmgiqrphdosaeb", 0, 20, S::npos));
  BOOST_TEST(testFFN(S("shbcqnmoar"), "", 1, 0, 1));
  BOOST_TEST(testFFN(S("bdoshlmfin"), "ontrs", 1, 0, 1));
  BOOST_TEST(testFFN(S("khfrebnsgq"), "pfkna", 1, 1, 1));
  BOOST_TEST(testFFN(S("getcrsaoji"), "ekosa", 1, 2, 2));
  BOOST_TEST(testFFN(S("fjiknedcpq"), "anqhk", 1, 4, 1));
  BOOST_TEST(testFFN(S("tkejgnafrm"), "jekca", 1, 5, 4));
  BOOST_TEST(testFFN(S("jnakolqrde"), "ikemsjgacf", 1, 0, 1));
  BOOST_TEST(testFFN(S("lcjptsmgbe"), "arolgsjkhm", 1, 1, 1));
  BOOST_TEST(testFFN(S("itfsmcjorl"), "oftkbldhre", 1, 5, 3));
  BOOST_TEST(testFFN(S("omchkfrjea"), "gbkqdoeftl", 1, 9, 1));
  BOOST_TEST(testFFN(S("cigfqkated"), "sqcflrgtim", 1, 10, 5));
  BOOST_TEST(testFFN(S("tscenjikml"), "fmhbkislrjdpanogqcet", 1, 0, 1));
  BOOST_TEST(testFFN(S("qcpaemsinf"), "rnioadktqlgpbcjsmhef", 1, 1, 1));
  BOOST_TEST(testFFN(S("gltkojeipd"), "oakgtnldpsefihqmjcbr", 1, 10, 5));
  BOOST_TEST(testFFN(S("qistfrgnmp"), "gbnaelosidmcjqktfhpr", 1, 19, 5));
  BOOST_TEST(testFFN(S("bdnpfcqaem"), "akbripjhlosndcmqgfet", 1, 20, S::npos));
  BOOST_TEST(testFFN(S("ectnhskflp"), "", 5, 0, 5));
  BOOST_TEST(testFFN(S("fgtianblpq"), "pijag", 5, 0, 5));
  BOOST_TEST(testFFN(S("mfeqklirnh"), "jrckd", 5, 1, 5));
  BOOST_TEST(testFFN(S("astedncjhk"), "qcloh", 5, 2, 5));
  BOOST_TEST(testFFN(S("fhlqgcajbr"), "thlmp", 5, 4, 5));
  BOOST_TEST(testFFN(S("epfhocmdng"), "qidmo", 5, 5, 5));
  BOOST_TEST(testFFN(S("apcnsibger"), "lnegpsjqrd", 5, 0, 5));
  BOOST_TEST(testFFN(S("aqkocrbign"), "rjqdablmfs", 5, 1, 6));
  BOOST_TEST(testFFN(S("ijsmdtqgce"), "enkgpbsjaq", 5, 5, 5));
  BOOST_TEST(testFFN(S("clobgsrken"), "kdsgoaijfh", 5, 9, 6));
  BOOST_TEST(testFFN(S("jbhcfposld"), "trfqgmckbe", 5, 10, 5));
  BOOST_TEST(testFFN(S("oqnpblhide"), "igetsracjfkdnpoblhqm", 5, 0, 5));
  BOOST_TEST(testFFN(S("lroeasctif"), "nqctfaogirshlekbdjpm", 5, 1, 5));
  BOOST_TEST(testFFN(S("bpjlgmiedh"), "csehfgomljdqinbartkp", 5, 10, 6));
  BOOST_TEST(testFFN(S("pamkeoidrj"), "qahoegcmplkfsjbdnitr", 5, 19, 8));
  BOOST_TEST(testFFN(S("espogqbthk"), "dpteiajrqmsognhlfbkc", 5, 20, S::npos));
  BOOST_TEST(testFFN(S("shoiedtcjb"), "", 9, 0, 9));
  BOOST_TEST(testFFN(S("ebcinjgads"), "tqbnh", 9, 0, 9));
  BOOST_TEST(testFFN(S("dqmregkcfl"), "akmle", 9, 1, 9));
  BOOST_TEST(testFFN(S("ngcrieqajf"), "iqfkm", 9, 2, 9));
  BOOST_TEST(testFFN(S("qosmilgnjb"), "tqjsr", 9, 4, 9));
  BOOST_TEST(testFFN(S("ikabsjtdfl"), "jplqg", 9, 5, S::npos));
  BOOST_TEST(testFFN(S("ersmicafdh"), "oilnrbcgtj", 9, 0, 9));
  BOOST_TEST(testFFN(S("fdnplotmgh"), "morkglpesn", 9, 1, 9));
  BOOST_TEST(testFFN(S("fdbicojerm"), "dmicerngat", 9, 5, S::npos));
  BOOST_TEST(testFFN(S("mbtafndjcq"), "radgeskbtc", 9, 9, 9));
  BOOST_TEST(testFFN(S("mlenkpfdtc"), "ljikprsmqo", 9, 10, 9));
  BOOST_TEST(testFFN(S("ahlcifdqgs"), "trqihkcgsjamfdbolnpe", 9, 0, 9));
  BOOST_TEST(testFFN(S("bgjemaltks"), "lqmthbsrekajgnofcipd", 9, 1, 9));
  BOOST_TEST(testFFN(S("pdhslbqrfc"), "jtalmedribkgqsopcnfh", 9, 10, 9));
  BOOST_TEST(testFFN(S("dirhtsnjkc"), "spqfoiclmtagejbndkrh", 9, 19, S::npos));
  BOOST_TEST(testFFN(S("dlroktbcja"), "nmotklspigjrdhcfaebq", 9, 20, S::npos));
  BOOST_TEST(testFFN(S("ncjpmaekbs"), "", 10, 0, S::npos));
  BOOST_TEST(testFFN(S("hlbosgmrak"), "hpmsd", 10, 0, S::npos));
  BOOST_TEST(testFFN(S("pqfhsgilen"), "qnpor", 10, 1, S::npos));
  BOOST_TEST(testFFN(S("gqtjsbdckh"), "otdma", 10, 2, S::npos));
  BOOST_TEST(testFFN(S("cfkqpjlegi"), "efhjg", 10, 4, S::npos));
  BOOST_TEST(testFFN(S("beanrfodgj"), "odpte", 10, 5, S::npos));
  BOOST_TEST(testFFN(S("adtkqpbjfi"), "bctdgfmolr", 10, 0, S::npos));
  BOOST_TEST(testFFN(S("iomkfthagj"), "oaklidrbqg", 10, 1, S::npos));
  BOOST_TEST(testFFN(S("sdpcilonqj"), "dnjfsagktr", 10, 5, S::npos));
  BOOST_TEST(testFFN(S("gtfbdkqeml"), "nejaktmiqg", 10, 9, S::npos));
  BOOST_TEST(testFFN(S("bmeqgcdorj"), "pjqonlebsf", 10, 10, S::npos));
  BOOST_TEST(testFFN(S("etqlcanmob"), "dshmnbtolcjepgaikfqr", 10, 0, S::npos));
  BOOST_TEST(testFFN(S("roqmkbdtia"), "iogfhpabtjkqlrnemcds", 10, 1, S::npos));
  BOOST_TEST(testFFN(S("kadsithljf"), "ngridfabjsecpqltkmoh", 10, 10, S::npos));
  BOOST_TEST(testFFN(S("sgtkpbfdmh"), "athmknplcgofrqejsdib", 10, 19, S::npos));
  BOOST_TEST(testFFN(S("qgmetnabkl"), "ldobhmqcafnjtkeisgrp", 10, 20, S::npos));
  BOOST_TEST(testFFN(S("cqjohampgd"), "", 11, 0, S::npos));
  BOOST_TEST(testFFN(S("hobitmpsan"), "aocjb", 11, 0, S::npos));
  BOOST_TEST(testFFN(S("tjehkpsalm"), "jbrnk", 11, 1, S::npos));
  BOOST_TEST(testFFN(S("ngfbojitcl"), "tqedg", 11, 2, S::npos));
  BOOST_TEST(testFFN(S("rcfkdbhgjo"), "nqskp", 11, 4, S::npos));
  BOOST_TEST(testFFN(S("qghptonrea"), "eaqkl", 11, 5, S::npos));
  BOOST_TEST(testFFN(S("hnprfgqjdl"), "reaoicljqm", 11, 0, S::npos));
  BOOST_TEST(testFFN(S("hlmgabenti"), "lsftgajqpm", 11, 1, S::npos));
  BOOST_TEST(testFFN(S("ofcjanmrbs"), "rlpfogmits", 11, 5, S::npos));
  BOOST_TEST(testFFN(S("jqedtkornm"), "shkncmiaqj", 11, 9, S::npos));
  BOOST_TEST(testFFN(S("rfedlasjmg"), "fpnatrhqgs", 11, 10, S::npos));
  BOOST_TEST(testFFN(S("talpqjsgkm"), "sjclemqhnpdbgikarfot", 11, 0, S::npos));
  BOOST_TEST(testFFN(S("lrkcbtqpie"), "otcmedjikgsfnqbrhpla", 11, 1, S::npos));
  BOOST_TEST(testFFN(S("cipogdskjf"), "bonsaefdqiprkhlgtjcm", 11, 10, S::npos));
  BOOST_TEST(testFFN(S("nqedcojahi"), "egpscmahijlfnkrodqtb", 11, 19, S::npos));
  BOOST_TEST(testFFN(S("hefnrkmctj"), "kmqbfepjthgilscrndoa", 11, 20, S::npos));
  BOOST_TEST(testFFN(S("atqirnmekfjolhpdsgcb"), "", 0, 0, 0));
  BOOST_TEST(testFFN(S("echfkmlpribjnqsaogtd"), "prboq", 0, 0, 0));
  BOOST_TEST(testFFN(S("qnhiftdgcleajbpkrosm"), "fjcqh", 0, 1, 0));
  BOOST_TEST(testFFN(S("chamfknorbedjitgslpq"), "fmosa", 0, 2, 0));
  BOOST_TEST(testFFN(S("njhqpibfmtlkaecdrgso"), "qdbok", 0, 4, 0));
  BOOST_TEST(testFFN(S("ebnghfsqkprmdcljoiat"), "amslg", 0, 5, 0));
  BOOST_TEST(testFFN(S("letjomsgihfrpqbkancd"), "smpltjneqb", 0, 0, 0));
  BOOST_TEST(testFFN(S("nblgoipcrqeaktshjdmf"), "flitskrnge", 0, 1, 0));
  BOOST_TEST(testFFN(S("cehkbngtjoiflqapsmrd"), "pgqihmlbef", 0, 5, 0));
  BOOST_TEST(testFFN(S("mignapfoklbhcqjetdrs"), "cfpdqjtgsb", 0, 9, 0));
  BOOST_TEST(testFFN(S("ceatbhlsqjgpnokfrmdi"), "htpsiaflom", 0, 10, 0));
  BOOST_TEST(testFFN(S("ocihkjgrdelpfnmastqb"), "kpjfiaceghsrdtlbnomq", 0, 0, 0));
  BOOST_TEST(testFFN(S("noelgschdtbrjfmiqkap"), "qhtbomidljgafneksprc", 0, 1, 0));
  BOOST_TEST(testFFN(S("dkclqfombepritjnghas"), "nhtjobkcefldimpsaqgr", 0, 10, 0));
  BOOST_TEST(testFFN(S("miklnresdgbhqcojftap"), "prabcjfqnoeskilmtgdh", 0, 19, 11));
  BOOST_TEST(testFFN(S("htbcigojaqmdkfrnlsep"), "dtrgmchilkasqoebfpjn", 0, 20, S::npos));
  BOOST_TEST(testFFN(S("febhmqtjanokscdirpgl"), "", 1, 0, 1));
  BOOST_TEST(testFFN(S("loakbsqjpcrdhftniegm"), "sqome", 1, 0, 1));
  BOOST_TEST(testFFN(S("reagphsqflbitdcjmkno"), "smfte", 1, 1, 1));
  BOOST_TEST(testFFN(S("jitlfrqemsdhkopncabg"), "ciboh", 1, 2, 2));
  BOOST_TEST(testFFN(S("mhtaepscdnrjqgbkifol"), "haois", 1, 4, 2));
  BOOST_TEST(testFFN(S("tocesrfmnglpbjihqadk"), "abfki", 1, 5, 1));
  BOOST_TEST(testFFN(S("lpfmctjrhdagneskbqoi"), "frdkocntmq", 1, 0, 1));
  BOOST_TEST(testFFN(S("lsmqaepkdhncirbtjfgo"), "oasbpedlnr", 1, 1, 1));
  BOOST_TEST(testFFN(S("epoiqmtldrabnkjhcfsg"), "kltqmhgand", 1, 5, 1));
  BOOST_TEST(testFFN(S("emgasrilpknqojhtbdcf"), "gdtfjchpmr", 1, 9, 3));
  BOOST_TEST(testFFN(S("hnfiagdpcklrjetqbsom"), "ponmcqblet", 1, 10, 2));
  BOOST_TEST(testFFN(S("nsdfebgajhmtricpoklq"), "sgphqdnofeiklatbcmjr", 1, 0, 1));
  BOOST_TEST(testFFN(S("atjgfsdlpobmeiqhncrk"), "ljqprsmigtfoneadckbh", 1, 1, 1));
  BOOST_TEST(testFFN(S("sitodfgnrejlahcbmqkp"), "ligeojhafnkmrcsqtbdp", 1, 10, 2));
  BOOST_TEST(testFFN(S("fraghmbiceknltjpqosd"), "lsimqfnjarbopedkhcgt", 1, 19, 13));
  BOOST_TEST(testFFN(S("pmafenlhqtdbkirjsogc"), "abedmfjlghniorcqptks", 1, 20, S::npos));
  BOOST_TEST(testFFN(S("pihgmoeqtnakrjslcbfd"), "", 10, 0, 10));
  BOOST_TEST(testFFN(S("gjdkeprctqblnhiafsom"), "hqtoa", 10, 0, 10));
  BOOST_TEST(testFFN(S("mkpnblfdsahrcqijteog"), "cahif", 10, 1, 10));
  BOOST_TEST(testFFN(S("gckarqnelodfjhmbptis"), "kehis", 10, 2, 10));
  BOOST_TEST(testFFN(S("gqpskidtbclomahnrjfe"), "kdlmh", 10, 4, 11));
  BOOST_TEST(testFFN(S("pkldjsqrfgitbhmaecno"), "paeql", 10, 5, 10));
  BOOST_TEST(testFFN(S("aftsijrbeklnmcdqhgop"), "aghoqiefnb", 10, 0, 10));
  BOOST_TEST(testFFN(S("mtlgdrhafjkbiepqnsoc"), "jrbqaikpdo", 10, 1, 10));
  BOOST_TEST(testFFN(S("pqgirnaefthokdmbsclj"), "smjonaeqcl", 10, 5, 10));
  BOOST_TEST(testFFN(S("kpdbgjmtherlsfcqoina"), "eqbdrkcfah", 10, 9, 11));
  BOOST_TEST(testFFN(S("jrlbothiknqmdgcfasep"), "kapmsienhf", 10, 10, 10));
  BOOST_TEST(testFFN(S("mjogldqferckabinptsh"), "jpqotrlenfcsbhkaimdg", 10, 0, 10));
  BOOST_TEST(testFFN(S("apoklnefbhmgqcdrisjt"), "jlbmhnfgtcqprikeados", 10, 1, 10));
  BOOST_TEST(testFFN(S("ifeopcnrjbhkdgatmqls"), "stgbhfmdaljnpqoicker", 10, 10, 11));
  BOOST_TEST(testFFN(S("ckqhaiesmjdnrgolbtpf"), "oihcetflbjagdsrkmqpn", 10, 19, 11));
  BOOST_TEST(testFFN(S("bnlgapfimcoterskqdjh"), "adtclebmnpjsrqfkigoh", 10, 20, S::npos));
  BOOST_TEST(testFFN(S("kgdlrobpmjcthqsafeni"), "", 19, 0, 19));
  BOOST_TEST(testFFN(S("dfkechomjapgnslbtqir"), "beafg", 19, 0, 19));
  BOOST_TEST(testFFN(S("rloadknfbqtgmhcsipje"), "iclat", 19, 1, 19));
  BOOST_TEST(testFFN(S("mgjhkolrnadqbpetcifs"), "rkhnf", 19, 2, 19));
  BOOST_TEST(testFFN(S("cmlfakiojdrgtbsphqen"), "clshq", 19, 4, 19));
  BOOST_TEST(testFFN(S("kghbfipeomsntdalrqjc"), "dtcoj", 19, 5, S::npos));
  BOOST_TEST(testFFN(S("eldiqckrnmtasbghjfpo"), "rqosnjmfth", 19, 0, 19));
  BOOST_TEST(testFFN(S("abqjcfedgotihlnspkrm"), "siatdfqglh", 19, 1, 19));
  BOOST_TEST(testFFN(S("qfbadrtjsimkolcenhpg"), "mrlshtpgjq", 19, 5, 19));
  BOOST_TEST(testFFN(S("abseghclkjqifmtodrnp"), "adlcskgqjt", 19, 9, 19));
  BOOST_TEST(testFFN(S("ibmsnlrjefhtdokacqpg"), "drshcjknaf", 19, 10, 19));
  BOOST_TEST(testFFN(S("mrkfciqjebaponsthldg"), "etsaqroinghpkjdlfcbm", 19, 0, 19));
  BOOST_TEST(testFFN(S("mjkticdeoqshpalrfbgn"), "sgepdnkqliambtrocfhj", 19, 1, 19));
  BOOST_TEST(testFFN(S("rqnoclbdejgiphtfsakm"), "nlmcjaqgbsortfdihkpe", 19, 10, S::npos));
  BOOST_TEST(testFFN(S("plkqbhmtfaeodjcrsing"), "racfnpmosldibqkghjet", 19, 19, S::npos));
  BOOST_TEST(testFFN(S("oegalhmstjrfickpbndq"), "fjhdsctkqeiolagrnmbp", 19, 20, S::npos));
  BOOST_TEST(testFFN(S("rdtgjcaohpblniekmsfq"), "", 20, 0, S::npos));
  BOOST_TEST(testFFN(S("ofkqbnjetrmsaidphglc"), "ejanp", 20, 0, S::npos));
  BOOST_TEST(testFFN(S("grkpahljcftesdmonqib"), "odife", 20, 1, S::npos));
  BOOST_TEST(testFFN(S("jimlgbhfqkteospardcn"), "okaqd", 20, 2, S::npos));
  BOOST_TEST(testFFN(S("gftenihpmslrjkqadcob"), "lcdbi", 20, 4, S::npos));
  BOOST_TEST(testFFN(S("bmhldogtckrfsanijepq"), "fsqbj", 20, 5, S::npos));
  BOOST_TEST(testFFN(S("nfqkrpjdesabgtlcmoih"), "bigdomnplq", 20, 0, S::npos));
  BOOST_TEST(testFFN(S("focalnrpiqmdkstehbjg"), "apiblotgcd", 20, 1, S::npos));
  BOOST_TEST(testFFN(S("rhqdspkmebiflcotnjga"), "acfhdenops", 20, 5, S::npos));
  BOOST_TEST(testFFN(S("rahdtmsckfboqlpniegj"), "jopdeamcrk", 20, 9, S::npos));
  BOOST_TEST(testFFN(S("fbkeiopclstmdqranjhg"), "trqncbkgmh", 20, 10, S::npos));
  BOOST_TEST(testFFN(S("lifhpdgmbconstjeqark"), "tomglrkencbsfjqpihda", 20, 0, S::npos));
  BOOST_TEST(testFFN(S("pboqganrhedjmltsicfk"), "gbkhdnpoietfcmrslajq", 20, 1, S::npos));
  BOOST_TEST(testFFN(S("klchabsimetjnqgorfpd"), "rtfnmbsglkjaichoqedp", 20, 10, S::npos));
  BOOST_TEST(testFFN(S("sirfgmjqhctndbklaepo"), "ohkmdpfqbsacrtjnlgei", 20, 19, S::npos));
  BOOST_TEST(testFFN(S("rlbdsiceaonqjtfpghkm"), "dlbrteoisgphmkncajfq", 20, 20, S::npos));
  BOOST_TEST(testFFN(S("ecgdanriptblhjfqskom"), "", 21, 0, S::npos));
  BOOST_TEST(testFFN(S("fdmiarlpgcskbhoteqjn"), "sjrlo", 21, 0, S::npos));
  BOOST_TEST(testFFN(S("rlbstjqopignecmfadkh"), "qjpor", 21, 1, S::npos));
  BOOST_TEST(testFFN(S("grjpqmbshektdolcafni"), "odhfn", 21, 2, S::npos));
  BOOST_TEST(testFFN(S("sakfcohtqnibprjmlged"), "qtfin", 21, 4, S::npos));
  BOOST_TEST(testFFN(S("mjtdglasihqpocebrfkn"), "hpqfo", 21, 5, S::npos));
  BOOST_TEST(testFFN(S("okaplfrntghqbmeicsdj"), "fabmertkos", 21, 0, S::npos));
  BOOST_TEST(testFFN(S("sahngemrtcjidqbklfpo"), "brqtgkmaej", 21, 1, S::npos));
  BOOST_TEST(testFFN(S("dlmsipcnekhbgoaftqjr"), "nfrdeihsgl", 21, 5, S::npos));
  BOOST_TEST(testFFN(S("ahegrmqnoiklpfsdbcjt"), "hlfrosekpi", 21, 9, S::npos));
  BOOST_TEST(testFFN(S("hdsjbnmlegtkqripacof"), "atgbkrjdsm", 21, 10, S::npos));
  BOOST_TEST(testFFN(S("pcnedrfjihqbalkgtoms"), "blnrptjgqmaifsdkhoec", 21, 0, S::npos));
  BOOST_TEST(testFFN(S("qjidealmtpskrbfhocng"), "ctpmdahebfqjgknloris", 21, 1, S::npos));
  BOOST_TEST(testFFN(S("qeindtagmokpfhsclrbj"), "apnkeqthrmlbfodiscgj", 21, 10, S::npos));
  BOOST_TEST(testFFN(S("kpfegbjhsrnodltqciam"), "jdgictpframeoqlsbknh", 21, 19, S::npos));
  BOOST_TEST(testFFN(S("hnbrcplsjfgiktoedmaq"), "qprlsfojamgndekthibc", 21, 20, S::npos));

  // find_last_not_of

  BOOST_TEST(fs1.find_last_not_of(v3) == 2);
  BOOST_TEST(fs1.find_last_not_of(v4) == 3);
  BOOST_TEST(fs1.find_last_not_of(fs3) == 2);
  BOOST_TEST(fs1.find_last_not_of(fs4) == 3);

  BOOST_TEST(fs1.find_last_not_of(cs3) == 2);
  BOOST_TEST(fs1.find_last_not_of(cs4) == 3);

  BOOST_TEST(fs1.find_last_not_of(cs3, 0) == S::npos);
  BOOST_TEST(fs1.find_last_not_of(cs4, 0) == 0);

  BOOST_TEST(fs1.find_last_not_of(cs4, 0, 2) == 0);

  BOOST_TEST(fs1.find_last_not_of(cs3, 4) == 2);
  BOOST_TEST(fs1.find_last_not_of(cs4, 4) == 3);

  BOOST_TEST(fs1.find_last_not_of('1') == 4);
  BOOST_TEST(fs1.find_last_not_of('1', 3) == 3);


  BOOST_TEST(testFLN(S(""), "", 0, 0, S::npos));
  BOOST_TEST(testFLN(S(""), "irkhs", 0, 0, S::npos));
  BOOST_TEST(testFLN(S(""), "kante", 0, 1, S::npos));
  BOOST_TEST(testFLN(S(""), "oknlr", 0, 2, S::npos));
  BOOST_TEST(testFLN(S(""), "pcdro", 0, 4, S::npos));
  BOOST_TEST(testFLN(S(""), "bnrpe", 0, 5, S::npos));
  BOOST_TEST(testFLN(S(""), "jtdaefblso", 0, 0, S::npos));
  BOOST_TEST(testFLN(S(""), "oselktgbca", 0, 1, S::npos));
  BOOST_TEST(testFLN(S(""), "eqgaplhckj", 0, 5, S::npos));
  BOOST_TEST(testFLN(S(""), "bjahtcmnlp", 0, 9, S::npos));
  BOOST_TEST(testFLN(S(""), "hjlcmgpket", 0, 10, S::npos));
  BOOST_TEST(testFLN(S(""), "htaobedqikfplcgjsmrn", 0, 0, S::npos));
  BOOST_TEST(testFLN(S(""), "hpqiarojkcdlsgnmfetb", 0, 1, S::npos));
  BOOST_TEST(testFLN(S(""), "dfkaprhjloqetcsimnbg", 0, 10, S::npos));
  BOOST_TEST(testFLN(S(""), "ihqrfebgadntlpmjksoc", 0, 19, S::npos));
  BOOST_TEST(testFLN(S(""), "ngtjfcalbseiqrphmkdo", 0, 20, S::npos));
  BOOST_TEST(testFLN(S(""), "", 1, 0, S::npos));
  BOOST_TEST(testFLN(S(""), "lbtqd", 1, 0, S::npos));
  BOOST_TEST(testFLN(S(""), "tboim", 1, 1, S::npos));
  BOOST_TEST(testFLN(S(""), "slcer", 1, 2, S::npos));
  BOOST_TEST(testFLN(S(""), "cbjfs", 1, 4, S::npos));
  BOOST_TEST(testFLN(S(""), "aqibs", 1, 5, S::npos));
  BOOST_TEST(testFLN(S(""), "gtfblmqinc", 1, 0, S::npos));
  BOOST_TEST(testFLN(S(""), "mkqpbtdalg", 1, 1, S::npos));
  BOOST_TEST(testFLN(S(""), "kphatlimcd", 1, 5, S::npos));
  BOOST_TEST(testFLN(S(""), "pblasqogic", 1, 9, S::npos));
  BOOST_TEST(testFLN(S(""), "arosdhcfme", 1, 10, S::npos));
  BOOST_TEST(testFLN(S(""), "blkhjeogicatqfnpdmsr", 1, 0, S::npos));
  BOOST_TEST(testFLN(S(""), "bmhineprjcoadgstflqk", 1, 1, S::npos));
  BOOST_TEST(testFLN(S(""), "djkqcmetslnghpbarfoi", 1, 10, S::npos));
  BOOST_TEST(testFLN(S(""), "lgokshjtpbemarcdqnfi", 1, 19, S::npos));
  BOOST_TEST(testFLN(S(""), "bqjhtkfepimcnsgrlado", 1, 20, S::npos));
  BOOST_TEST(testFLN(S("eaint"), "", 0, 0, 0));
  BOOST_TEST(testFLN(S("binja"), "gfsrt", 0, 0, 0));
  BOOST_TEST(testFLN(S("latkm"), "pfsoc", 0, 1, 0));
  BOOST_TEST(testFLN(S("lecfr"), "tpflm", 0, 2, 0));
  BOOST_TEST(testFLN(S("eqkst"), "sgkec", 0, 4, S::npos));
  BOOST_TEST(testFLN(S("cdafr"), "romds", 0, 5, 0));
  BOOST_TEST(testFLN(S("prbhe"), "qhjistlgmr", 0, 0, 0));
  BOOST_TEST(testFLN(S("lbisk"), "pedfirsglo", 0, 1, 0));
  BOOST_TEST(testFLN(S("hrlpd"), "aqcoslgrmk", 0, 5, 0));
  BOOST_TEST(testFLN(S("ehmja"), "dabckmepqj", 0, 9, S::npos));
  BOOST_TEST(testFLN(S("mhqgd"), "pqscrjthli", 0, 10, 0));
  BOOST_TEST(testFLN(S("tgklq"), "kfphdcsjqmobliagtren", 0, 0, 0));
  BOOST_TEST(testFLN(S("bocjs"), "rokpefncljibsdhqtagm", 0, 1, 0));
  BOOST_TEST(testFLN(S("grbsd"), "afionmkphlebtcjqsgrd", 0, 10, 0));
  BOOST_TEST(testFLN(S("ofjqr"), "aenmqplidhkofrjbctsg", 0, 19, S::npos));
  BOOST_TEST(testFLN(S("btlfi"), "osjmbtcadhiklegrpqnf", 0, 20, S::npos));
  BOOST_TEST(testFLN(S("clrgb"), "", 1, 0, 1));
  BOOST_TEST(testFLN(S("tjmek"), "osmia", 1, 0, 1));
  BOOST_TEST(testFLN(S("bgstp"), "ckonl", 1, 1, 1));
  BOOST_TEST(testFLN(S("hstrk"), "ilcaj", 1, 2, 1));
  BOOST_TEST(testFLN(S("kmspj"), "lasiq", 1, 4, 1));
  BOOST_TEST(testFLN(S("tjboh"), "kfqmr", 1, 5, 1));
  BOOST_TEST(testFLN(S("ilbcj"), "klnitfaobg", 1, 0, 1));
  BOOST_TEST(testFLN(S("jkngf"), "gjhmdlqikp", 1, 1, 1));
  BOOST_TEST(testFLN(S("gfcql"), "skbgtahqej", 1, 5, 1));
  BOOST_TEST(testFLN(S("dqtlg"), "bjsdgtlpkf", 1, 9, 1));
  BOOST_TEST(testFLN(S("bthpg"), "bjgfmnlkio", 1, 10, 1));
  BOOST_TEST(testFLN(S("dgsnq"), "lbhepotfsjdqigcnamkr", 1, 0, 1));
  BOOST_TEST(testFLN(S("rmfhp"), "tebangckmpsrqdlfojhi", 1, 1, 1));
  BOOST_TEST(testFLN(S("jfdam"), "joflqbdkhtegimscpanr", 1, 10, S::npos));
  BOOST_TEST(testFLN(S("edapb"), "adpmcohetfbsrjinlqkg", 1, 19, S::npos));
  BOOST_TEST(testFLN(S("brfsm"), "iacldqjpfnogbsrhmetk", 1, 20, S::npos));
  BOOST_TEST(testFLN(S("ndrhl"), "", 2, 0, 2));
  BOOST_TEST(testFLN(S("mrecp"), "otkgb", 2, 0, 2));
  BOOST_TEST(testFLN(S("qlasf"), "cqsjl", 2, 1, 2));
  BOOST_TEST(testFLN(S("smaqd"), "dpifl", 2, 2, 2));
  BOOST_TEST(testFLN(S("hjeni"), "oapht", 2, 4, 2));
  BOOST_TEST(testFLN(S("ocmfj"), "cifts", 2, 5, 2));
  BOOST_TEST(testFLN(S("hmftq"), "nmsckbgalo", 2, 0, 2));
  BOOST_TEST(testFLN(S("fklad"), "tpksqhamle", 2, 1, 2));
  BOOST_TEST(testFLN(S("dirnm"), "tpdrchmkji", 2, 5, 1));
  BOOST_TEST(testFLN(S("hrgdc"), "ijagfkblst", 2, 9, 1));
  BOOST_TEST(testFLN(S("ifakg"), "kpocsignjb", 2, 10, 2));
  BOOST_TEST(testFLN(S("ebrgd"), "pecqtkjsnbdrialgmohf", 2, 0, 2));
  BOOST_TEST(testFLN(S("rcjml"), "aiortphfcmkjebgsndql", 2, 1, 2));
  BOOST_TEST(testFLN(S("peqmt"), "sdbkeamglhipojqftrcn", 2, 10, 2));
  BOOST_TEST(testFLN(S("frehn"), "ljqncehgmfktroapidbs", 2, 19, S::npos));
  BOOST_TEST(testFLN(S("tqolf"), "rtcfodilamkbenjghqps", 2, 20, S::npos));
  BOOST_TEST(testFLN(S("cjgao"), "", 4, 0, 4));
  BOOST_TEST(testFLN(S("kjplq"), "mabns", 4, 0, 4));
  BOOST_TEST(testFLN(S("herni"), "bdnrp", 4, 1, 4));
  BOOST_TEST(testFLN(S("tadrb"), "scidp", 4, 2, 4));
  BOOST_TEST(testFLN(S("pkfeo"), "agbjl", 4, 4, 4));
  BOOST_TEST(testFLN(S("hoser"), "jfmpr", 4, 5, 3));
  BOOST_TEST(testFLN(S("kgrsp"), "rbpefghsmj", 4, 0, 4));
  BOOST_TEST(testFLN(S("pgejb"), "apsfntdoqc", 4, 1, 4));
  BOOST_TEST(testFLN(S("thlnq"), "ndkjeisgcl", 4, 5, 4));
  BOOST_TEST(testFLN(S("nbmit"), "rnfpqatdeo", 4, 9, 3));
  BOOST_TEST(testFLN(S("jgmib"), "bntjlqrfik", 4, 10, 2));
  BOOST_TEST(testFLN(S("ncrfj"), "kcrtmpolnaqejghsfdbi", 4, 0, 4));
  BOOST_TEST(testFLN(S("ncsik"), "lobheanpkmqidsrtcfgj", 4, 1, 4));
  BOOST_TEST(testFLN(S("sgbfh"), "athdkljcnreqbgpmisof", 4, 10, 3));
  BOOST_TEST(testFLN(S("dktbn"), "qkdmjialrscpbhefgont", 4, 19, 2));
  BOOST_TEST(testFLN(S("fthqm"), "dmasojntqleribkgfchp", 4, 20, S::npos));
  BOOST_TEST(testFLN(S("klopi"), "", 5, 0, 4));
  BOOST_TEST(testFLN(S("dajhn"), "psthd", 5, 0, 4));
  BOOST_TEST(testFLN(S("jbgno"), "rpmjd", 5, 1, 4));
  BOOST_TEST(testFLN(S("hkjae"), "dfsmk", 5, 2, 4));
  BOOST_TEST(testFLN(S("gbhqo"), "skqne", 5, 4, 4));
  BOOST_TEST(testFLN(S("ktdor"), "kipnf", 5, 5, 4));
  BOOST_TEST(testFLN(S("ldprn"), "hmrnqdgifl", 5, 0, 4));
  BOOST_TEST(testFLN(S("egmjk"), "fsmjcdairn", 5, 1, 4));
  BOOST_TEST(testFLN(S("armql"), "pcdgltbrfj", 5, 5, 3));
  BOOST_TEST(testFLN(S("cdhjo"), "aekfctpirg", 5, 9, 4));
  BOOST_TEST(testFLN(S("jcons"), "ledihrsgpf", 5, 10, 3));
  BOOST_TEST(testFLN(S("cbrkp"), "mqcklahsbtirgopefndj", 5, 0, 4));
  BOOST_TEST(testFLN(S("fhgna"), "kmlthaoqgecrnpdbjfis", 5, 1, 4));
  BOOST_TEST(testFLN(S("ejfcd"), "sfhbamcdptojlkrenqgi", 5, 10, 1));
  BOOST_TEST(testFLN(S("kqjhe"), "pbniofmcedrkhlstgaqj", 5, 19, 2));
  BOOST_TEST(testFLN(S("pbdjl"), "mongjratcskbhqiepfdl", 5, 20, S::npos));
  BOOST_TEST(testFLN(S("gajqn"), "", 6, 0, 4));
  BOOST_TEST(testFLN(S("stedk"), "hrnat", 6, 0, 4));
  BOOST_TEST(testFLN(S("tjkaf"), "gsqdt", 6, 1, 4));
  BOOST_TEST(testFLN(S("dthpe"), "bspkd", 6, 2, 4));
  BOOST_TEST(testFLN(S("klhde"), "ohcmb", 6, 4, 4));
  BOOST_TEST(testFLN(S("bhlki"), "heatr", 6, 5, 4));
  BOOST_TEST(testFLN(S("lqmoh"), "pmblckedfn", 6, 0, 4));
  BOOST_TEST(testFLN(S("mtqin"), "aceqmsrbik", 6, 1, 4));
  BOOST_TEST(testFLN(S("dpqbr"), "lmbtdehjrn", 6, 5, 4));
  BOOST_TEST(testFLN(S("kdhmo"), "teqmcrlgib", 6, 9, 4));
  BOOST_TEST(testFLN(S("jblqp"), "njolbmspac", 6, 10, 3));
  BOOST_TEST(testFLN(S("qmjgl"), "pofnhidklamecrbqjgst", 6, 0, 4));
  BOOST_TEST(testFLN(S("rothp"), "jbhckmtgrqnosafedpli", 6, 1, 4));
  BOOST_TEST(testFLN(S("ghknq"), "dobntpmqklicsahgjerf", 6, 10, 1));
  BOOST_TEST(testFLN(S("eopfi"), "tpdshainjkbfoemlrgcq", 6, 19, S::npos));
  BOOST_TEST(testFLN(S("dsnmg"), "oldpfgeakrnitscbjmqh", 6, 20, S::npos));
  BOOST_TEST(testFLN(S("jnkrfhotgl"), "", 0, 0, 0));
  BOOST_TEST(testFLN(S("dltjfngbko"), "rqegt", 0, 0, 0));
  BOOST_TEST(testFLN(S("bmjlpkiqde"), "dashm", 0, 1, 0));
  BOOST_TEST(testFLN(S("skrflobnqm"), "jqirk", 0, 2, 0));
  BOOST_TEST(testFLN(S("jkpldtshrm"), "rckeg", 0, 4, 0));
  BOOST_TEST(testFLN(S("ghasdbnjqo"), "jscie", 0, 5, 0));
  BOOST_TEST(testFLN(S("igrkhpbqjt"), "efsphndliq", 0, 0, 0));
  BOOST_TEST(testFLN(S("ikthdgcamf"), "gdicosleja", 0, 1, 0));
  BOOST_TEST(testFLN(S("pcofgeniam"), "qcpjibosfl", 0, 5, S::npos));
  BOOST_TEST(testFLN(S("rlfjgesqhc"), "lrhmefnjcq", 0, 9, S::npos));
  BOOST_TEST(testFLN(S("itphbqsker"), "dtablcrseo", 0, 10, 0));
  BOOST_TEST(testFLN(S("skjafcirqm"), "apckjsftedbhgomrnilq", 0, 0, 0));
  BOOST_TEST(testFLN(S("tcqomarsfd"), "pcbrgflehjtiadnsokqm", 0, 1, 0));
  BOOST_TEST(testFLN(S("rocfeldqpk"), "nsiadegjklhobrmtqcpf", 0, 10, 0));
  BOOST_TEST(testFLN(S("cfpegndlkt"), "cpmajdqnolikhgsbretf", 0, 19, S::npos));
  BOOST_TEST(testFLN(S("fqbtnkeasj"), "jcflkntmgiqrphdosaeb", 0, 20, S::npos));
  BOOST_TEST(testFLN(S("shbcqnmoar"), "", 1, 0, 1));
  BOOST_TEST(testFLN(S("bdoshlmfin"), "ontrs", 1, 0, 1));
  BOOST_TEST(testFLN(S("khfrebnsgq"), "pfkna", 1, 1, 1));
  BOOST_TEST(testFLN(S("getcrsaoji"), "ekosa", 1, 2, 0));
  BOOST_TEST(testFLN(S("fjiknedcpq"), "anqhk", 1, 4, 1));
  BOOST_TEST(testFLN(S("tkejgnafrm"), "jekca", 1, 5, 0));
  BOOST_TEST(testFLN(S("jnakolqrde"), "ikemsjgacf", 1, 0, 1));
  BOOST_TEST(testFLN(S("lcjptsmgbe"), "arolgsjkhm", 1, 1, 1));
  BOOST_TEST(testFLN(S("itfsmcjorl"), "oftkbldhre", 1, 5, 0));
  BOOST_TEST(testFLN(S("omchkfrjea"), "gbkqdoeftl", 1, 9, 1));
  BOOST_TEST(testFLN(S("cigfqkated"), "sqcflrgtim", 1, 10, S::npos));
  BOOST_TEST(testFLN(S("tscenjikml"), "fmhbkislrjdpanogqcet", 1, 0, 1));
  BOOST_TEST(testFLN(S("qcpaemsinf"), "rnioadktqlgpbcjsmhef", 1, 1, 1));
  BOOST_TEST(testFLN(S("gltkojeipd"), "oakgtnldpsefihqmjcbr", 1, 10, S::npos));
  BOOST_TEST(testFLN(S("qistfrgnmp"), "gbnaelosidmcjqktfhpr", 1, 19, S::npos));
  BOOST_TEST(testFLN(S("bdnpfcqaem"), "akbripjhlosndcmqgfet", 1, 20, S::npos));
  BOOST_TEST(testFLN(S("ectnhskflp"), "", 5, 0, 5));
  BOOST_TEST(testFLN(S("fgtianblpq"), "pijag", 5, 0, 5));
  BOOST_TEST(testFLN(S("mfeqklirnh"), "jrckd", 5, 1, 5));
  BOOST_TEST(testFLN(S("astedncjhk"), "qcloh", 5, 2, 5));
  BOOST_TEST(testFLN(S("fhlqgcajbr"), "thlmp", 5, 4, 5));
  BOOST_TEST(testFLN(S("epfhocmdng"), "qidmo", 5, 5, 5));
  BOOST_TEST(testFLN(S("apcnsibger"), "lnegpsjqrd", 5, 0, 5));
  BOOST_TEST(testFLN(S("aqkocrbign"), "rjqdablmfs", 5, 1, 4));
  BOOST_TEST(testFLN(S("ijsmdtqgce"), "enkgpbsjaq", 5, 5, 5));
  BOOST_TEST(testFLN(S("clobgsrken"), "kdsgoaijfh", 5, 9, 3));
  BOOST_TEST(testFLN(S("jbhcfposld"), "trfqgmckbe", 5, 10, 5));
  BOOST_TEST(testFLN(S("oqnpblhide"), "igetsracjfkdnpoblhqm", 5, 0, 5));
  BOOST_TEST(testFLN(S("lroeasctif"), "nqctfaogirshlekbdjpm", 5, 1, 5));
  BOOST_TEST(testFLN(S("bpjlgmiedh"), "csehfgomljdqinbartkp", 5, 10, 1));
  BOOST_TEST(testFLN(S("pamkeoidrj"), "qahoegcmplkfsjbdnitr", 5, 19, S::npos));
  BOOST_TEST(testFLN(S("espogqbthk"), "dpteiajrqmsognhlfbkc", 5, 20, S::npos));
  BOOST_TEST(testFLN(S("shoiedtcjb"), "", 9, 0, 9));
  BOOST_TEST(testFLN(S("ebcinjgads"), "tqbnh", 9, 0, 9));
  BOOST_TEST(testFLN(S("dqmregkcfl"), "akmle", 9, 1, 9));
  BOOST_TEST(testFLN(S("ngcrieqajf"), "iqfkm", 9, 2, 9));
  BOOST_TEST(testFLN(S("qosmilgnjb"), "tqjsr", 9, 4, 9));
  BOOST_TEST(testFLN(S("ikabsjtdfl"), "jplqg", 9, 5, 8));
  BOOST_TEST(testFLN(S("ersmicafdh"), "oilnrbcgtj", 9, 0, 9));
  BOOST_TEST(testFLN(S("fdnplotmgh"), "morkglpesn", 9, 1, 9));
  BOOST_TEST(testFLN(S("fdbicojerm"), "dmicerngat", 9, 5, 8));
  BOOST_TEST(testFLN(S("mbtafndjcq"), "radgeskbtc", 9, 9, 9));
  BOOST_TEST(testFLN(S("mlenkpfdtc"), "ljikprsmqo", 9, 10, 9));
  BOOST_TEST(testFLN(S("ahlcifdqgs"), "trqihkcgsjamfdbolnpe", 9, 0, 9));
  BOOST_TEST(testFLN(S("bgjemaltks"), "lqmthbsrekajgnofcipd", 9, 1, 9));
  BOOST_TEST(testFLN(S("pdhslbqrfc"), "jtalmedribkgqsopcnfh", 9, 10, 9));
  BOOST_TEST(testFLN(S("dirhtsnjkc"), "spqfoiclmtagejbndkrh", 9, 19, 3));
  BOOST_TEST(testFLN(S("dlroktbcja"), "nmotklspigjrdhcfaebq", 9, 20, S::npos));
  BOOST_TEST(testFLN(S("ncjpmaekbs"), "", 10, 0, 9));
  BOOST_TEST(testFLN(S("hlbosgmrak"), "hpmsd", 10, 0, 9));
  BOOST_TEST(testFLN(S("pqfhsgilen"), "qnpor", 10, 1, 9));
  BOOST_TEST(testFLN(S("gqtjsbdckh"), "otdma", 10, 2, 9));
  BOOST_TEST(testFLN(S("cfkqpjlegi"), "efhjg", 10, 4, 9));
  BOOST_TEST(testFLN(S("beanrfodgj"), "odpte", 10, 5, 9));
  BOOST_TEST(testFLN(S("adtkqpbjfi"), "bctdgfmolr", 10, 0, 9));
  BOOST_TEST(testFLN(S("iomkfthagj"), "oaklidrbqg", 10, 1, 9));
  BOOST_TEST(testFLN(S("sdpcilonqj"), "dnjfsagktr", 10, 5, 8));
  BOOST_TEST(testFLN(S("gtfbdkqeml"), "nejaktmiqg", 10, 9, 9));
  BOOST_TEST(testFLN(S("bmeqgcdorj"), "pjqonlebsf", 10, 10, 8));
  BOOST_TEST(testFLN(S("etqlcanmob"), "dshmnbtolcjepgaikfqr", 10, 0, 9));
  BOOST_TEST(testFLN(S("roqmkbdtia"), "iogfhpabtjkqlrnemcds", 10, 1, 9));
  BOOST_TEST(testFLN(S("kadsithljf"), "ngridfabjsecpqltkmoh", 10, 10, 7));
  BOOST_TEST(testFLN(S("sgtkpbfdmh"), "athmknplcgofrqejsdib", 10, 19, 5));
  BOOST_TEST(testFLN(S("qgmetnabkl"), "ldobhmqcafnjtkeisgrp", 10, 20, S::npos));
  BOOST_TEST(testFLN(S("cqjohampgd"), "", 11, 0, 9));
  BOOST_TEST(testFLN(S("hobitmpsan"), "aocjb", 11, 0, 9));
  BOOST_TEST(testFLN(S("tjehkpsalm"), "jbrnk", 11, 1, 9));
  BOOST_TEST(testFLN(S("ngfbojitcl"), "tqedg", 11, 2, 9));
  BOOST_TEST(testFLN(S("rcfkdbhgjo"), "nqskp", 11, 4, 9));
  BOOST_TEST(testFLN(S("qghptonrea"), "eaqkl", 11, 5, 7));
  BOOST_TEST(testFLN(S("hnprfgqjdl"), "reaoicljqm", 11, 0, 9));
  BOOST_TEST(testFLN(S("hlmgabenti"), "lsftgajqpm", 11, 1, 9));
  BOOST_TEST(testFLN(S("ofcjanmrbs"), "rlpfogmits", 11, 5, 9));
  BOOST_TEST(testFLN(S("jqedtkornm"), "shkncmiaqj", 11, 9, 7));
  BOOST_TEST(testFLN(S("rfedlasjmg"), "fpnatrhqgs", 11, 10, 8));
  BOOST_TEST(testFLN(S("talpqjsgkm"), "sjclemqhnpdbgikarfot", 11, 0, 9));
  BOOST_TEST(testFLN(S("lrkcbtqpie"), "otcmedjikgsfnqbrhpla", 11, 1, 9));
  BOOST_TEST(testFLN(S("cipogdskjf"), "bonsaefdqiprkhlgtjcm", 11, 10, 8));
  BOOST_TEST(testFLN(S("nqedcojahi"), "egpscmahijlfnkrodqtb", 11, 19, S::npos));
  BOOST_TEST(testFLN(S("hefnrkmctj"), "kmqbfepjthgilscrndoa", 11, 20, S::npos));
  BOOST_TEST(testFLN(S("atqirnmekfjolhpdsgcb"), "", 0, 0, 0));
  BOOST_TEST(testFLN(S("echfkmlpribjnqsaogtd"), "prboq", 0, 0, 0));
  BOOST_TEST(testFLN(S("qnhiftdgcleajbpkrosm"), "fjcqh", 0, 1, 0));
  BOOST_TEST(testFLN(S("chamfknorbedjitgslpq"), "fmosa", 0, 2, 0));
  BOOST_TEST(testFLN(S("njhqpibfmtlkaecdrgso"), "qdbok", 0, 4, 0));
  BOOST_TEST(testFLN(S("ebnghfsqkprmdcljoiat"), "amslg", 0, 5, 0));
  BOOST_TEST(testFLN(S("letjomsgihfrpqbkancd"), "smpltjneqb", 0, 0, 0));
  BOOST_TEST(testFLN(S("nblgoipcrqeaktshjdmf"), "flitskrnge", 0, 1, 0));
  BOOST_TEST(testFLN(S("cehkbngtjoiflqapsmrd"), "pgqihmlbef", 0, 5, 0));
  BOOST_TEST(testFLN(S("mignapfoklbhcqjetdrs"), "cfpdqjtgsb", 0, 9, 0));
  BOOST_TEST(testFLN(S("ceatbhlsqjgpnokfrmdi"), "htpsiaflom", 0, 10, 0));
  BOOST_TEST(testFLN(S("ocihkjgrdelpfnmastqb"), "kpjfiaceghsrdtlbnomq", 0, 0, 0));
  BOOST_TEST(testFLN(S("noelgschdtbrjfmiqkap"), "qhtbomidljgafneksprc", 0, 1, 0));
  BOOST_TEST(testFLN(S("dkclqfombepritjnghas"), "nhtjobkcefldimpsaqgr", 0, 10, 0));
  BOOST_TEST(testFLN(S("miklnresdgbhqcojftap"), "prabcjfqnoeskilmtgdh", 0, 19, S::npos));
  BOOST_TEST(testFLN(S("htbcigojaqmdkfrnlsep"), "dtrgmchilkasqoebfpjn", 0, 20, S::npos));
  BOOST_TEST(testFLN(S("febhmqtjanokscdirpgl"), "", 1, 0, 1));
  BOOST_TEST(testFLN(S("loakbsqjpcrdhftniegm"), "sqome", 1, 0, 1));
  BOOST_TEST(testFLN(S("reagphsqflbitdcjmkno"), "smfte", 1, 1, 1));
  BOOST_TEST(testFLN(S("jitlfrqemsdhkopncabg"), "ciboh", 1, 2, 0));
  BOOST_TEST(testFLN(S("mhtaepscdnrjqgbkifol"), "haois", 1, 4, 0));
  BOOST_TEST(testFLN(S("tocesrfmnglpbjihqadk"), "abfki", 1, 5, 1));
  BOOST_TEST(testFLN(S("lpfmctjrhdagneskbqoi"), "frdkocntmq", 1, 0, 1));
  BOOST_TEST(testFLN(S("lsmqaepkdhncirbtjfgo"), "oasbpedlnr", 1, 1, 1));
  BOOST_TEST(testFLN(S("epoiqmtldrabnkjhcfsg"), "kltqmhgand", 1, 5, 1));
  BOOST_TEST(testFLN(S("emgasrilpknqojhtbdcf"), "gdtfjchpmr", 1, 9, 0));
  BOOST_TEST(testFLN(S("hnfiagdpcklrjetqbsom"), "ponmcqblet", 1, 10, 0));
  BOOST_TEST(testFLN(S("nsdfebgajhmtricpoklq"), "sgphqdnofeiklatbcmjr", 1, 0, 1));
  BOOST_TEST(testFLN(S("atjgfsdlpobmeiqhncrk"), "ljqprsmigtfoneadckbh", 1, 1, 1));
  BOOST_TEST(testFLN(S("sitodfgnrejlahcbmqkp"), "ligeojhafnkmrcsqtbdp", 1, 10, 0));
  BOOST_TEST(testFLN(S("fraghmbiceknltjpqosd"), "lsimqfnjarbopedkhcgt", 1, 19, S::npos));
  BOOST_TEST(testFLN(S("pmafenlhqtdbkirjsogc"), "abedmfjlghniorcqptks", 1, 20, S::npos));
  BOOST_TEST(testFLN(S("pihgmoeqtnakrjslcbfd"), "", 10, 0, 10));
  BOOST_TEST(testFLN(S("gjdkeprctqblnhiafsom"), "hqtoa", 10, 0, 10));
  BOOST_TEST(testFLN(S("mkpnblfdsahrcqijteog"), "cahif", 10, 1, 10));
  BOOST_TEST(testFLN(S("gckarqnelodfjhmbptis"), "kehis", 10, 2, 10));
  BOOST_TEST(testFLN(S("gqpskidtbclomahnrjfe"), "kdlmh", 10, 4, 9));
  BOOST_TEST(testFLN(S("pkldjsqrfgitbhmaecno"), "paeql", 10, 5, 10));
  BOOST_TEST(testFLN(S("aftsijrbeklnmcdqhgop"), "aghoqiefnb", 10, 0, 10));
  BOOST_TEST(testFLN(S("mtlgdrhafjkbiepqnsoc"), "jrbqaikpdo", 10, 1, 10));
  BOOST_TEST(testFLN(S("pqgirnaefthokdmbsclj"), "smjonaeqcl", 10, 5, 10));
  BOOST_TEST(testFLN(S("kpdbgjmtherlsfcqoina"), "eqbdrkcfah", 10, 9, 8));
  BOOST_TEST(testFLN(S("jrlbothiknqmdgcfasep"), "kapmsienhf", 10, 10, 10));
  BOOST_TEST(testFLN(S("mjogldqferckabinptsh"), "jpqotrlenfcsbhkaimdg", 10, 0, 10));
  BOOST_TEST(testFLN(S("apoklnefbhmgqcdrisjt"), "jlbmhnfgtcqprikeados", 10, 1, 10));
  BOOST_TEST(testFLN(S("ifeopcnrjbhkdgatmqls"), "stgbhfmdaljnpqoicker", 10, 10, 8));
  BOOST_TEST(testFLN(S("ckqhaiesmjdnrgolbtpf"), "oihcetflbjagdsrkmqpn", 10, 19, S::npos));
  BOOST_TEST(testFLN(S("bnlgapfimcoterskqdjh"), "adtclebmnpjsrqfkigoh", 10, 20, S::npos));
  BOOST_TEST(testFLN(S("kgdlrobpmjcthqsafeni"), "", 19, 0, 19));
  BOOST_TEST(testFLN(S("dfkechomjapgnslbtqir"), "beafg", 19, 0, 19));
  BOOST_TEST(testFLN(S("rloadknfbqtgmhcsipje"), "iclat", 19, 1, 19));
  BOOST_TEST(testFLN(S("mgjhkolrnadqbpetcifs"), "rkhnf", 19, 2, 19));
  BOOST_TEST(testFLN(S("cmlfakiojdrgtbsphqen"), "clshq", 19, 4, 19));
  BOOST_TEST(testFLN(S("kghbfipeomsntdalrqjc"), "dtcoj", 19, 5, 17));
  BOOST_TEST(testFLN(S("eldiqckrnmtasbghjfpo"), "rqosnjmfth", 19, 0, 19));
  BOOST_TEST(testFLN(S("abqjcfedgotihlnspkrm"), "siatdfqglh", 19, 1, 19));
  BOOST_TEST(testFLN(S("qfbadrtjsimkolcenhpg"), "mrlshtpgjq", 19, 5, 19));
  BOOST_TEST(testFLN(S("abseghclkjqifmtodrnp"), "adlcskgqjt", 19, 9, 19));
  BOOST_TEST(testFLN(S("ibmsnlrjefhtdokacqpg"), "drshcjknaf", 19, 10, 19));
  BOOST_TEST(testFLN(S("mrkfciqjebaponsthldg"), "etsaqroinghpkjdlfcbm", 19, 0, 19));
  BOOST_TEST(testFLN(S("mjkticdeoqshpalrfbgn"), "sgepdnkqliambtrocfhj", 19, 1, 19));
  BOOST_TEST(testFLN(S("rqnoclbdejgiphtfsakm"), "nlmcjaqgbsortfdihkpe", 19, 10, 18));
  BOOST_TEST(testFLN(S("plkqbhmtfaeodjcrsing"), "racfnpmosldibqkghjet", 19, 19, 7));
  BOOST_TEST(testFLN(S("oegalhmstjrfickpbndq"), "fjhdsctkqeiolagrnmbp", 19, 20, S::npos));
  BOOST_TEST(testFLN(S("rdtgjcaohpblniekmsfq"), "", 20, 0, 19));
  BOOST_TEST(testFLN(S("ofkqbnjetrmsaidphglc"), "ejanp", 20, 0, 19));
  BOOST_TEST(testFLN(S("grkpahljcftesdmonqib"), "odife", 20, 1, 19));
  BOOST_TEST(testFLN(S("jimlgbhfqkteospardcn"), "okaqd", 20, 2, 19));
  BOOST_TEST(testFLN(S("gftenihpmslrjkqadcob"), "lcdbi", 20, 4, 18));
  BOOST_TEST(testFLN(S("bmhldogtckrfsanijepq"), "fsqbj", 20, 5, 18));
  BOOST_TEST(testFLN(S("nfqkrpjdesabgtlcmoih"), "bigdomnplq", 20, 0, 19));
  BOOST_TEST(testFLN(S("focalnrpiqmdkstehbjg"), "apiblotgcd", 20, 1, 19));
  BOOST_TEST(testFLN(S("rhqdspkmebiflcotnjga"), "acfhdenops", 20, 5, 18));
  BOOST_TEST(testFLN(S("rahdtmsckfboqlpniegj"), "jopdeamcrk", 20, 9, 18));
  BOOST_TEST(testFLN(S("fbkeiopclstmdqranjhg"), "trqncbkgmh", 20, 10, 17));
  BOOST_TEST(testFLN(S("lifhpdgmbconstjeqark"), "tomglrkencbsfjqpihda", 20, 0, 19));
  BOOST_TEST(testFLN(S("pboqganrhedjmltsicfk"), "gbkhdnpoietfcmrslajq", 20, 1, 19));
  BOOST_TEST(testFLN(S("klchabsimetjnqgorfpd"), "rtfnmbsglkjaichoqedp", 20, 10, 19));
  BOOST_TEST(testFLN(S("sirfgmjqhctndbklaepo"), "ohkmdpfqbsacrtjnlgei", 20, 19, 1));
  BOOST_TEST(testFLN(S("rlbdsiceaonqjtfpghkm"), "dlbrteoisgphmkncajfq", 20, 20, S::npos));
  BOOST_TEST(testFLN(S("ecgdanriptblhjfqskom"), "", 21, 0, 19));
  BOOST_TEST(testFLN(S("fdmiarlpgcskbhoteqjn"), "sjrlo", 21, 0, 19));
  BOOST_TEST(testFLN(S("rlbstjqopignecmfadkh"), "qjpor", 21, 1, 19));
  BOOST_TEST(testFLN(S("grjpqmbshektdolcafni"), "odhfn", 21, 2, 19));
  BOOST_TEST(testFLN(S("sakfcohtqnibprjmlged"), "qtfin", 21, 4, 19));
  BOOST_TEST(testFLN(S("mjtdglasihqpocebrfkn"), "hpqfo", 21, 5, 19));
  BOOST_TEST(testFLN(S("okaplfrntghqbmeicsdj"), "fabmertkos", 21, 0, 19));
  BOOST_TEST(testFLN(S("sahngemrtcjidqbklfpo"), "brqtgkmaej", 21, 1, 19));
  BOOST_TEST(testFLN(S("dlmsipcnekhbgoaftqjr"), "nfrdeihsgl", 21, 5, 18));
  BOOST_TEST(testFLN(S("ahegrmqnoiklpfsdbcjt"), "hlfrosekpi", 21, 9, 19));
  BOOST_TEST(testFLN(S("hdsjbnmlegtkqripacof"), "atgbkrjdsm", 21, 10, 19));
  BOOST_TEST(testFLN(S("pcnedrfjihqbalkgtoms"), "blnrptjgqmaifsdkhoec", 21, 0, 19));
  BOOST_TEST(testFLN(S("qjidealmtpskrbfhocng"), "ctpmdahebfqjgknloris", 21, 1, 19));
  BOOST_TEST(testFLN(S("qeindtagmokpfhsclrbj"), "apnkeqthrmlbfodiscgj", 21, 10, 19));
  BOOST_TEST(testFLN(S("kpfegbjhsrnodltqciam"), "jdgictpframeoqlsbknh", 21, 19, 7));
  BOOST_TEST(testFLN(S("hnbrcplsjfgiktoedmaq"), "qprlsfojamgndekthibc", 21, 20, S::npos));
}

#include <iostream>

// done
void
testReplace()
{
  // replace(size_type pos1, size_type n1, const charT* s, size_type n2); 
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(5, 2, fs1.data() + 1, 8) == "helloelloworlrld");
  }
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(3, 2, fs1.data() + 2, 2) == "helllworld");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(5, 2, fs2.data(), 2) == "0123401789");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(1, 3, fs2.data() + 1, 5) == "012345456789");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(0, 5, fs2.data(), 5) == "0123456789");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(0, 5, fs2.data() + 5, 5) == "5678956789");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(5, 2, fs2.data() + 3, 5) == "0123434567789");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(5, 2, fs2.data() + 7, 3) == "01234789789");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(3, 5, fs2.data() + 4, 2) == "0124589");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(3, 5, fs2.data() + 1, 3) == "01212389");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(0, 10, fs2.data(), 10) == "0123456789");
  }
  {
    static_string<20> fs2 = "0123456789";
    BOOST_TEST(fs2.replace(0, 10, fs2.data(), 5) == "01234");
  }
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(4, 3, fs1.data() + 1, 3) == "hellellrld");
  }
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST_EQ(fs1.replace(0, 1, fs1.data() + 4, 4), static_string<20>("oworelloworld"));
  }
  // replace(size_type pos1, size_type n1, const basic_string& str);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(0, fs1.size(), fs1) == "helloworld");
  }
  // replace(size_type pos1, size_type n1, const basic_string& str); unchecked
  {
    static_string<20> fs1 = "helloworld";
    static_string<15> fs2 = "helloworld";
    BOOST_TEST(fs1.replace(0, fs1.size(), fs2) == "helloworld");
  }
  // replace(size_type pos1, size_type n1, const basic_string& str, size_type pos2, size_type n2 = npos);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(0, fs1.size(), fs1, 0, fs1.size()) == "helloworld");
  }
  // replace(size_type pos1, size_type n1, const basic_string& str, size_type pos2, size_type n2 = npos); unchecked
  {
    static_string<20> fs1 = "helloworld";
    static_string<15> fs2 = "helloworld";
    BOOST_TEST(fs1.replace(0, fs1.size(), fs2, 0, fs2.size()) == "helloworld");
  }
  // replace(size_type pos1, size_type n1, const T& t);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(0, fs1.size(), string_view(fs1)) == "helloworld");
  }
  // replace(size_type pos1, size_type n1, const T& t, size_type pos2, size_type n2 = npos);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(0, fs1.size(), string_view(fs1), 0, fs1.size()) == "helloworld");
  }
  // replace(size_type pos, size_type n, const charT * s);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(0, fs1.size(), fs1.data()) == "helloworld");
  }
  // replace(size_type pos1, size_type n1, size_type n2, charT c);]
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(0, fs1.size(), fs1.size(), 'a') == "aaaaaaaaaa");
  }
  // replace(const_iterator i1, const_iterator i2, const basic_string& str);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(fs1.begin(), fs1.end(), fs1) == "helloworld");
  }
  // replace(const_iterator i1, const_iterator i2, const basic_string& str); unchecked
  {
    static_string<20> fs1 = "helloworld";
    static_string<15> fs2 = "helloworld";
    BOOST_TEST(fs1.replace(fs1.begin(), fs1.end(), fs2) == "helloworld");
  }
  // replace(const_iterator i1, const_iterator i2, const T& t);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(fs1.begin(), fs1.end(), string_view(fs1)) == "helloworld");
  }
  // replace(const_iterator i1, const_iterator i2, const charT* s, size_type n);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(fs1.begin(), fs1.end(), fs1.data(), fs1.size()) == "helloworld");
  }
  // replace(const_iterator i1, const_iterator i2, const charT* s);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(fs1.begin(), fs1.end(), fs1.data()) == "helloworld");
  }
  // replace(const_iterator i1, const_iterator i2, size_type n, charT c);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(fs1.begin(), fs1.end(), fs1.size(), 'a') == "aaaaaaaaaa");
  }
  // replace(const_iterator i1, const_iterator i2, InputIterator j1, InputIterator j2);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(fs1.begin(), fs1.begin() + 5, fs1.begin(), fs1.end()) == "helloworldworld");
  }
  // replace(const_iterator i1, const_iterator i2, initializer_list<charT> il);
  {
    static_string<20> fs1 = "helloworld";
    BOOST_TEST(fs1.replace(fs1.begin(), fs1.end(), {'h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd'}) == "helloworld");
  }
  // replace(const_iterator, const_iterator, InputIterator, InputIterator)
  {
    std::stringstream a("defghi");
    static_string<30> b = "abcabcdefjklmnop";
    BOOST_TEST(b.replace(b.begin() + 3, b.begin() + 9,
      std::istream_iterator<char>(a), 
      std::istream_iterator<char>()) ==
      "abcdefghijklmnop");
  }

  using S = static_string<400>;
  S s_short = "123/";
  S s_long = "Lorem ipsum dolor sit amet, consectetur/";

  BOOST_TEST(s_short.replace(s_short.begin(), s_short.begin(), s_short.begin(), s_short.end()) == "123/123/");
  BOOST_TEST(s_short.replace(s_short.begin(), s_short.begin(), s_short.begin(), s_short.end()) == "123/123/123/123/");
  BOOST_TEST(s_short.replace(s_short.begin(), s_short.begin(), s_short.begin(), s_short.end()) == "123/123/123/123/123/123/123/123/");
  BOOST_TEST(s_long.replace(s_long.begin(), s_long.begin(), s_long.begin(), s_long.end()) == "Lorem ipsum dolor sit amet, consectetur/Lorem ipsum dolor sit amet, consectetur/");

  BOOST_TEST(testR(S(""), 0, 0, "", S("")));
  BOOST_TEST(testR(S(""), 0, 0, "12345", S("12345")));
  BOOST_TEST(testR(S(""), 0, 0, "1234567890", S("1234567890")));
  BOOST_TEST(testR(S(""), 0, 0, "12345678901234567890", S("12345678901234567890")));
  BOOST_TEST(testR(S("abcde"), 0, 0, "", S("abcde")));
  BOOST_TEST(testR(S("abcde"), 0, 0, "12345", S("12345abcde")));
  BOOST_TEST(testR(S("abcde"), 0, 0, "1234567890", S("1234567890abcde")));
  BOOST_TEST(testR(S("abcde"), 0, 0, "12345678901234567890", S("12345678901234567890abcde")));
  BOOST_TEST(testR(S("abcde"), 0, 1, "", S("bcde")));
  BOOST_TEST(testR(S("abcde"), 0, 1, "12345", S("12345bcde")));
  BOOST_TEST(testR(S("abcde"), 0, 1, "1234567890", S("1234567890bcde")));
  BOOST_TEST(testR(S("abcde"), 0, 1, "12345678901234567890", S("12345678901234567890bcde")));
  BOOST_TEST(testR(S("abcde"), 0, 2, "", S("cde")));
  BOOST_TEST(testR(S("abcde"), 0, 2, "12345", S("12345cde")));
  BOOST_TEST(testR(S("abcde"), 0, 2, "1234567890", S("1234567890cde")));
  BOOST_TEST(testR(S("abcde"), 0, 2, "12345678901234567890", S("12345678901234567890cde")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "", S("e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345", S("12345e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "1234567890", S("1234567890e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345678901234567890", S("12345678901234567890e")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "", S("")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345", S("12345")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "1234567890", S("1234567890")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345678901234567890", S("12345678901234567890")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "", S("abcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345", S("a12345bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "1234567890", S("a1234567890bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345678901234567890", S("a12345678901234567890bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "", S("acde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345", S("a12345cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "1234567890", S("a1234567890cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345678901234567890", S("a12345678901234567890cde")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "", S("ade")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345", S("a12345de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "1234567890", S("a1234567890de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345678901234567890", S("a12345678901234567890de")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "", S("ae")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "12345", S("a12345e")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "1234567890", S("a1234567890e")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "12345678901234567890", S("a12345678901234567890e")));
  BOOST_TEST(testR(S("abcde"), 1, 4, "", S("a")));
  BOOST_TEST(testR(S("abcde"), 1, 4, "12345", S("a12345")));
  BOOST_TEST(testR(S("abcde"), 1, 4, "1234567890", S("a1234567890")));
  BOOST_TEST(testR(S("abcde"), 1, 4, "12345678901234567890", S("a12345678901234567890")));
  BOOST_TEST(testR(S("abcde"), 2, 0, "", S("abcde")));
  BOOST_TEST(testR(S("abcde"), 2, 0, "12345", S("ab12345cde")));
  BOOST_TEST(testR(S("abcde"), 2, 0, "1234567890", S("ab1234567890cde")));
  BOOST_TEST(testR(S("abcde"), 2, 0, "12345678901234567890", S("ab12345678901234567890cde")));
  BOOST_TEST(testR(S("abcde"), 2, 1, "", S("abde")));
  BOOST_TEST(testR(S("abcde"), 2, 1, "12345", S("ab12345de")));
  BOOST_TEST(testR(S("abcde"), 2, 1, "1234567890", S("ab1234567890de")));
  BOOST_TEST(testR(S("abcde"), 2, 1, "12345678901234567890", S("ab12345678901234567890de")));
  BOOST_TEST(testR(S("abcde"), 2, 2, "", S("abe")));
  BOOST_TEST(testR(S("abcde"), 2, 2, "12345", S("ab12345e")));
  BOOST_TEST(testR(S("abcde"), 2, 2, "1234567890", S("ab1234567890e")));
  BOOST_TEST(testR(S("abcde"), 2, 2, "12345678901234567890", S("ab12345678901234567890e")));
  BOOST_TEST(testR(S("abcde"), 2, 3, "", S("ab")));
  BOOST_TEST(testR(S("abcde"), 2, 3, "12345", S("ab12345")));
  BOOST_TEST(testR(S("abcde"), 2, 3, "1234567890", S("ab1234567890")));
  BOOST_TEST(testR(S("abcde"), 2, 3, "12345678901234567890", S("ab12345678901234567890")));
  BOOST_TEST(testR(S("abcde"), 4, 0, "", S("abcde")));
  BOOST_TEST(testR(S("abcde"), 4, 0, "12345", S("abcd12345e")));
  BOOST_TEST(testR(S("abcde"), 4, 0, "1234567890", S("abcd1234567890e")));
  BOOST_TEST(testR(S("abcde"), 4, 0, "12345678901234567890", S("abcd12345678901234567890e")));
  BOOST_TEST(testR(S("abcde"), 4, 1, "", S("abcd")));
  BOOST_TEST(testR(S("abcde"), 4, 1, "12345", S("abcd12345")));
  BOOST_TEST(testR(S("abcde"), 4, 1, "1234567890", S("abcd1234567890")));
  BOOST_TEST(testR(S("abcde"), 4, 1, "12345678901234567890", S("abcd12345678901234567890")));
  BOOST_TEST(testR(S("abcde"), 5, 0, "", S("abcde")));
  BOOST_TEST(testR(S("abcde"), 5, 0, "12345", S("abcde12345")));
  BOOST_TEST(testR(S("abcde"), 5, 0, "1234567890", S("abcde1234567890")));
  BOOST_TEST(testR(S("abcde"), 5, 0, "12345678901234567890", S("abcde12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 0, "", S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 0, "12345", S("12345abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 0, "1234567890", S("1234567890abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 0, "12345678901234567890", S("12345678901234567890abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 1, "", S("bcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 1, "12345", S("12345bcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 1, "1234567890", S("1234567890bcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 1, "12345678901234567890", S("12345678901234567890bcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 5, "", S("fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 5, "12345", S("12345fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 5, "1234567890", S("1234567890fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 5, "12345678901234567890", S("12345678901234567890fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 9, "", S("j")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 9, "12345", S("12345j")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 9, "1234567890", S("1234567890j")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 9, "12345678901234567890", S("12345678901234567890j")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 10, "", S("")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 10, "12345", S("12345")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 10, "1234567890", S("1234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 0, 10, "12345678901234567890", S("12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 0, "", S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 0, "12345", S("a12345bcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 0, "1234567890", S("a1234567890bcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 0, "12345678901234567890", S("a12345678901234567890bcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 1, "", S("acdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 1, "12345", S("a12345cdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 1, "1234567890", S("a1234567890cdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 1, "12345678901234567890", S("a12345678901234567890cdefghij")));
  BOOST_TEST(testR(S(""), 0, 0, 0, '2', S("")));
  BOOST_TEST(testR(S(""), 0, 0, 5, '2', S("22222")));
  BOOST_TEST(testR(S(""), 0, 0, 10, '2', S("2222222222")));
  BOOST_TEST(testR(S(""), 0, 0, 20, '2', S("22222222222222222222")));
  BOOST_TEST(testR(S(""), 0, 1, 0, '2', S("")));
  BOOST_TEST(testR(S(""), 0, 1, 5, '2', S("22222")));
  BOOST_TEST(testR(S(""), 0, 1, 10, '2', S("2222222222")));
  BOOST_TEST(testR(S(""), 0, 1, 20, '2', S("22222222222222222222")));
  BOOST_TEST(testR(S("abcde"), 0, 0, 0, '2', S("abcde")));
  BOOST_TEST(testR(S("abcde"), 0, 0, 5, '2', S("22222abcde")));
  BOOST_TEST(testR(S("abcde"), 0, 0, 10, '2', S("2222222222abcde")));
  BOOST_TEST(testR(S("abcde"), 0, 0, 20, '2', S("22222222222222222222abcde")));
  BOOST_TEST(testR(S("abcde"), 0, 1, 0, '2', S("bcde")));
  BOOST_TEST(testR(S("abcde"), 0, 1, 5, '2', S("22222bcde")));
  BOOST_TEST(testR(S("abcde"), 0, 1, 10, '2', S("2222222222bcde")));
  BOOST_TEST(testR(S("abcde"), 0, 1, 20, '2', S("22222222222222222222bcde")));
  BOOST_TEST(testR(S("abcde"), 0, 2, 0, '2', S("cde")));
  BOOST_TEST(testR(S("abcde"), 0, 2, 5, '2', S("22222cde")));
  BOOST_TEST(testR(S("abcde"), 0, 2, 10, '2', S("2222222222cde")));
  BOOST_TEST(testR(S("abcde"), 0, 2, 20, '2', S("22222222222222222222cde")));
  BOOST_TEST(testR(S("abcde"), 0, 4, 0, '2', S("e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, 5, '2', S("22222e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, 10, '2', S("2222222222e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, 20, '2', S("22222222222222222222e")));
  BOOST_TEST(testR(S("abcde"), 0, 5, 0, '2', S("")));
  BOOST_TEST(testR(S("abcde"), 0, 5, 5, '2', S("22222")));
  BOOST_TEST(testR(S("abcde"), 0, 5, 10, '2', S("2222222222")));
  BOOST_TEST(testR(S("abcde"), 0, 5, 20, '2', S("22222222222222222222")));
  BOOST_TEST(testR(S("abcde"), 0, 6, 0, '2', S("")));
  BOOST_TEST(testR(S("abcde"), 0, 6, 5, '2', S("22222")));
  BOOST_TEST(testR(S("abcde"), 0, 6, 10, '2', S("2222222222")));
  BOOST_TEST(testR(S("abcde"), 0, 6, 20, '2', S("22222222222222222222")));
  BOOST_TEST(testR(S("abcde"), 1, 0, 0, '2', S("abcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, 5, '2', S("a22222bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, 10, '2', S("a2222222222bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, 20, '2', S("a22222222222222222222bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, 0, '2', S("acde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, 5, '2', S("a22222cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, 10, '2', S("a2222222222cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, 20, '2', S("a22222222222222222222cde")));
  BOOST_TEST(testR(S("abcde"), 1, 2, 0, '2', S("ade")));
  BOOST_TEST(testR(S("abcde"), 1, 2, 5, '2', S("a22222de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, 10, '2', S("a2222222222de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, 20, '2', S("a22222222222222222222de")));
  BOOST_TEST(testR(S("abcde"), 1, 3, 0, '2', S("ae")));
  BOOST_TEST(testR(S("abcde"), 1, 3, 5, '2', S("a22222e")));
  BOOST_TEST(testR(S("abcde"), 1, 3, 10, '2', S("a2222222222e")));
  BOOST_TEST(testR(S("abcde"), 1, 3, 20, '2', S("a22222222222222222222e")));
  BOOST_TEST(testR(S("abcde"), 1, 4, 0, '2', S("a")));
  BOOST_TEST(testR(S("abcde"), 1, 4, 5, '2', S("a22222")));
  BOOST_TEST(testR(S("abcde"), 1, 4, 10, '2', S("a2222222222")));
  BOOST_TEST(testR(S("abcde"), 1, 4, 20, '2', S("a22222222222222222222")));
  BOOST_TEST(testR(S("abcde"), 1, 5, 0, '2', S("a")));
  BOOST_TEST(testR(S("abcde"), 1, 5, 5, '2', S("a22222")));
  BOOST_TEST(testR(S("abcde"), 1, 5, 10, '2', S("a2222222222")));
  BOOST_TEST(testR(S("abcde"), 1, 5, 20, '2', S("a22222222222222222222")));
  BOOST_TEST(testR(S("abcde"), 2, 0, 0, '2', S("abcde")));
  BOOST_TEST(testR(S("abcde"), 2, 0, 5, '2', S("ab22222cde")));
  BOOST_TEST(testR(S("abcde"), 2, 0, 10, '2', S("ab2222222222cde")));
  BOOST_TEST(testR(S("abcde"), 2, 0, 20, '2', S("ab22222222222222222222cde")));
  BOOST_TEST(testR(S("abcde"), 2, 1, 0, '2', S("abde")));
  BOOST_TEST(testR(S("abcde"), 2, 1, 5, '2', S("ab22222de")));
  BOOST_TEST(testR(S("abcde"), 2, 1, 10, '2', S("ab2222222222de")));
  BOOST_TEST(testR(S("abcde"), 2, 1, 20, '2', S("ab22222222222222222222de")));
  BOOST_TEST(testR(S("abcde"), 2, 2, 0, '2', S("abe")));
  BOOST_TEST(testR(S("abcde"), 2, 2, 5, '2', S("ab22222e")));
  BOOST_TEST(testR(S("abcde"), 2, 2, 10, '2', S("ab2222222222e")));
  BOOST_TEST(testR(S("abcde"), 2, 2, 20, '2', S("ab22222222222222222222e")));
  BOOST_TEST(testR(S("abcde"), 2, 3, 0, '2', S("ab")));
  BOOST_TEST(testR(S("abcde"), 2, 3, 5, '2', S("ab22222")));
  BOOST_TEST(testR(S("abcde"), 2, 3, 10, '2', S("ab2222222222")));
  BOOST_TEST(testR(S("abcde"), 2, 3, 20, '2', S("ab22222222222222222222")));
  BOOST_TEST(testR(S("abcde"), 2, 4, 0, '2', S("ab")));
  BOOST_TEST(testR(S("abcde"), 2, 4, 5, '2', S("ab22222")));
  BOOST_TEST(testR(S("abcde"), 2, 4, 10, '2', S("ab2222222222")));
  BOOST_TEST(testR(S("abcde"), 2, 4, 20, '2', S("ab22222222222222222222")));
  BOOST_TEST(testR(S("abcde"), 4, 0, 0, '2', S("abcde")));
  BOOST_TEST(testR(S("abcde"), 4, 0, 5, '2', S("abcd22222e")));
  BOOST_TEST(testR(S("abcde"), 4, 0, 10, '2', S("abcd2222222222e")));
  BOOST_TEST(testR(S("abcde"), 4, 0, 20, '2', S("abcd22222222222222222222e")));
  BOOST_TEST(testR(S("abcde"), 4, 1, 0, '2', S("abcd")));
  BOOST_TEST(testR(S("abcde"), 4, 1, 5, '2', S("abcd22222")));
  BOOST_TEST(testR(S("abcde"), 4, 1, 10, '2', S("abcd2222222222")));
  BOOST_TEST(testR(S("abcde"), 4, 1, 20, '2', S("abcd22222222222222222222")));
  BOOST_TEST(testR(S("abcde"), 4, 2, 0, '2', S("abcd")));

  BOOST_TEST(testR(S("abcde"), 4, 2, 5, '2', S("abcd22222")));
  BOOST_TEST(testR(S("abcde"), 4, 2, 10, '2', S("abcd2222222222")));
  BOOST_TEST(testR(S("abcde"), 4, 2, 20, '2', S("abcd22222222222222222222")));

  BOOST_TEST(testR(S("abcde"), 5, 0, 0, '2', S("abcde")));
  BOOST_TEST(testR(S("abcde"), 5, 0, 5, '2', S("abcde22222")));
  BOOST_TEST(testR(S("abcde"), 5, 0, 10, '2', S("abcde2222222222")));
  BOOST_TEST(testR(S("abcde"), 5, 0, 20, '2', S("abcde22222222222222222222")));

  BOOST_TEST(testR(S("abcde"), 5, 1, 0, '2', S("abcde")));
  BOOST_TEST(testR(S("abcde"), 5, 1, 5, '2', S("abcde22222")));
  BOOST_TEST(testR(S("abcde"), 5, 1, 10, '2', S("abcde2222222222")));
  BOOST_TEST(testR(S("abcde"), 5, 1, 20, '2', S("abcde22222222222222222222")));
  
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345", 4, S("1234e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345", 5, S("12345e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "1234567890", 0, S("e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "1234567890", 1, S("1e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "1234567890", 5, S("12345e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "1234567890", 9, S("123456789e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "1234567890", 10, S("1234567890e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345678901234567890", 0, S("e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345678901234567890", 1, S("1e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345678901234567890", 10, S("1234567890e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345678901234567890", 19, S("1234567890123456789e")));
  BOOST_TEST(testR(S("abcde"), 0, 4, "12345678901234567890", 20, S("12345678901234567890e")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "", 0, S("")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345", 0, S("")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345", 1, S("1")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345", 2, S("12")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345", 4, S("1234")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345", 5, S("12345")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "1234567890", 0, S("")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "1234567890", 1, S("1")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "1234567890", 5, S("12345")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "1234567890", 9, S("123456789")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "1234567890", 10, S("1234567890")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345678901234567890", 0, S("")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345678901234567890", 1, S("1")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345678901234567890", 10, S("1234567890")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345678901234567890", 19, S("1234567890123456789")));
  BOOST_TEST(testR(S("abcde"), 0, 5, "12345678901234567890", 20, S("12345678901234567890")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "", 0, S("")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345", 0, S("")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345", 1, S("1")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345", 2, S("12")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345", 4, S("1234")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345", 5, S("12345")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "1234567890", 0, S("")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "1234567890", 1, S("1")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "1234567890", 5, S("12345")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "1234567890", 9, S("123456789")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "1234567890", 10, S("1234567890")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345678901234567890", 0, S("")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345678901234567890", 1, S("1")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345678901234567890", 10, S("1234567890")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345678901234567890", 19, S("1234567890123456789")));
  BOOST_TEST(testR(S("abcde"), 0, 6, "12345678901234567890", 20, S("12345678901234567890")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "", 0, S("abcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345", 0, S("abcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345", 1, S("a1bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345", 2, S("a12bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345", 4, S("a1234bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345", 5, S("a12345bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "1234567890", 0, S("abcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "1234567890", 1, S("a1bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "1234567890", 5, S("a12345bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "1234567890", 9, S("a123456789bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "1234567890", 10, S("a1234567890bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345678901234567890", 0, S("abcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345678901234567890", 1, S("a1bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345678901234567890", 10, S("a1234567890bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345678901234567890", 19, S("a1234567890123456789bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 0, "12345678901234567890", 20, S("a12345678901234567890bcde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "", 0, S("acde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345", 0, S("acde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345", 1, S("a1cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345", 2, S("a12cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345", 4, S("a1234cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345", 5, S("a12345cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "1234567890", 0, S("acde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "1234567890", 1, S("a1cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "1234567890", 5, S("a12345cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "1234567890", 9, S("a123456789cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "1234567890", 10, S("a1234567890cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345678901234567890", 0, S("acde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345678901234567890", 1, S("a1cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345678901234567890", 10, S("a1234567890cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345678901234567890", 19, S("a1234567890123456789cde")));
  BOOST_TEST(testR(S("abcde"), 1, 1, "12345678901234567890", 20, S("a12345678901234567890cde")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "", 0, S("ade")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345", 0, S("ade")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345", 1, S("a1de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345", 2, S("a12de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345", 4, S("a1234de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345", 5, S("a12345de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "1234567890", 0, S("ade")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "1234567890", 1, S("a1de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "1234567890", 5, S("a12345de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "1234567890", 9, S("a123456789de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "1234567890", 10, S("a1234567890de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345678901234567890", 0, S("ade")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345678901234567890", 1, S("a1de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345678901234567890", 10, S("a1234567890de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345678901234567890", 19, S("a1234567890123456789de")));
  BOOST_TEST(testR(S("abcde"), 1, 2, "12345678901234567890", 20, S("a12345678901234567890de")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "", 0, S("ae")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "12345", 0, S("ae")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "12345", 1, S("a1e")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "12345", 2, S("a12e")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "12345", 4, S("a1234e")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "12345", 5, S("a12345e")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "1234567890", 0, S("ae")));
  BOOST_TEST(testR(S("abcde"), 1, 3, "1234567890", 1, S("a1e")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 10, "1234567890", 5, S("a12345")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 10, "1234567890", 9, S("a123456789")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 10, "1234567890", 10, S("a1234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 10, "12345678901234567890", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 10, "12345678901234567890", 1, S("a1")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 10, "12345678901234567890", 10, S("a1234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 10, "12345678901234567890", 19, S("a1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghij"), 1, 10, "12345678901234567890", 20, S("a12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345", 1, S("abcde1fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345", 2, S("abcde12fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345", 4, S("abcde1234fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345", 5, S("abcde12345fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "1234567890", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "1234567890", 1, S("abcde1fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "1234567890", 5, S("abcde12345fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "1234567890", 9, S("abcde123456789fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "1234567890", 10, S("abcde1234567890fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345678901234567890", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345678901234567890", 1, S("abcde1fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345678901234567890", 10, S("abcde1234567890fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345678901234567890", 19, S("abcde1234567890123456789fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 0, "12345678901234567890", 20, S("abcde12345678901234567890fghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "", 0, S("abcdeghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345", 0, S("abcdeghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345", 1, S("abcde1ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345", 2, S("abcde12ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345", 4, S("abcde1234ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345", 5, S("abcde12345ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "1234567890", 0, S("abcdeghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "1234567890", 1, S("abcde1ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "1234567890", 5, S("abcde12345ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "1234567890", 9, S("abcde123456789ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "1234567890", 10, S("abcde1234567890ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345678901234567890", 0, S("abcdeghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345678901234567890", 1, S("abcde1ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345678901234567890", 10, S("abcde1234567890ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345678901234567890", 19, S("abcde1234567890123456789ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 1, "12345678901234567890", 20, S("abcde12345678901234567890ghij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "", 0, S("abcdehij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345", 0, S("abcdehij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345", 1, S("abcde1hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345", 2, S("abcde12hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345", 4, S("abcde1234hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345", 5, S("abcde12345hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "1234567890", 0, S("abcdehij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "1234567890", 1, S("abcde1hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "1234567890", 5, S("abcde12345hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "1234567890", 9, S("abcde123456789hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "1234567890", 10, S("abcde1234567890hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345678901234567890", 0, S("abcdehij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345678901234567890", 1, S("abcde1hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345678901234567890", 10, S("abcde1234567890hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345678901234567890", 19, S("abcde1234567890123456789hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 2, "12345678901234567890", 20, S("abcde12345678901234567890hij")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "", 0, S("abcdej")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345", 0, S("abcdej")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345", 1, S("abcde1j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345", 2, S("abcde12j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345", 4, S("abcde1234j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345", 5, S("abcde12345j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "1234567890", 0, S("abcdej")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "1234567890", 1, S("abcde1j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "1234567890", 5, S("abcde12345j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "1234567890", 9, S("abcde123456789j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "1234567890", 10, S("abcde1234567890j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345678901234567890", 0, S("abcdej")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345678901234567890", 1, S("abcde1j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345678901234567890", 10, S("abcde1234567890j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345678901234567890", 19, S("abcde1234567890123456789j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 4, "12345678901234567890", 20, S("abcde12345678901234567890j")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "", 0, S("abcde")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345", 0, S("abcde")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345", 1, S("abcde1")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345", 2, S("abcde12")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345", 4, S("abcde1234")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345", 5, S("abcde12345")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "1234567890", 0, S("abcde")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "1234567890", 1, S("abcde1")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "1234567890", 5, S("abcde12345")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "1234567890", 9, S("abcde123456789")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "1234567890", 10, S("abcde1234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345678901234567890", 0, S("abcde")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345678901234567890", 1, S("abcde1")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345678901234567890", 10, S("abcde1234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345678901234567890", 19, S("abcde1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 5, "12345678901234567890", 20, S("abcde12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "", 0, S("abcde")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "12345", 0, S("abcde")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "12345", 1, S("abcde1")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "12345", 2, S("abcde12")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "12345", 4, S("abcde1234")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "12345", 5, S("abcde12345")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "1234567890", 0, S("abcde")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "1234567890", 1, S("abcde1")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "1234567890", 5, S("abcde12345")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "1234567890", 9, S("abcde123456789")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "1234567890", 10, S("abcde1234567890")));
  BOOST_TEST(testR(S("abcdefghij"), 5, 6, "12345678901234567890", 0, S("abcde")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345", 1, S("1abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345", 2, S("12abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345", 4, S("1234abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345", 5, S("12345abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "1234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "1234567890", 1, S("1abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "1234567890", 5, S("12345abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "1234567890", 9, S("123456789abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "1234567890", 10, S("1234567890abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345678901234567890", 1, S("1abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345678901234567890", 10, S("1234567890abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345678901234567890", 19, S("1234567890123456789abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 0, "12345678901234567890", 20, S("12345678901234567890abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "", 0, S("bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345", 0, S("bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345", 1, S("1bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345", 2, S("12bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345", 4, S("1234bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345", 5, S("12345bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "1234567890", 0, S("bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "1234567890", 1, S("1bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "1234567890", 5, S("12345bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "1234567890", 9, S("123456789bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "1234567890", 10, S("1234567890bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345678901234567890", 0, S("bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345678901234567890", 1, S("1bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345678901234567890", 10, S("1234567890bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345678901234567890", 19, S("1234567890123456789bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 1, "12345678901234567890", 20, S("12345678901234567890bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "", 0, S("klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345", 0, S("klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345", 1, S("1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345", 2, S("12klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345", 4, S("1234klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345", 5, S("12345klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "1234567890", 0, S("klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "1234567890", 1, S("1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "1234567890", 5, S("12345klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "1234567890", 9, S("123456789klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "1234567890", 10, S("1234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345678901234567890", 0, S("klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345678901234567890", 1, S("1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345678901234567890", 10, S("1234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345678901234567890", 19, S("1234567890123456789klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 10, "12345678901234567890", 20, S("12345678901234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "", 0, S("t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345", 0, S("t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345", 1, S("1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345", 2, S("12t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345", 4, S("1234t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345", 5, S("12345t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "1234567890", 0, S("t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "1234567890", 1, S("1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "1234567890", 5, S("12345t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "1234567890", 9, S("123456789t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "1234567890", 10, S("1234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345678901234567890", 0, S("t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345678901234567890", 1, S("1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345678901234567890", 10, S("1234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345678901234567890", 19, S("1234567890123456789t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 19, "12345678901234567890", 20, S("12345678901234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "", 0, S("")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345", 0, S("")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345", 1, S("1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345", 2, S("12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345", 4, S("1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345", 5, S("12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "1234567890", 0, S("")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "1234567890", 1, S("1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "1234567890", 5, S("12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "1234567890", 9, S("123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "1234567890", 10, S("1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345678901234567890", 0, S("")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345678901234567890", 1, S("1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345678901234567890", 10, S("1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345678901234567890", 19, S("1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 20, "12345678901234567890", 20, S("12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "", 0, S("")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345", 0, S("")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345", 1, S("1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345", 2, S("12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345", 4, S("1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345", 5, S("12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "1234567890", 0, S("")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "1234567890", 1, S("1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "1234567890", 5, S("12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "1234567890", 9, S("123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "1234567890", 10, S("1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345678901234567890", 0, S("")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345678901234567890", 1, S("1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345678901234567890", 10, S("1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345678901234567890", 19, S("1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 0, 21, "12345678901234567890", 20, S("12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345", 1, S("a1bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345", 2, S("a12bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345", 4, S("a1234bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345", 5, S("a12345bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "1234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "1234567890", 1, S("a1bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "1234567890", 5, S("a12345bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "1234567890", 9, S("a123456789bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "1234567890", 10, S("a1234567890bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345678901234567890", 1, S("a1bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345678901234567890", 10, S("a1234567890bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345678901234567890", 19, S("a1234567890123456789bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 0, "12345678901234567890", 20, S("a12345678901234567890bcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "", 0, S("acdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345", 0, S("acdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345", 1, S("a1cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345", 2, S("a12cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345", 4, S("a1234cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345", 5, S("a12345cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "1234567890", 0, S("acdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "1234567890", 1, S("a1cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "1234567890", 5, S("a12345cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "1234567890", 9, S("a123456789cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "1234567890", 10, S("a1234567890cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345678901234567890", 0, S("acdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345678901234567890", 1, S("a1cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345678901234567890", 10, S("a1234567890cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345678901234567890", 19, S("a1234567890123456789cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 1, "12345678901234567890", 20, S("a12345678901234567890cdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "", 0, S("aklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345", 0, S("aklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345", 1, S("a1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345", 2, S("a12klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345", 4, S("a1234klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345", 5, S("a12345klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "1234567890", 0, S("aklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "1234567890", 1, S("a1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "1234567890", 5, S("a12345klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "1234567890", 9, S("a123456789klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "1234567890", 10, S("a1234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345678901234567890", 0, S("aklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345678901234567890", 1, S("a1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345678901234567890", 10, S("a1234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345678901234567890", 19, S("a1234567890123456789klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 9, "12345678901234567890", 20, S("a12345678901234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "", 0, S("at")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345", 0, S("at")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345", 1, S("a1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345", 2, S("a12t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345", 4, S("a1234t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345", 5, S("a12345t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "1234567890", 0, S("at")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "1234567890", 1, S("a1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "1234567890", 5, S("a12345t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "1234567890", 9, S("a123456789t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "1234567890", 10, S("a1234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345678901234567890", 0, S("at")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345678901234567890", 1, S("a1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345678901234567890", 10, S("a1234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345678901234567890", 19, S("a1234567890123456789t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 18, "12345678901234567890", 20, S("a12345678901234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345", 1, S("a1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345", 2, S("a12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345", 4, S("a1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345", 5, S("a12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "1234567890", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "1234567890", 1, S("a1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "1234567890", 5, S("a12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "1234567890", 9, S("a123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "1234567890", 10, S("a1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345678901234567890", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345678901234567890", 1, S("a1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345678901234567890", 10, S("a1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345678901234567890", 19, S("a1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 19, "12345678901234567890", 20, S("a12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345", 1, S("a1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345", 2, S("a12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345", 4, S("a1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345", 5, S("a12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "1234567890", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "1234567890", 1, S("a1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "1234567890", 5, S("a12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "1234567890", 9, S("a123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "1234567890", 10, S("a1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345678901234567890", 0, S("a")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345678901234567890", 1, S("a1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345678901234567890", 10, S("a1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345678901234567890", 19, S("a1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 1, 20, "12345678901234567890", 20, S("a12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345", 1, S("abcdefghij1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345", 2, S("abcdefghij12klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345", 4, S("abcdefghij1234klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345", 5, S("abcdefghij12345klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "1234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "1234567890", 1, S("abcdefghij1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "1234567890", 5, S("abcdefghij12345klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "1234567890", 9, S("abcdefghij123456789klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "1234567890", 10, S("abcdefghij1234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345678901234567890", 1, S("abcdefghij1klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345678901234567890", 10, S("abcdefghij1234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345678901234567890", 19, S("abcdefghij1234567890123456789klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 0, "12345678901234567890", 20, S("abcdefghij12345678901234567890klmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "", 0, S("abcdefghijlmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345", 0, S("abcdefghijlmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345", 1, S("abcdefghij1lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345", 2, S("abcdefghij12lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345", 4, S("abcdefghij1234lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345", 5, S("abcdefghij12345lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "1234567890", 0, S("abcdefghijlmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "1234567890", 1, S("abcdefghij1lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "1234567890", 5, S("abcdefghij12345lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "1234567890", 9, S("abcdefghij123456789lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "1234567890", 10, S("abcdefghij1234567890lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345678901234567890", 0, S("abcdefghijlmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345678901234567890", 1, S("abcdefghij1lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345678901234567890", 10, S("abcdefghij1234567890lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345678901234567890", 19, S("abcdefghij1234567890123456789lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 1, "12345678901234567890", 20, S("abcdefghij12345678901234567890lmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "", 0, S("abcdefghijpqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345", 0, S("abcdefghijpqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345", 1, S("abcdefghij1pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345", 2, S("abcdefghij12pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345", 4, S("abcdefghij1234pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345", 5, S("abcdefghij12345pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "1234567890", 0, S("abcdefghijpqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "1234567890", 1, S("abcdefghij1pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "1234567890", 5, S("abcdefghij12345pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "1234567890", 9, S("abcdefghij123456789pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "1234567890", 10, S("abcdefghij1234567890pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345678901234567890", 0, S("abcdefghijpqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345678901234567890", 1, S("abcdefghij1pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345678901234567890", 10, S("abcdefghij1234567890pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345678901234567890", 19, S("abcdefghij1234567890123456789pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 5, "12345678901234567890", 20, S("abcdefghij12345678901234567890pqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "", 0, S("abcdefghijt")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345", 0, S("abcdefghijt")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345", 1, S("abcdefghij1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345", 2, S("abcdefghij12t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345", 4, S("abcdefghij1234t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345", 5, S("abcdefghij12345t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "1234567890", 0, S("abcdefghijt")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "1234567890", 1, S("abcdefghij1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "1234567890", 5, S("abcdefghij12345t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "1234567890", 9, S("abcdefghij123456789t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "1234567890", 10, S("abcdefghij1234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345678901234567890", 0, S("abcdefghijt")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345678901234567890", 1, S("abcdefghij1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345678901234567890", 10, S("abcdefghij1234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345678901234567890", 19, S("abcdefghij1234567890123456789t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 9, "12345678901234567890", 20, S("abcdefghij12345678901234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345", 1, S("abcdefghij1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345", 2, S("abcdefghij12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345", 4, S("abcdefghij1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345", 5, S("abcdefghij12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "1234567890", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "1234567890", 1, S("abcdefghij1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "1234567890", 5, S("abcdefghij12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "1234567890", 9, S("abcdefghij123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "1234567890", 10, S("abcdefghij1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345678901234567890", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345678901234567890", 1, S("abcdefghij1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345678901234567890", 10, S("abcdefghij1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345678901234567890", 19, S("abcdefghij1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 10, "12345678901234567890", 20, S("abcdefghij12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345", 1, S("abcdefghij1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345", 2, S("abcdefghij12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345", 4, S("abcdefghij1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345", 5, S("abcdefghij12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "1234567890", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "1234567890", 1, S("abcdefghij1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "1234567890", 5, S("abcdefghij12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "1234567890", 9, S("abcdefghij123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "1234567890", 10, S("abcdefghij1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345678901234567890", 0, S("abcdefghij")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345678901234567890", 1, S("abcdefghij1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345678901234567890", 10, S("abcdefghij1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345678901234567890", 19, S("abcdefghij1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 10, 11, "12345678901234567890", 20, S("abcdefghij12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345", 1, S("abcdefghijklmnopqrs1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345", 2, S("abcdefghijklmnopqrs12t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345", 4, S("abcdefghijklmnopqrs1234t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345", 5, S("abcdefghijklmnopqrs12345t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "1234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "1234567890", 1, S("abcdefghijklmnopqrs1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "1234567890", 5, S("abcdefghijklmnopqrs12345t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "1234567890", 9, S("abcdefghijklmnopqrs123456789t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "1234567890", 10, S("abcdefghijklmnopqrs1234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S(""), 1, 0, "12345", 0, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345", 1, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345", 2, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345", 4, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345", 5, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "1234567890", 0, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "1234567890", 1, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "1234567890", 5, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "1234567890", 9, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "1234567890", 10, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345678901234567890", 0, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345678901234567890", 1, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345678901234567890", 10, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345678901234567890", 19, S("can't happen")));
  BOOST_TEST(testR(S(""), 1, 0, "12345678901234567890", 20, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345", 2, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345", 4, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345", 5, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "1234567890", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "1234567890", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "1234567890", 5, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "1234567890", 9, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "1234567890", 10, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345678901234567890", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345678901234567890", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345678901234567890", 10, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345678901234567890", 19, S("can't happen")));
  BOOST_TEST(testR(S("abcde"), 6, 0, "12345678901234567890", 20, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345", 2, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345", 4, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345", 5, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "1234567890", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "1234567890", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "1234567890", 5, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "1234567890", 9, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "1234567890", 10, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345678901234567890", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345678901234567890", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345678901234567890", 10, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345678901234567890", 19, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghij"), 11, 0, "12345678901234567890", 20, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345", 2, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345", 4, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345", 5, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "1234567890", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "1234567890", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "1234567890", 5, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "1234567890", 9, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "1234567890", 10, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345678901234567890", 0, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345678901234567890", 1, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345678901234567890", 10, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345678901234567890", 19, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 21, 0, "12345678901234567890", 20, S("can't happen")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345678901234567890", 1, S("abcdefghijklmnopqrs1t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345678901234567890", 10, S("abcdefghijklmnopqrs1234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345678901234567890", 19, S("abcdefghijklmnopqrs1234567890123456789t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 0, "12345678901234567890", 20, S("abcdefghijklmnopqrs12345678901234567890t")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "", 0, S("abcdefghijklmnopqrs")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345", 0, S("abcdefghijklmnopqrs")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345", 1, S("abcdefghijklmnopqrs1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345", 2, S("abcdefghijklmnopqrs12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345", 4, S("abcdefghijklmnopqrs1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345", 5, S("abcdefghijklmnopqrs12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "1234567890", 0, S("abcdefghijklmnopqrs")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "1234567890", 1, S("abcdefghijklmnopqrs1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "1234567890", 5, S("abcdefghijklmnopqrs12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "1234567890", 9, S("abcdefghijklmnopqrs123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "1234567890", 10, S("abcdefghijklmnopqrs1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345678901234567890", 0, S("abcdefghijklmnopqrs")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345678901234567890", 1, S("abcdefghijklmnopqrs1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345678901234567890", 10, S("abcdefghijklmnopqrs1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345678901234567890", 19, S("abcdefghijklmnopqrs1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 1, "12345678901234567890", 20, S("abcdefghijklmnopqrs12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "", 0, S("abcdefghijklmnopqrs")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345", 0, S("abcdefghijklmnopqrs")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345", 1, S("abcdefghijklmnopqrs1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345", 2, S("abcdefghijklmnopqrs12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345", 4, S("abcdefghijklmnopqrs1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345", 5, S("abcdefghijklmnopqrs12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "1234567890", 0, S("abcdefghijklmnopqrs")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "1234567890", 1, S("abcdefghijklmnopqrs1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "1234567890", 5, S("abcdefghijklmnopqrs12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "1234567890", 9, S("abcdefghijklmnopqrs123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "1234567890", 10, S("abcdefghijklmnopqrs1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345678901234567890", 0, S("abcdefghijklmnopqrs")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345678901234567890", 1, S("abcdefghijklmnopqrs1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345678901234567890", 10, S("abcdefghijklmnopqrs1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345678901234567890", 19, S("abcdefghijklmnopqrs1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 19, 2, "12345678901234567890", 20, S("abcdefghijklmnopqrs12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345", 1, S("abcdefghijklmnopqrst1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345", 2, S("abcdefghijklmnopqrst12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345", 4, S("abcdefghijklmnopqrst1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345", 5, S("abcdefghijklmnopqrst12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "1234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "1234567890", 1, S("abcdefghijklmnopqrst1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "1234567890", 5, S("abcdefghijklmnopqrst12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "1234567890", 9, S("abcdefghijklmnopqrst123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "1234567890", 10, S("abcdefghijklmnopqrst1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345678901234567890", 1, S("abcdefghijklmnopqrst1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345678901234567890", 10, S("abcdefghijklmnopqrst1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345678901234567890", 19, S("abcdefghijklmnopqrst1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 0, "12345678901234567890", 20, S("abcdefghijklmnopqrst12345678901234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345", 1, S("abcdefghijklmnopqrst1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345", 2, S("abcdefghijklmnopqrst12")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345", 4, S("abcdefghijklmnopqrst1234")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345", 5, S("abcdefghijklmnopqrst12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "1234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "1234567890", 1, S("abcdefghijklmnopqrst1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "1234567890", 5, S("abcdefghijklmnopqrst12345")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "1234567890", 9, S("abcdefghijklmnopqrst123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "1234567890", 10, S("abcdefghijklmnopqrst1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345678901234567890", 0, S("abcdefghijklmnopqrst")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345678901234567890", 1, S("abcdefghijklmnopqrst1")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345678901234567890", 10, S("abcdefghijklmnopqrst1234567890")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345678901234567890", 19, S("abcdefghijklmnopqrst1234567890123456789")));
  BOOST_TEST(testR(S("abcdefghijklmnopqrst"), 20, 1, "12345678901234567890", 20, S("abcdefghijklmnopqrst12345678901234567890")));

  using T = static_string<10>;
  BOOST_TEST_THROWS(T("12345").replace(0, 1, 500, 'a'), std::length_error);
  BOOST_TEST_THROWS(T("12345").replace(0, 1, "aaaaaaaaaaaaaa"), std::length_error);

  // unchecked replacement throwing
  BOOST_TEST_THROWS(S("aaaaa").replace(10, 1, T("bbbbb")), std::out_of_range);
  BOOST_TEST_THROWS(T("aaaaa").replace(0, 1, S("bbbbbbbbbbbbb")), std::length_error);
}

// done
void
testSubstr()
{
  using S = static_string<400>;
  BOOST_TEST(testS(S(""), 0, 0));
  BOOST_TEST(testS(S(""), 1, 0));
  BOOST_TEST(testS(S("pniot"), 0, 0));
  BOOST_TEST(testS(S("htaob"), 0, 1));
  BOOST_TEST(testS(S("fodgq"), 0, 2));
  BOOST_TEST(testS(S("hpqia"), 0, 4));
  BOOST_TEST(testS(S("qanej"), 0, 5));
  BOOST_TEST(testS(S("dfkap"), 1, 0));
  BOOST_TEST(testS(S("clbao"), 1, 1));
  BOOST_TEST(testS(S("ihqrf"), 1, 2));
  BOOST_TEST(testS(S("mekdn"), 1, 3));
  BOOST_TEST(testS(S("ngtjf"), 1, 4));
  BOOST_TEST(testS(S("srdfq"), 2, 0));
  BOOST_TEST(testS(S("qkdrs"), 2, 1));
  BOOST_TEST(testS(S("ikcrq"), 2, 2));
  BOOST_TEST(testS(S("cdaih"), 2, 3));
  BOOST_TEST(testS(S("dmajb"), 4, 0));
  BOOST_TEST(testS(S("karth"), 4, 1));
  BOOST_TEST(testS(S("lhcdo"), 5, 0));
  BOOST_TEST(testS(S("acbsj"), 6, 0));
  BOOST_TEST(testS(S("pbsjikaole"), 0, 0));
  BOOST_TEST(testS(S("pcbahntsje"), 0, 1));
  BOOST_TEST(testS(S("mprdjbeiak"), 0, 5));
  BOOST_TEST(testS(S("fhepcrntko"), 0, 9));
  BOOST_TEST(testS(S("eqmpaidtls"), 0, 10));
  BOOST_TEST(testS(S("joidhalcmq"), 1, 0));
  BOOST_TEST(testS(S("omigsphflj"), 1, 1));
  BOOST_TEST(testS(S("kocgbphfji"), 1, 4));
  BOOST_TEST(testS(S("onmjekafbi"), 1, 8));
  BOOST_TEST(testS(S("fbslrjiqkm"), 1, 9));
  BOOST_TEST(testS(S("oqmrjahnkg"), 5, 0));
  BOOST_TEST(testS(S("jeidpcmalh"), 5, 1));
  BOOST_TEST(testS(S("schfalibje"), 5, 2));
  BOOST_TEST(testS(S("crliponbqe"), 5, 4));
  BOOST_TEST(testS(S("igdscopqtm"), 5, 5));
  BOOST_TEST(testS(S("qngpdkimlc"), 9, 0));
  BOOST_TEST(testS(S("thdjgafrlb"), 9, 1));
  BOOST_TEST(testS(S("hcjitbfapl"), 10, 0));
  BOOST_TEST(testS(S("mgojkldsqh"), 11, 0));
  BOOST_TEST(testS(S("gfshlcmdjreqipbontak"), 0, 0));
  BOOST_TEST(testS(S("nadkhpfemgclosibtjrq"), 0, 1));
  BOOST_TEST(testS(S("nkodajteqplrbifhmcgs"), 0, 10));
  BOOST_TEST(testS(S("ofdrqmkeblthacpgijsn"), 0, 19));
  BOOST_TEST(testS(S("gbmetiprqdoasckjfhln"), 0, 20));
  BOOST_TEST(testS(S("bdfjqgatlksriohemnpc"), 1, 0));
  BOOST_TEST(testS(S("crnklpmegdqfiashtojb"), 1, 1));
  BOOST_TEST(testS(S("ejqcnahdrkfsmptilgbo"), 1, 9));
  BOOST_TEST(testS(S("jsbtafedocnirgpmkhql"), 1, 18));
  BOOST_TEST(testS(S("prqgnlbaejsmkhdctoif"), 1, 19));
  BOOST_TEST(testS(S("qnmodrtkebhpasifgcjl"), 10, 0));
  BOOST_TEST(testS(S("pejafmnokrqhtisbcdgl"), 10, 1));
  BOOST_TEST(testS(S("cpebqsfmnjdolhkratgi"), 10, 5));
  BOOST_TEST(testS(S("odnqkgijrhabfmcestlp"), 10, 9));
  BOOST_TEST(testS(S("lmofqdhpkibagnrcjste"), 10, 10));
  BOOST_TEST(testS(S("lgjqketopbfahrmnsicd"), 19, 0));
  BOOST_TEST(testS(S("ktsrmnqagdecfhijpobl"), 19, 1));
  BOOST_TEST(testS(S("lsaijeqhtrbgcdmpfkno"), 20, 0));
  BOOST_TEST(testS(S("dplqartnfgejichmoskb"), 21, 0));
  BOOST_TEST(testS(S(""), 0, 0));
  BOOST_TEST(testS(S(""), 1, 0));
  BOOST_TEST(testS(S("pniot"), 0, 0));
  BOOST_TEST(testS(S("htaob"), 0, 1));
  BOOST_TEST(testS(S("fodgq"), 0, 2));
  BOOST_TEST(testS(S("hpqia"), 0, 4));
  BOOST_TEST(testS(S("qanej"), 0, 5));
  BOOST_TEST(testS(S("dfkap"), 1, 0));
  BOOST_TEST(testS(S("clbao"), 1, 1));
  BOOST_TEST(testS(S("ihqrf"), 1, 2));
  BOOST_TEST(testS(S("mekdn"), 1, 3));
  BOOST_TEST(testS(S("ngtjf"), 1, 4));
  BOOST_TEST(testS(S("srdfq"), 2, 0));
  BOOST_TEST(testS(S("qkdrs"), 2, 1));
  BOOST_TEST(testS(S("ikcrq"), 2, 2));
  BOOST_TEST(testS(S("cdaih"), 2, 3));
  BOOST_TEST(testS(S("dmajb"), 4, 0));
  BOOST_TEST(testS(S("karth"), 4, 1));
  BOOST_TEST(testS(S("lhcdo"), 5, 0));
  BOOST_TEST(testS(S("acbsj"), 6, 0));
  BOOST_TEST(testS(S("pbsjikaole"), 0, 0));
  BOOST_TEST(testS(S("pcbahntsje"), 0, 1));
  BOOST_TEST(testS(S("mprdjbeiak"), 0, 5));
  BOOST_TEST(testS(S("fhepcrntko"), 0, 9));
  BOOST_TEST(testS(S("eqmpaidtls"), 0, 10));
  BOOST_TEST(testS(S("joidhalcmq"), 1, 0));
  BOOST_TEST(testS(S("omigsphflj"), 1, 1));
  BOOST_TEST(testS(S("kocgbphfji"), 1, 4));
  BOOST_TEST(testS(S("onmjekafbi"), 1, 8));
  BOOST_TEST(testS(S("fbslrjiqkm"), 1, 9));
  BOOST_TEST(testS(S("oqmrjahnkg"), 5, 0));
  BOOST_TEST(testS(S("jeidpcmalh"), 5, 1));
  BOOST_TEST(testS(S("schfalibje"), 5, 2));
  BOOST_TEST(testS(S("crliponbqe"), 5, 4));
  BOOST_TEST(testS(S("igdscopqtm"), 5, 5));
  BOOST_TEST(testS(S("qngpdkimlc"), 9, 0));
  BOOST_TEST(testS(S("thdjgafrlb"), 9, 1));
  BOOST_TEST(testS(S("hcjitbfapl"), 10, 0));
  BOOST_TEST(testS(S("mgojkldsqh"), 11, 0));
  BOOST_TEST(testS(S("gfshlcmdjreqipbontak"), 0, 0));
  BOOST_TEST(testS(S("nadkhpfemgclosibtjrq"), 0, 1));
  BOOST_TEST(testS(S("nkodajteqplrbifhmcgs"), 0, 10));
  BOOST_TEST(testS(S("ofdrqmkeblthacpgijsn"), 0, 19));
  BOOST_TEST(testS(S("gbmetiprqdoasckjfhln"), 0, 20));
  BOOST_TEST(testS(S("bdfjqgatlksriohemnpc"), 1, 0));
  BOOST_TEST(testS(S("crnklpmegdqfiashtojb"), 1, 1));
  BOOST_TEST(testS(S("ejqcnahdrkfsmptilgbo"), 1, 9));
  BOOST_TEST(testS(S("jsbtafedocnirgpmkhql"), 1, 18));
  BOOST_TEST(testS(S("prqgnlbaejsmkhdctoif"), 1, 19));
  BOOST_TEST(testS(S("qnmodrtkebhpasifgcjl"), 10, 0));
  BOOST_TEST(testS(S("pejafmnokrqhtisbcdgl"), 10, 1));
  BOOST_TEST(testS(S("cpebqsfmnjdolhkratgi"), 10, 5));
  BOOST_TEST(testS(S("odnqkgijrhabfmcestlp"), 10, 9));
  BOOST_TEST(testS(S("lmofqdhpkibagnrcjste"), 10, 10));
  BOOST_TEST(testS(S("lgjqketopbfahrmnsicd"), 19, 0));
  BOOST_TEST(testS(S("ktsrmnqagdecfhijpobl"), 19, 1));
  BOOST_TEST(testS(S("lsaijeqhtrbgcdmpfkno"), 20, 0));
  BOOST_TEST(testS(S("dplqartnfgejichmoskb"), 21, 0));
}

// done
void
testSubview()
{
  using S = static_string<400>;
  BOOST_TEST(testSV(S(""), 0, 0));
  BOOST_TEST(testSV(S(""), 1, 0));
  BOOST_TEST(testSV(S("pniot"), 0, 0));
  BOOST_TEST(testSV(S("htaob"), 0, 1));
  BOOST_TEST(testSV(S("fodgq"), 0, 2));
  BOOST_TEST(testSV(S("hpqia"), 0, 4));
  BOOST_TEST(testSV(S("qanej"), 0, 5));
  BOOST_TEST(testSV(S("dfkap"), 1, 0));
  BOOST_TEST(testSV(S("clbao"), 1, 1));
  BOOST_TEST(testSV(S("ihqrf"), 1, 2));
  BOOST_TEST(testSV(S("mekdn"), 1, 3));
  BOOST_TEST(testSV(S("ngtjf"), 1, 4));
  BOOST_TEST(testSV(S("srdfq"), 2, 0));
  BOOST_TEST(testSV(S("qkdrs"), 2, 1));
  BOOST_TEST(testSV(S("ikcrq"), 2, 2));
  BOOST_TEST(testSV(S("cdaih"), 2, 3));
  BOOST_TEST(testSV(S("dmajb"), 4, 0));
  BOOST_TEST(testSV(S("karth"), 4, 1));
  BOOST_TEST(testSV(S("lhcdo"), 5, 0));
  BOOST_TEST(testSV(S("acbsj"), 6, 0));
  BOOST_TEST(testSV(S("pbsjikaole"), 0, 0));
  BOOST_TEST(testSV(S("pcbahntsje"), 0, 1));
  BOOST_TEST(testSV(S("mprdjbeiak"), 0, 5));
  BOOST_TEST(testSV(S("fhepcrntko"), 0, 9));
  BOOST_TEST(testSV(S("eqmpaidtls"), 0, 10));
  BOOST_TEST(testSV(S("joidhalcmq"), 1, 0));
  BOOST_TEST(testSV(S("omigsphflj"), 1, 1));
  BOOST_TEST(testSV(S("kocgbphfji"), 1, 4));
  BOOST_TEST(testSV(S("onmjekafbi"), 1, 8));
  BOOST_TEST(testSV(S("fbslrjiqkm"), 1, 9));
  BOOST_TEST(testSV(S("oqmrjahnkg"), 5, 0));
  BOOST_TEST(testSV(S("jeidpcmalh"), 5, 1));
  BOOST_TEST(testSV(S("schfalibje"), 5, 2));
  BOOST_TEST(testSV(S("crliponbqe"), 5, 4));
  BOOST_TEST(testSV(S("igdscopqtm"), 5, 5));
  BOOST_TEST(testSV(S("qngpdkimlc"), 9, 0));
  BOOST_TEST(testSV(S("thdjgafrlb"), 9, 1));
  BOOST_TEST(testSV(S("hcjitbfapl"), 10, 0));
  BOOST_TEST(testSV(S("mgojkldsqh"), 11, 0));
  BOOST_TEST(testSV(S("gfshlcmdjreqipbontak"), 0, 0));
  BOOST_TEST(testSV(S("nadkhpfemgclosibtjrq"), 0, 1));
  BOOST_TEST(testSV(S("nkodajteqplrbifhmcgs"), 0, 10));
  BOOST_TEST(testSV(S("ofdrqmkeblthacpgijsn"), 0, 19));
  BOOST_TEST(testSV(S("gbmetiprqdoasckjfhln"), 0, 20));
  BOOST_TEST(testSV(S("bdfjqgatlksriohemnpc"), 1, 0));
  BOOST_TEST(testSV(S("crnklpmegdqfiashtojb"), 1, 1));
  BOOST_TEST(testSV(S("ejqcnahdrkfsmptilgbo"), 1, 9));
  BOOST_TEST(testSV(S("jsbtafedocnirgpmkhql"), 1, 18));
  BOOST_TEST(testSV(S("prqgnlbaejsmkhdctoif"), 1, 19));
  BOOST_TEST(testSV(S("qnmodrtkebhpasifgcjl"), 10, 0));
  BOOST_TEST(testSV(S("pejafmnokrqhtisbcdgl"), 10, 1));
  BOOST_TEST(testSV(S("cpebqsfmnjdolhkratgi"), 10, 5));
  BOOST_TEST(testSV(S("odnqkgijrhabfmcestlp"), 10, 9));
  BOOST_TEST(testSV(S("lmofqdhpkibagnrcjste"), 10, 10));
  BOOST_TEST(testSV(S("lgjqketopbfahrmnsicd"), 19, 0));
  BOOST_TEST(testSV(S("ktsrmnqagdecfhijpobl"), 19, 1));
  BOOST_TEST(testSV(S("lsaijeqhtrbgcdmpfkno"), 20, 0));
  BOOST_TEST(testSV(S("dplqartnfgejichmoskb"), 21, 0));
  BOOST_TEST(testSV(S(""), 0, 0));
  BOOST_TEST(testSV(S(""), 1, 0));
  BOOST_TEST(testSV(S("pniot"), 0, 0));
  BOOST_TEST(testSV(S("htaob"), 0, 1));
  BOOST_TEST(testSV(S("fodgq"), 0, 2));
  BOOST_TEST(testSV(S("hpqia"), 0, 4));
  BOOST_TEST(testSV(S("qanej"), 0, 5));
  BOOST_TEST(testSV(S("dfkap"), 1, 0));
  BOOST_TEST(testSV(S("clbao"), 1, 1));
  BOOST_TEST(testSV(S("ihqrf"), 1, 2));
  BOOST_TEST(testSV(S("mekdn"), 1, 3));
  BOOST_TEST(testSV(S("ngtjf"), 1, 4));
  BOOST_TEST(testSV(S("srdfq"), 2, 0));
  BOOST_TEST(testSV(S("qkdrs"), 2, 1));
  BOOST_TEST(testSV(S("ikcrq"), 2, 2));
  BOOST_TEST(testSV(S("cdaih"), 2, 3));
  BOOST_TEST(testSV(S("dmajb"), 4, 0));
  BOOST_TEST(testSV(S("karth"), 4, 1));
  BOOST_TEST(testSV(S("lhcdo"), 5, 0));
  BOOST_TEST(testSV(S("acbsj"), 6, 0));
  BOOST_TEST(testSV(S("pbsjikaole"), 0, 0));
  BOOST_TEST(testSV(S("pcbahntsje"), 0, 1));
  BOOST_TEST(testSV(S("mprdjbeiak"), 0, 5));
  BOOST_TEST(testSV(S("fhepcrntko"), 0, 9));
  BOOST_TEST(testSV(S("eqmpaidtls"), 0, 10));
  BOOST_TEST(testSV(S("joidhalcmq"), 1, 0));
  BOOST_TEST(testSV(S("omigsphflj"), 1, 1));
  BOOST_TEST(testSV(S("kocgbphfji"), 1, 4));
  BOOST_TEST(testSV(S("onmjekafbi"), 1, 8));
  BOOST_TEST(testSV(S("fbslrjiqkm"), 1, 9));
  BOOST_TEST(testSV(S("oqmrjahnkg"), 5, 0));
  BOOST_TEST(testSV(S("jeidpcmalh"), 5, 1));
  BOOST_TEST(testSV(S("schfalibje"), 5, 2));
  BOOST_TEST(testSV(S("crliponbqe"), 5, 4));
  BOOST_TEST(testSV(S("igdscopqtm"), 5, 5));
  BOOST_TEST(testSV(S("qngpdkimlc"), 9, 0));
  BOOST_TEST(testSV(S("thdjgafrlb"), 9, 1));
  BOOST_TEST(testSV(S("hcjitbfapl"), 10, 0));
  BOOST_TEST(testSV(S("mgojkldsqh"), 11, 0));
  BOOST_TEST(testSV(S("gfshlcmdjreqipbontak"), 0, 0));
  BOOST_TEST(testSV(S("nadkhpfemgclosibtjrq"), 0, 1));
  BOOST_TEST(testSV(S("nkodajteqplrbifhmcgs"), 0, 10));
  BOOST_TEST(testSV(S("ofdrqmkeblthacpgijsn"), 0, 19));
  BOOST_TEST(testSV(S("gbmetiprqdoasckjfhln"), 0, 20));
  BOOST_TEST(testSV(S("bdfjqgatlksriohemnpc"), 1, 0));
  BOOST_TEST(testSV(S("crnklpmegdqfiashtojb"), 1, 1));
  BOOST_TEST(testSV(S("ejqcnahdrkfsmptilgbo"), 1, 9));
  BOOST_TEST(testSV(S("jsbtafedocnirgpmkhql"), 1, 18));
  BOOST_TEST(testSV(S("prqgnlbaejsmkhdctoif"), 1, 19));
  BOOST_TEST(testSV(S("qnmodrtkebhpasifgcjl"), 10, 0));
  BOOST_TEST(testSV(S("pejafmnokrqhtisbcdgl"), 10, 1));
  BOOST_TEST(testSV(S("cpebqsfmnjdolhkratgi"), 10, 5));
  BOOST_TEST(testSV(S("odnqkgijrhabfmcestlp"), 10, 9));
  BOOST_TEST(testSV(S("lmofqdhpkibagnrcjste"), 10, 10));
  BOOST_TEST(testSV(S("lgjqketopbfahrmnsicd"), 19, 0));
  BOOST_TEST(testSV(S("ktsrmnqagdecfhijpobl"), 19, 1));
  BOOST_TEST(testSV(S("lsaijeqhtrbgcdmpfkno"), 20, 0));
  BOOST_TEST(testSV(S("dplqartnfgejichmoskb"), 21, 0));
}

// done
void
testStartsEnds()
{
  using S = static_string<400>;
  BOOST_TEST(S("1234567890").starts_with('1'));
  BOOST_TEST(S("1234567890").starts_with("123"));
  BOOST_TEST(S("1234567890").starts_with("1234567890"));
  BOOST_TEST(!S("1234567890").starts_with("234"));
  BOOST_TEST(!S("1234567890").starts_with("12345678900"));
  BOOST_TEST(S("1234567890").starts_with(string_view("1234567890")));

  BOOST_TEST(S("1234567890").ends_with('0'));
  BOOST_TEST(S("1234567890").ends_with("890"));
  BOOST_TEST(S("1234567890").ends_with("1234567890"));
  BOOST_TEST(!S("1234567890").ends_with("234"));
  BOOST_TEST(!S("1234567890").ends_with("12345678900"));
  BOOST_TEST(S("1234567890").ends_with(string_view("1234567890")));

  BOOST_TEST(!S().starts_with('0'));
  BOOST_TEST(!S().starts_with("0"));
  BOOST_TEST(!S().starts_with(string_view("0")));
  BOOST_TEST(!S().ends_with('0'));
  BOOST_TEST(!S().ends_with("0"));
  BOOST_TEST(!S().ends_with(string_view("0")));
}

void
testHash()
{
  using U = static_string<30>;
  std::hash<U> hasher;
  BOOST_TEST(hasher(U("1")) != hasher(U("123456789")));
  BOOST_TEST(hasher(U("1234567890")) == hasher(U("1234567890")));
}

void
testEmpty()
{
  static_string<0> a;
  BOOST_TEST(a.size() == 0);
  BOOST_TEST(a.data() != nullptr);
  BOOST_TEST(a.capacity() == 0);
}

void
testResize()
{
  static_string<10> a = "a";
  a.resize(a.size() + 1);
  BOOST_TEST(a.size() == 2);

  static_string<10> b = "a";
  b.resize(b.size() + 1, 'a');
  BOOST_TEST(b == "aa");
  BOOST_TEST(b.size() == 2);
}

void
testStream()
{
  std::stringstream a;
  static_string<10> b = "abcdefghij";
  a << b;
  static_string<10> c(std::istream_iterator<char>{a}, std::istream_iterator<char>{});
  BOOST_TEST(a.str() == b.subview());
  BOOST_TEST(b == c);
}

void
testOperatorPlus()
{
  static_string<10> s1 = "hello";
  static_string<10> s2 = "world";

  // operator+(static_string, static_string)
  {
    auto res = s1 + s2;
    BOOST_TEST(res == "helloworld");
    BOOST_TEST(res.capacity() == 20);
    BOOST_TEST(res.size() == 10);
  }

  // operator+(static_string, CharT)
  {
    auto res = s1 + '!';
    BOOST_TEST(res == "hello!");
    BOOST_TEST(res.capacity() == 11);
    BOOST_TEST(res.size() == 6);
  }

  // operator+(CharT, static_string)
  {
    auto res = '!' + s1;
    BOOST_TEST(res == "!hello");
    BOOST_TEST(res.capacity() == 11);
    BOOST_TEST(res.size() == 6);
  }

  // operator+(static_string, CharT(&)[N])
  {
    auto res = s1 + "world";
    BOOST_TEST(res == "helloworld");
    BOOST_TEST(res.capacity() == 16);
    BOOST_TEST(res.size() == 10);
  }

  // operator+(CharT(&)[N], static_string)
  {
    auto res = "hello" + s2;
    BOOST_TEST(res == "helloworld");
    BOOST_TEST(res.capacity() == 16);
    BOOST_TEST(res.size() == 10);
  }

  // operator+(static_string, CharT(&)[N]), no null
  {
    char arr[10] = {'w', 'o', 'r', 'l', 'd'};
    auto res = s1 + arr;
    BOOST_TEST(res == "helloworld");
    BOOST_TEST(res.capacity() == 20);
    BOOST_TEST(res.size() == 10);
  }

  // operator+(CharT(&)[N], static_string), no null
  {
    char arr[10] = {'h', 'e', 'l', 'l', 'o'};
    auto res = arr + s2;
    BOOST_TEST(res == "helloworld");
    BOOST_TEST(res.capacity() == 20);
    BOOST_TEST(res.size() == 10);
  }
}

int
runTests()
{
  constexpr auto cxper = testConstantEvaluation();
  static_cast<void>(cxper);

  testConstruct();
    
  testAssignment();
    
  testElements();

  testIterators();
    
  testCapacity();

  testClear();
  testInsert();
  testErase();
  testEraseIf();
  testPushBack();
  testPopBack();
  testAppend();
  testPlusEquals();

  testCompare();
  testSwap();
  testGeneral();
  testToStaticString();
  testResize();

  testFind();

  testReplace();
  testSubstr();
  testStartsEnds();

  testHash();
  testEmpty();
  testStream();
  testOperatorPlus();

  return report_errors();
}
} // static_string
} // boost

int
main()
{
  return boost::static_strings::runTests();
}