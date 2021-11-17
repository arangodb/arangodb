// Copyright (C) 2020 Steffen Hirschmann

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpi.hpp>
#include <boost/mpi/nonblocking.hpp>
#include <vector>


int main()
{
  boost::mpi::environment env;
  std::vector<boost::mpi::request> req(1);
  boost::mpi::wait_all(req.begin(), req.end());
}
