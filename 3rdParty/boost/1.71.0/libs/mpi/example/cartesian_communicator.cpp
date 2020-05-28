//          Copyright Alain Miniussi 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// Authors: Alain Miniussi

#include <vector>
#include <iostream>

#include <boost/mpi/communicator.hpp>
#include <boost/mpi/collectives.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/cartesian_communicator.hpp>

namespace mpi = boost::mpi;
// Curly brace init make this useless, but
//  - Need to support obsolete like g++ 4.3.x. for some reason
//  - Can't conditionnaly compile with bjam (unless you find 
//  the doc, and read it, which would only make sense if you
//  actually wan't to use bjam, which does not (make sense))
typedef mpi::cartesian_dimension cd;

int main(int argc, char* argv[])
{
  mpi::environment  env;
  mpi::communicator world;
  
  if (world.size() != 24)  return -1;
  mpi::cartesian_dimension dims[] = {cd(2, true), cd(3,true), cd(4,true)};
  mpi::cartesian_communicator cart(world, mpi::cartesian_topology(dims));
  for (int r = 0; r < cart.size(); ++r) {
    cart.barrier();
    if (r == cart.rank()) {
      std::vector<int> c = cart.coordinates(r);
      std::cout << "rk :" << r << " coords: " 
                << c[0] << ' ' << c[1] << ' ' << c[2] << '\n';
    }
  }
  return 0;
}

