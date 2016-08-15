// Copyright (C) 2005, 2006 Douglas Gregor.

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// A test of the broadcast() collective.
#include <boost/mpi/collectives/broadcast.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/test/minimal.hpp>

#include <algorithm>
#include <vector>
#include <map>

#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

namespace mpi = boost::mpi;

typedef std::vector<std::map<int, double> > sparse;

template<typename T>
void
broadcast_test(const mpi::communicator& comm, const T& bc_value,
               std::string const& kind, int root) {
  using boost::mpi::broadcast;
  
  T value;
  if (comm.rank() == root) {
    value = bc_value;
    std::cout << "Broadcasting " << kind << " from root " << root << "...";
    std::cout.flush();
  }
  
  
  broadcast(comm, value, root);
  BOOST_CHECK(value == bc_value);
  if (comm.rank() == root) {
    if (value == bc_value) {
      std::cout << "OK." << std::endl;
    } else {
      std::cout << "FAIL." << std::endl;
    }
  }
  comm.barrier();
}

template<typename T>
void
broadcast_test(const mpi::communicator& comm, const T& bc_value,
               std::string const& kind)
{
  for (int root = 0; root < comm.size(); ++root) {
    broadcast_test(comm, bc_value, kind, root);
  }
}

int test_main(int argc, char* argv[])
{
  boost::mpi::environment env(argc, argv);

  mpi::communicator comm;
  if (comm.size() == 1) {
    std::cerr << "ERROR: Must run the broadcast test with more than one "
              << "process." << std::endl;
    comm.abort(-1);
  }
  
  sparse s;
  s.resize(2);
  s[0][12] = 0.12;
  s[1][13] = 1.13;
  broadcast_test(comm, s, "sparse");
  return 0;
}
