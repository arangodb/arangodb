// Copyright (c) 2019 Ilya Kiselev
// Copyright (c) 2019-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/detail/offset_based_getter.hpp>
#include <boost/pfr/detail/sequence_tuple.hpp>

#include <boost/core/lightweight_test.hpp>

struct user_type {
    char c;
    double d;
};

int main() {
   using pfr_tuple = boost::pfr::detail::sequence_tuple::tuple<char, double>;
   using getter = boost::pfr::detail::offset_based_getter<user_type, pfr_tuple>;
   using boost::pfr::detail::size_t_;
   using boost::pfr::detail::sequence_tuple::get;

   user_type value{};
   auto begin = reinterpret_cast<char*>(&value);
   auto native_offset = reinterpret_cast<char*>(&value.d) - begin;

   auto getter_offset = reinterpret_cast<char*>(&getter{}.get(value, size_t_<1>{})) - begin;
   BOOST_TEST_EQ(native_offset, getter_offset);

   pfr_tuple pfr_value{};
   auto pfr_tuple_offset = (
      reinterpret_cast<char*>(&get<1>(pfr_value)) - reinterpret_cast<char*>(&get<0>(pfr_value))
   );
   BOOST_TEST_EQ(native_offset, pfr_tuple_offset);

   return boost::report_errors();
}
