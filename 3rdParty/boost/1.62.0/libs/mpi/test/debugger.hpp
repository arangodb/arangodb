//          Copyright AlainMiniussi 20014 - 20015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <vector>
#include "boost/mpi/communicator.hpp"

/**
 * @brief Extract the MPI rank to pause.
 *
 * Right now, just atois alla the parameters in argv....
 */
std::vector<int> extract_paused_ranks(int argc, char** argv);

/**
 * @print Print rank pid map and wait if requested.
 * @param processes Wait if our rank is in there.
 * @param comm The communicator to consider.
 *
 * Once the debugger has attached to the process, it is expected to 
 * set the local variable 'i' to 0 to let the process restarts.
 */
void wait_for_debugger(std::vector<int> const& processes, boost::mpi::communicator const& comm);
