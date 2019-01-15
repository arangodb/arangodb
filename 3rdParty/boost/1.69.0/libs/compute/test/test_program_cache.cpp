//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestProgramCache
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/utility/program_cache.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(setup)
{
    // get default context
    compute::context ctx = context;

    // get program cache
    boost::shared_ptr<compute::program_cache> cache =
        compute::program_cache::get_global_cache(ctx);

    // try to load a null string
    BOOST_CHECK(cache->get(std::string()) == boost::none);

    // try to load a non-existant program
    BOOST_CHECK(cache->get("nonexistant") == boost::none);

    // create and store a program
    const char p1_source[] =
        "__kernel void add(__global int *a, int x)\n"
        "{\n"
        "    a[get_global_id(0)] += x;\n"
        "}\n";
    compute::program p1 =
        compute::program::create_with_source(p1_source, ctx);
    p1.build();
    cache->insert("p1", p1);

    // try to load the program
    BOOST_CHECK(cache->get("p1") == p1);

    // create a copy of the context
    compute::context ctx_copy = ctx;

    // check that they both have the same cl_context
    BOOST_CHECK(ctx_copy.get() == ctx.get());

    // check that the cache is the same
    boost::shared_ptr<compute::program_cache> cache_copy =
        compute::program_cache::get_global_cache(ctx_copy);
    BOOST_CHECK(cache_copy == cache);

    // try to load the program again
    BOOST_CHECK(cache_copy->get("p1") == p1);
}

BOOST_AUTO_TEST_CASE(evict)
{
    // create cache with capacity of four and insert four programs
    compute::program_cache cache(4);
    cache.insert("a", compute::program());
    cache.insert("b", compute::program());
    cache.insert("c", compute::program());
    cache.insert("d", compute::program());

    // check that all four programs still reside in the cache
    BOOST_CHECK(cache.get("a") != boost::none);
    BOOST_CHECK(cache.get("b") != boost::none);
    BOOST_CHECK(cache.get("c") != boost::none);
    BOOST_CHECK(cache.get("d") != boost::none);

    // insert fifth program which should evict the oldest ("a")
    cache.insert("e", compute::program());

    // check that "a" has been evicted and that "e" is now present
    BOOST_CHECK(cache.get("a") == boost::none);
    BOOST_CHECK(cache.get("b") != boost::none);
    BOOST_CHECK(cache.get("c") != boost::none);
    BOOST_CHECK(cache.get("d") != boost::none);
    BOOST_CHECK(cache.get("e") != boost::none);

    // clear cache and ensure no program objects are found
    cache.clear();
    BOOST_CHECK(cache.get("a") == boost::none);
    BOOST_CHECK(cache.get("b") == boost::none);
    BOOST_CHECK(cache.get("c") == boost::none);
    BOOST_CHECK(cache.get("d") == boost::none);
    BOOST_CHECK(cache.get("e") == boost::none);
}

BOOST_AUTO_TEST_SUITE_END()
