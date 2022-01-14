// Copyright (C) 2005, 2006 Douglas Gregor.

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// A test of the scatter() and scatterv() collectives.
#include <iterator>
#include <boost/mpi/collectives/scatter.hpp>
#include <boost/mpi/collectives/scatterv.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/environment.hpp>
#include "gps_position.hpp"
#include <boost/serialization/string.hpp>
#include <boost/serialization/list.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/lexical_cast.hpp>

#define BOOST_TEST_MODULE mpi_scatter
#include <boost/test/included/unit_test.hpp>

using namespace boost::mpi;

template<typename Generator>
void
scatter_test(const communicator& comm, Generator generator,
             const char* kind, int root = -1)
{
  typedef typename Generator::result_type value_type;

  if (root == -1) {
    for (root = 0; root < comm.size(); ++root)
      scatter_test(comm, generator, kind, root);
  } else {
    using boost::mpi::scatter;

    value_type value;

    if (comm.rank() == root) {
      std::vector<value_type> values;

      for (int p = 0; p < comm.size(); ++p)
        values.push_back(generator(p));

      std::cout << "Scattering " << kind << " from root "
                << root << "..." << std::endl;

      scatter(comm, values, value, root);
    } else {
      scatter(comm, value, root);
    }

    BOOST_CHECK(value == generator(comm.rank()));
  }

  comm.barrier();
}


//
// Generators to test with scatter/scatterv
//
struct int_generator
{
  typedef int result_type;

  int operator()(int p) const { return 17 + p; }
};

struct gps_generator
{
  typedef gps_position result_type;

  gps_position operator()(int p) const
  {
    return gps_position(39 + p, 16, 20.2799);
  }
};

struct string_generator
{
  typedef std::string result_type;

  std::string operator()(int p) const
  {
    std::string result = boost::lexical_cast<std::string>(p);
    result += " rosebud";
    if (p != 1) result += 's';
    return result;
  }
};

struct string_list_generator
{
  typedef std::list<std::string> result_type;

  std::list<std::string> operator()(int p) const
  {
    std::list<std::string> result;
    for (int i = 0; i <= p; ++i) {
      std::string value = boost::lexical_cast<std::string>(i);
      result.push_back(value);
    }
    return result;
  }
};

std::ostream&
operator<<(std::ostream& out, std::list<std::string> const& l) {
  out << '[';
  std::copy(l.begin(), l.end(), std::ostream_iterator<std::string>(out, " "));
  out << ']';
  return out;
}

template<typename Generator>
void
scatterv_test(const communicator& comm, Generator generator,
              const char* kind, int root = -1)
{
  typedef typename Generator::result_type value_type;

  if (root == -1) {
    for (root = 0; root < comm.size(); ++root)
      scatterv_test(comm, generator, kind, root);
  } else {
    using boost::mpi::scatterv;

    int mysize = comm.rank() + 1;
    std::vector<value_type> myvalues(mysize);

    if (comm.rank() == root) {
      std::vector<value_type> values;
      std::vector<int> sizes(comm.size());

      // process p will receive p+1 identical generator(p) elements
      for (int p = 0; p < comm.size(); ++p) {
        for (int i = 0; i < p+1; ++i)
          values.push_back(generator(p));
        sizes[p] = p + 1;
      }

      std::cout << "Scatteringv " << kind << " from root "
                << root << "..." << std::endl;
      assert(mysize == sizes[comm.rank()]);
      scatterv(comm, values, sizes, &(myvalues[0]), root);
    } else {
      scatterv(comm, &(myvalues[0]), mysize, root);
    }

    for (int i = 0; i < mysize; ++i)
      BOOST_CHECK(myvalues[i] == generator(comm.rank()));
  }

  comm.barrier();
}

template<typename Generator>
void
scatterd_test(const communicator& comm, Generator generator,
              const char* kind, int root = -1)
{
  typedef typename Generator::result_type value_type;

  if (root == -1) {
    for (root = 0; root < comm.size(); ++root)
      scatterv_test(comm, generator, kind, root);
  } else {
    using boost::mpi::scatterv;
    
    int mysize = comm.rank() + 1;
    std::vector<value_type> myvalues(mysize);
    
    if (comm.rank() == root) {
      std::vector<value_type> values;
      std::vector<int> sizes(comm.size());
      std::vector<int> displs(comm.size());
      value_type noise = generator(comm.size()+1);
      // process p will receive a payload of p+1 identical generator(p) elements
      // root will insert pseudo random pading between each payload.
      int shift = 0; // the current position of next payload in source array
      for (int p = 0; p < comm.size(); ++p) {
        int size = p+1;
        int pad  = p % 3;
        // padding
        for (int i = 0; i < pad; ++i) {
          values.push_back(noise);
        }
        // payload
        for (int i = 0; i < size; ++i)
          values.push_back(generator(p));
        shift += pad;
        displs[p] = shift;
        sizes[p]  = size;
        shift += size;
      }

      std::cout << "Scatteringv " << kind << " from root "
                << root << "..." << std::endl;
      assert(mysize == sizes[comm.rank()]);
      scatterv(comm, values, sizes, displs, &(myvalues[0]), mysize, root);
    } else {
      scatterv(comm, &(myvalues[0]), mysize, root);
    }

    for (int i = 0; i < mysize; ++i)
      BOOST_CHECK(myvalues[i] == generator(comm.rank()));
  }

  comm.barrier();
}


BOOST_AUTO_TEST_CASE(simple_scatter)
{
  environment env;
  communicator comm;

  scatter_test(comm, int_generator(), "integers");
  scatter_test(comm, gps_generator(), "GPS positions");
  scatter_test(comm, string_generator(), "string");
  scatter_test(comm, string_list_generator(), "list of strings");

  scatterv_test(comm, int_generator(), "integers");
  scatterv_test(comm, gps_generator(), "GPS positions");
  scatterv_test(comm, string_generator(), "string");
  scatterv_test(comm, string_list_generator(), "list of strings");

  scatterd_test(comm, int_generator(), "integers");
  scatterd_test(comm, gps_generator(), "GPS positions");
  scatterd_test(comm, string_generator(), "string");
  scatterd_test(comm, string_list_generator(), "list of strings");
}
