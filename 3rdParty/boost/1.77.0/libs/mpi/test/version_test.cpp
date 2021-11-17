// Copyright (C) 2013 Alain Miniussi <alain.miniussi@oca.eu>

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test mpi version

#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <iostream>

#define BOOST_TEST_MODULE mpi_version
#include <boost/test/included/unit_test.hpp>

namespace mpi = boost::mpi;

void
test_version(mpi::communicator const& comm) {
#if defined(MPI_VERSION)
  int mpi_version    = MPI_VERSION;
  int mpi_subversion = MPI_SUBVERSION;
#else
  int mpi_version = 0;
  int mpi_subversion = 0;
#endif
  
  std::pair<int,int> version = mpi::environment::version();
  if (comm.rank() == 0) {
    std::cout << "MPI Version: " << version.first << ',' << version.second << '\n';
  }
  BOOST_CHECK(version.first == mpi_version);
  BOOST_CHECK(version.second == mpi_subversion);
}

std::string
yesno(bool b) {
  return b ? std::string("yes") : std::string("no");
}

void
report_features(mpi::communicator const& comm) {
  if (comm.rank() == 0) {
    std::cout << "Assuming working MPI_Improbe:" <<
#if defined(BOOST_MPI_USE_IMPROBE)
      "yes" << '\n';
#else
      "no"  << '\n';
#endif
  }
}

BOOST_AUTO_TEST_CASE(version)
{
  mpi::environment env;
  mpi::communicator world;

  test_version(world);
  report_features(world);
}
