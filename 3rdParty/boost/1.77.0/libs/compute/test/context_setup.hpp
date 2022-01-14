//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Denis Demidov
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_TEST_CONTEXT_SETUP_HPP
#define BOOST_COMPUTE_TEST_CONTEXT_SETUP_HPP

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>

#include "opencl_version_check.hpp"

struct Context {
    boost::compute::device        device;
    boost::compute::context       context;
    boost::compute::command_queue queue;

    Context() :
        device ( boost::compute::system::default_device() ),
        context( boost::compute::system::default_context() ),
        queue  ( boost::compute::system::default_queue() )
    {}
};

BOOST_FIXTURE_TEST_SUITE(compute_test, Context)

#endif
