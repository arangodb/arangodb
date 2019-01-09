// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <example_handcrafted.hpp>

#include <iostream>

int main()
{
  using boost::mpl::int_;

  std::cout
    << "fib 6 = " << fib::apply<int_<6> >::type::value << std::endl
    << "fact 4 = " << fact::apply<int_<4> >::type::value << std::endl
    << "times4 3 = " << times4::apply<int_<3> >::type::value << std::endl
    << "times11 2 = " << times11::apply<int_<2> >::type::value << std::endl;
}

