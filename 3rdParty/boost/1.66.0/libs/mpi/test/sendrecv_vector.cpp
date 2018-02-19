// Author: K. Noel Belcourt <kbelco -at- sandia.gov>

// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if defined(__cplusplus) && (201103L <= __cplusplus)

#include <array>
#include <cassert>
#include <vector>

#include "boost/mpi/environment.hpp"
#include "boost/mpi/communicator.hpp"

using std::array;
using std::vector;

namespace mpi = boost::mpi;

struct blob : array<int, 9>, array<double, 3>, array<char, 8> {
};

namespace boost {
  namespace mpi {

    template <>
    struct is_mpi_datatype<blob> : mpl::true_ {
    };

    template <>
    MPI_Datatype get_mpi_datatype<blob>(const blob& b) {
      array<unsigned long, 3> block_lengths{
        { 9, 3, 8 }
      };

      array<MPI_Aint, 3> displacements{
        { 0, 40, 64 }
      };

      array<MPI_Datatype, 3> datatypes{
        { MPI_INT, MPI_DOUBLE, MPI_CHAR }
      };

      MPI_Datatype blob_type;
      MPI_Type_create_struct(
          block_lengths.size()
        , reinterpret_cast<int*>(block_lengths.data())
        , displacements.data()
        , datatypes.data()
        , &blob_type);

      MPI_Type_commit(&blob_type);
      return blob_type;
    }

  } // namespace mpi
} // namespace boost

#endif // defined(__cplusplus)


int main(int argc, char* argv[]) {
#if defined(__cplusplus) && (201103L <= __cplusplus)

  mpi::environment env(argc, argv);
  mpi::communicator world;

  vector<blob> data;

  if (world.rank() == 0) {
    int size = 10000000;
    data.resize(size);
    // initialize data at vector ends
    blob& b1= data[0];
    array<int, 9>& i = b1;
    i[0] = -1;
    blob& b2= data[size-1];
    array<int, 9>& d = b2;
    d[2] = -17;
    world.send(1, 0, data);
  } 
  else {
    world.recv(0, 0, data);
    // check data at vector ends
    blob& b1 = data[0];
    array<int, 9>& i = b1;
    assert(i[0] == -1);
    // blob& b2 = data[data.size()-1];
    // array<int, 9>& d = b2;
    // assert(d[2] == -17);
  }
#endif // defined(__cplusplus)

  return 0;
}
