// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/compressed_pair.hpp>
#include "std_ostream.hpp"

using namespace boost::histogram::detail;

struct nothrow_move {
  nothrow_move(nothrow_move&&) noexcept {}
  nothrow_move& operator=(nothrow_move&&) noexcept { return *this; }
};

struct throw_move {
  throw_move(throw_move&&) {}
  throw_move& operator=(throw_move&&) { return *this; }
};

int main() {
  BOOST_TEST_TRAIT_TRUE(
      (std::is_nothrow_move_constructible<compressed_pair<nothrow_move, nothrow_move> >));

  BOOST_TEST_TRAIT_FALSE(
      (std::is_nothrow_move_constructible<compressed_pair<nothrow_move, throw_move> >));

  BOOST_TEST_TRAIT_FALSE(
      (std::is_nothrow_move_constructible<compressed_pair<throw_move, nothrow_move> >));

  BOOST_TEST_TRAIT_FALSE(
      (std::is_nothrow_move_constructible<compressed_pair<throw_move, throw_move> >));

  BOOST_TEST_GE(sizeof(compressed_pair<int, char>), sizeof(int) + sizeof(char));
  BOOST_TEST_EQ(sizeof(compressed_pair<int, nothrow_move>), sizeof(int));
  BOOST_TEST_EQ(sizeof(compressed_pair<int, throw_move>), sizeof(int));

  return boost::report_errors();
}
