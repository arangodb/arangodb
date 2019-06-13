#include <vector>
#include <iostream>
#include <iterator>
#include <boost/mpi.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/test/minimal.hpp>

namespace mpi = boost::mpi;

template<typename T>
bool test(mpi::communicator const& comm, std::vector<T> const& ref, bool iswap, bool alloc)
{
  int rank = comm.rank();
  if (rank == 0) {
    if (iswap) {
      std::cout << "Blockin send, non blocking receive.\n";
    } else {
      std::cout << "Non blockin send, blocking receive.\n";
    }
    if (alloc) {
      std::cout << "Explicitly allocate space for the receiver.\n";
    } else {
      std::cout << "Do not explicitly allocate space for the receiver.\n";
    }
  }
  if (rank == 0) {
    std::vector<T> data;
    if (alloc) {
      data.resize(ref.size());
    }
    if (iswap) {
      mpi::request req = comm.irecv(1, 0, data);
      req.wait();
    } else {
      comm.recv(1, 0, data);
    }
    std::cout << "Process 0 received " << data.size() << " elements :" << std::endl;
    std::copy(data.begin(), data.end(), std::ostream_iterator<T>(std::cout, " "));
    std::cout << std::endl;
    std::cout << "While expecting " << ref.size() << " elements :" << std::endl;
    std::copy(ref.begin(),  ref.end(),  std::ostream_iterator<T>(std::cout, " "));
    std::cout << std::endl;
    return (data == ref);
  } else {
    if (rank == 1) {
      std::vector<T> vec = ref;
      if (iswap) {
        comm.send(0, 0, vec);
      } else {
        mpi::request req = comm.isend(0, 0, vec);
        req.wait();
      }
    } 
    return true;
  }
}

int test_main(int argc, char **argv)
{
  mpi::environment env(argc, argv);
  mpi::communicator world;

  std::vector<int> integers(13); // don't assume we're lucky
  for(int i = 0; i < int(integers.size()); ++i) {
    integers[i] = i;
  }

  std::vector<std::string> strings(13); // don't assume we're lucky
  for(int i = 0; i < int(strings.size()); ++i) {
    std::ostringstream fmt;
    fmt << "S" << i;
    strings[i] = fmt.str();
  }
  
  bool block_to_non_block = true;
  bool non_block_to_block = true;
  if (argc == 2) {
    if (std::string(argv[1]) == "b2nb") {
      non_block_to_block = false;
    } else if (std::string(argv[1]) == "nb2b") {
      block_to_non_block = false;
    } else {
      if (world.rank() == 0) {
        std::cerr << "Usage: " << argv[0] << " [<n2nb|nb2b]\n";
      }
      return -1;
    }
  }
  bool passed = true;
  if (block_to_non_block) {
    passed = passed && test(world, integers, true,  true);
    passed = passed && test(world, integers, true,  false);
    passed = passed && test(world, strings, true,  true);
    passed = passed && test(world, strings, true,  false);
  }
  if (non_block_to_block) {
    passed = passed && test(world, integers, false,  true);
    passed = passed && test(world, integers, false,  false);
    passed = passed && test(world, strings, false,  true);
    passed = passed && test(world, strings, false,  false);
  }
  passed = mpi::all_reduce(world, passed, std::logical_and<bool>());
  return passed ? 0 : 1;
}
