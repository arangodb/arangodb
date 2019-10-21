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
  bool waiting = std::find(processes.begin(), processes.end(), comm.rank()) != processes.end();
  for (int r = 0; r < comm.size(); ++r) {
    if (comm.rank() == r) {
      std::cout << "Rank " << comm.rank() << " has PID " << getpid();
      if (waiting) {
        std::cout << " and is waiting.";
      }
      std::cout << std::endl;
    }
    comm.barrier();
  }
  if (std::find(processes.begin(), processes.end(), comm.rank()) != processes.end()) {
    while (i != 0) {
      sleep(5);
    }
  }
  std::cout << "Rank " << comm.rank() << " will proceed.\n";
}

void wait_for_debugger(boost::mpi::communicator const& comm)
{
  std::vector<int> all;
  for (int r = 0; r < comm.size(); ++r) {
    all.push_back(r);
  }
  wait_for_debugger(all, comm);
}

