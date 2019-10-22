// Copyright (C) 2005, 2006 Douglas Gregor.

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// A test of the broadcast() collective.
#include <algorithm>
#include <vector>
#include <map>

#include <boost/mpi/collectives/broadcast.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/environment.hpp>

#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

#define BOOST_TEST_MODULE mpi_broadcast_stl
#include <boost/test/included/unit_test.hpp>

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

BOOST_AUTO_TEST_CASE(broadcast_stl)
{
  boost::mpi::environment env;

  mpi::communicator comm;
  BOOST_TEST_REQUIRE(comm.size() > 1);
  
  sparse s;
  s.resize(2);
  s[0][12] = 0.12;
  s[1][13] = 1.13;
  broadcast_test(comm, s, "sparse");
}
