// Copyright (C) 2005, 2006 Douglas Gregor.

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


// a test of pointer serialization
#include <boost/mpi.hpp>
#include <boost/serialization/shared_ptr.hpp>

#define BOOST_TEST_MODULE mpi_pointer
#include <boost/test/included/unit_test.hpp>

class A
{
 public:
  int i;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & i;
  }
};

BOOST_AUTO_TEST_CASE(pointer)
{
  boost::mpi::environment env;
  boost::mpi::communicator world;

  if (world.rank() == 0)  {
    boost::shared_ptr<A> p(new A);
    p->i = 42;
    world.send(1, 0, p);
  } else if (world.rank() == 1)  {
    boost::shared_ptr<A> p;
    world.recv(0, 0, p);
    std::cout << p->i << std::endl;
    BOOST_CHECK(p->i==42);
  }
}

