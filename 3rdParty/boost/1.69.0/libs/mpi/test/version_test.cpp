// Copyright (C) 2013 Alain Miniussi <alain.miniussi@oca.eu>

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test mpi version

#include <boost/mpi/environment.hpp>
#include <boost/test/minimal.hpp>
#include <iostream>

namespace mpi = boost::mpi;

int
test_main(int argc, char* argv[]) {
#if defined(MPI_VERSION)
  int mpi_version    = MPI_VERSION;
  int mpi_subversion = MPI_SUBVERSION;
#else
  int mpi_version = 0;
  int mpi_subversion = 0;
#endif

  mpi::environment env(argc,argv);
  std::pair<int,int> version = env.version();
  std::cout << "MPI Version: " << version.first << ',' << version.second << '\n';

  BOOST_CHECK(version.first == mpi_version);
  BOOST_CHECK(version.second == mpi_subversion);
  return 0;
}
