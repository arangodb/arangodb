//          Copyright AlainMiniussi 20014 - 20015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>

#include "debugger.hpp"

std::vector<int> extract_paused_ranks(int argc, char** argv) {
  std::vector<int> paused;
  for (int i=1; i < argc; ++i) {
    paused.push_back(std::atoi(argv[i]));
  }
  return paused;
}

void wait_for_debugger(std::vector<int> const& processes, boost::mpi::communicator const& comm)
{
  int i = 1;
  for (int r = 0; r < comm.size(); ++r) {
    if (comm.rank() == r) {
      std::cout << "Rank " << comm.rank() << " has PID " << getpid() << '\n';
    }
    comm.barrier();
  }
  sleep(1);
  if (std::find(processes.begin(), processes.end(), comm.rank()) != processes.end()) {
    while (i!=0) {
      sleep(2);
    }
  }
}

