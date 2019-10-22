// Copyright (C) 2013 Alain Miniussi <alain.miniussi@oca.eu>

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test threading::level operations

#include <boost/mpi.hpp>
#include <iostream>
#include <sstream>

#define BOOST_TEST_MODULE mpi_mt_init
#include <boost/test/included/unit_test.hpp>

namespace mpi = boost::mpi;

void
test_mt_init(std::string s)
{
  mpi::threading::level required = mpi::threading::level(-1);
  std::istringstream in(s);
  in >> required;
  BOOST_CHECK(!in.bad());
  mpi::environment env;
  BOOST_CHECK(env.thread_level() >= mpi::threading::single);
  BOOST_CHECK(env.thread_level() <= mpi::threading::multiple);
}

BOOST_AUTO_TEST_CASE(mt_init)
{
  mpi::environment env;
  mpi::communicator comm;
  test_mt_init("single");
  test_mt_init("funneled");
  test_mt_init("serialized");
  test_mt_init("multiple");
}
