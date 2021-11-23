// Copyright (C) 2005-2006 Douglas Gregor <doug.gregor@gmail.com>

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// A test of the is_mpi_op functionality.
#include <boost/mpi/operations.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/type_traits/is_base_and_derived.hpp>

#define BOOST_TEST_MODULE mpi_is_mpi_op_test
#include <boost/test/included/unit_test.hpp>

using namespace boost::mpi;
using namespace std;
using boost::is_base_and_derived;

BOOST_AUTO_TEST_CASE(mpi_basic_op)
{
  boost::mpi::environment env;

  // Check each predefined MPI_Op type that we support directly.
  BOOST_TEST((is_mpi_op<minimum<float>, float>::op() == MPI_MIN));
  BOOST_TEST((is_mpi_op<plus<double>, double>::op() == MPI_SUM));
  BOOST_TEST((is_mpi_op<multiplies<long>, long>::op() == MPI_PROD));
  BOOST_TEST((is_mpi_op<logical_and<int>, int>::op() == MPI_LAND));
  BOOST_TEST((is_mpi_op<bitwise_and<int>, int>::op() == MPI_BAND));
  BOOST_TEST((is_mpi_op<logical_or<int>, int>::op() == MPI_LOR));
  BOOST_TEST((is_mpi_op<bitwise_or<int>, int>::op() == MPI_BOR));
  BOOST_TEST((is_mpi_op<logical_xor<int>, int>::op() == MPI_LXOR));
  BOOST_TEST((is_mpi_op<bitwise_xor<int>, int>::op() == MPI_BXOR));
}
