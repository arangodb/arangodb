// Copyright (C) 2005-2006 Douglas Gregor <doug.gregor@gmail.com>

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// A test of the all_gather() collective.

#include <algorithm>

#include <boost/mpi/collectives/all_gather.hpp>
#include <boost/mpi/collectives/all_gatherv.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/list.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/lexical_cast.hpp>

#define BOOST_TEST_MODULE mpi_all_gather
#include <boost/test/included/unit_test.hpp>

#include "gps_position.hpp"

namespace mpi = boost::mpi;

template<typename Generator>
void
all_gather_test(const mpi::communicator& comm, Generator generator,
                std::string kind)
{
  typedef typename Generator::result_type value_type;
  value_type value = generator(comm.rank());

  std::vector<value_type> values;
  if (comm.rank() == 0) {
    std::cout << "Gathering " << kind << "...";
    std::cout.flush();
  }

  mpi::all_gather(comm, value, values);

  std::vector<value_type> expected_values;
  for (int p = 0; p < comm.size(); ++p)
    expected_values.push_back(generator(p));
  BOOST_CHECK(values == expected_values);
  if (comm.rank() == 0 && values == expected_values)
    std::cout << "OK." << std::endl;

  (comm.barrier)();
}

template<typename Generator>
void
all_gatherv_test(const mpi::communicator& comm, Generator generator,
                 std::string kind)
{
  typedef typename Generator::result_type value_type;
  using boost::mpi::all_gatherv;

  std::vector<value_type> myvalues, expected, values;
  std::vector<int>        sizes;
  for(int r = 0; r < comm.size(); ++r) {
    value_type value = generator(r);
    sizes.push_back(r+1);
    for (int k=0; k < r+1; ++k) {
      expected.push_back(value);
      if(comm.rank() == r) {
        myvalues.push_back(value);
      }
    }
  }
  if (comm.rank() == 0) {
    std::cout << "Gathering " << kind << "...";
    std::cout.flush();
  }
  
  mpi::all_gatherv(comm, myvalues, values, sizes);
  
  BOOST_CHECK(values == expected);
  
  if (comm.rank() == 0 && values == expected)
    std::cout << "OK." << std::endl;
  
  (comm.barrier)();
}

// Generates integers to test with gather()
struct int_generator
{
  typedef int result_type;

  int operator()(int p) const { return 17 + p; }
};

// Generates GPS positions to test with gather()
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

BOOST_AUTO_TEST_CASE(all_gather)
{
  boost::mpi::environment env;
  mpi::communicator comm;
  all_gather_test(comm, int_generator(), "integers");
  all_gather_test(comm, gps_generator(), "GPS positions");
  all_gather_test(comm, string_generator(), "string");
  all_gather_test(comm, string_list_generator(), "list of strings");
  
  all_gatherv_test(comm, int_generator(), "integers");
  all_gatherv_test(comm, gps_generator(), "GPS positions");
  all_gatherv_test(comm, string_generator(), "string");
  all_gatherv_test(comm, string_list_generator(), "list of strings");
}
