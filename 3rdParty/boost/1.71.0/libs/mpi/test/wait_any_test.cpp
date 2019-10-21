// Copyright (C) 2017 Alain Miniussi & Steffen Hirschmann 

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <set>

#include <boost/mpi.hpp>
#include <boost/mpi/nonblocking.hpp>
#include <boost/serialization/string.hpp>

#define BOOST_TEST_MODULE mpi_wait_any
#include <boost/test/included/unit_test.hpp>

namespace mpi = boost::mpi;

BOOST_AUTO_TEST_CASE(wait_any)
{
  mpi::environment  env;
  mpi::communicator world;
  
  std::vector<std::string> ss(world.size());
  typedef std::vector<mpi::request> requests;
  requests rreqs;
  
  std::set<int> pending_senders;
  for (int i = 0; i < world.size(); ++i) {
    rreqs.push_back(world.irecv(i, i, ss[i]));
    pending_senders.insert(i);
  }
  
  std::ostringstream fmt;
  std::string msg = "Hello, World! this is ";
  fmt << msg << world.rank();

  requests sreqs;
  for (int i = 0; i < world.size(); ++i) {
    sreqs.push_back(world.isend(i, world.rank(), fmt.str()));
  }
  
  for (int i = 0; i < world.size(); ++i) {
    std::pair<mpi::status, requests::iterator> completed = mpi::wait_any(rreqs.begin(), rreqs.end());
    std::ostringstream out;
    out << "Proc " << world.rank() << " got message from " << completed.first.source() << '\n';
    std::cout << out.str();
  }
  
  for (int i = 0; i < world.size(); ++i) {
    std::ostringstream fmt;
    fmt << msg << i;
    std::vector<std::string>::iterator found = std::find(ss.begin(), ss.end(), fmt.str());
    BOOST_CHECK(found != ss.end());
    fmt.str("");
    fmt << "Proc " << world.rank() << " Got msg from " << i << '\n';
    std::cout << fmt.str();
  }

  mpi::wait_all(sreqs.begin(), sreqs.end());
}
