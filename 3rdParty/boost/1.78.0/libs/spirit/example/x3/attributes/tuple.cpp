/*=============================================================================
    Copyright (c) 2019 Tom Tan

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/home/x3.hpp>

#include <boost/fusion/adapted/std_tuple.hpp>

#include <iostream>
#include <tuple>
#include <string>

//
// X3 does not support more than one attribute anymore in the parse function,
// this example show how to wrap multiple attributes into one leveraging std::tuple.
//

std::tuple<uint32_t, uint32_t, uint32_t> parse_message_prefix_revision(const std::string &s)
{

  namespace x3 = boost::spirit::x3;

  auto const uint_3_digits = x3::uint_parser<std::uint32_t, 10, 3, 3>{};
  auto const uint_4_digits = x3::uint_parser<std::uint32_t, 10, 4, 4>{};

  auto iter = s.cbegin();
  auto end_iter = s.cend();

  std::tuple<uint32_t, uint32_t, uint32_t> length_id_revision;

  x3::parse(iter, end_iter,
            uint_4_digits >> uint_4_digits >> uint_3_digits,
            length_id_revision);

  return length_id_revision;
}

int main()
{
  std::string s = "00200060001";

  std::cout << "parsing " << s << '\n';
  auto [len, id, rev] = parse_message_prefix_revision(s);
  std::cout << "length = " << len << '\n'
            << "id = " << id << '\n'
            << "revision =" << rev;
}
