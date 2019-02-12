//  (C) Copyright Andy Tompkins 2010. Permission to copy, use, modify, sell and
//  distribute this software is granted provided this copyright notice appears
//  in all copies. This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.

// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  libs/uuid/test/test_random_generator.cpp  -------------------------------//

#include <boost/core/ignore_unused.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/predef/library/c/cloudabi.h>
#include <boost/random.hpp>
#include <boost/uuid/entropy_error.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/move/utility_core.hpp>

template <typename RandomUuidGenerator>
void check_random_generator(RandomUuidGenerator& uuid_gen)
{
    boost::uuids::uuid u1 = uuid_gen();
    boost::uuids::uuid u2 = uuid_gen();

    BOOST_TEST_NE(u1, u2);

    // check variant
    BOOST_TEST_EQ(u1.variant(), boost::uuids::uuid::variant_rfc_4122);

    // version
    BOOST_TEST_EQ(u1.version(), boost::uuids::uuid::version_random_number_based);
}

// This is the example block from the documentation - ensure it works!
void test_examples()
{
    // Depending on the platform there may be a setup cost in
    // initializing the generator so plan to reuse it if you can.
    boost::uuids::random_generator gen;
    boost::uuids::uuid id = gen();
    boost::uuids::uuid id2 = gen();
#if !BOOST_LIB_C_CLOUDABI
    std::cout << id << std::endl;
    std::cout << id2 << std::endl;
#endif
    BOOST_TEST_NE(id, id2);
    boost::ignore_unused(id);
    boost::ignore_unused(id2);

    // You can still use a PseudoRandomNumberGenerator to create
    // UUIDs, however this is not the preferred mechanism.  For
    // large numbers of UUIDs this may be more CPU efficient but
    // it comes with all the perils of a PRNG.  The test code
    // in test_bench_random identifies the transition point for
    // a given system.
    boost::uuids::random_generator_mt19937 bulkgen;
    for (size_t i = 0; i < 1000; ++i)
    {
        boost::uuids::uuid u = bulkgen();
        // do something with u
        boost::ignore_unused(u);
    }
}

int main(int, char*[])
{
    using namespace boost::uuids;

    // default random number generator
    {
        random_generator uuid_gen1;
        check_random_generator(uuid_gen1);
#if !BOOST_LIB_C_CLOUDABI
        // dump 10 of them to cout for observation
        for (size_t i = 0; i < 10; ++i)
        {
            std::cout << uuid_gen1() << std::endl;
        }
#endif
    }
    
    // specific random number generator
    {
        basic_random_generator<boost::rand48> uuid_gen2;
        check_random_generator(uuid_gen2);
    }

    // pass by reference
    {
        boost::ecuyer1988 ecuyer1988_gen;
        basic_random_generator<boost::ecuyer1988> uuid_gen3(ecuyer1988_gen);
        check_random_generator(uuid_gen3);
    }

    // pass by pointer
    {
        boost::lagged_fibonacci607 lagged_fibonacci607_gen;
        basic_random_generator<boost::lagged_fibonacci607> uuid_gen4(&lagged_fibonacci607_gen);
        check_random_generator(uuid_gen4);
    }

    // check move construction
    {
        random_generator uuid_gen1;
        random_generator uuid_gen2(boost::move(uuid_gen1));
        boost::ignore_unused(uuid_gen2);
    }
    {
        basic_random_generator<boost::rand48> uuid_gen1;
        basic_random_generator<boost::rand48> uuid_gen2(boost::move(uuid_gen1));
        boost::ignore_unused(uuid_gen2);
    }

    // check move assignment
    {
        random_generator uuid_gen1, uuid_gen2;
        uuid_gen2 = boost::move(uuid_gen1);
        boost::ignore_unused(uuid_gen2);
    }
    {
        basic_random_generator<boost::rand48> uuid_gen1, uuid_gen2;
        uuid_gen2 = boost::move(uuid_gen1);
        boost::ignore_unused(uuid_gen2);
    }

    // there was a bug in basic_random_generator where it did not
    // produce very random numbers.  This checks for that bug.
    {
        random_generator_mt19937 bulkgen;
        uuid u = bulkgen();
        if ((u.data[4] == u.data[12]) &&
            (u.data[5] == u.data[9] && u.data[5] == u.data[13]) &&
            (u.data[7] == u.data[11] && u.data[7] == u.data[15]) &&
            (u.data[10] == u.data[14]))
        {
            BOOST_ERROR("basic_random_generator is not producing random uuids");
        }

        // pseudo
        check_random_generator(bulkgen);
    }

    // make sure default construction seeding is happening for PRNGs
    {
        random_generator_mt19937 gen1;
        random_generator_mt19937 gen2;
        BOOST_TEST_NE(gen1(), gen2());
    }

    // The example code snippet in the documentation
    test_examples();

    return boost::report_errors();
}
