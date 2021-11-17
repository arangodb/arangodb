// (c) Copyright Andrey Semashev 2018

// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The test verifies that basic_random_generator is not copy constructible

#include <boost/uuid/random_generator.hpp>
#include <boost/random/linear_congruential.hpp>

int main()
{
    boost::uuids::basic_random_generator<boost::rand48> uuid_gen1;
    boost::uuids::basic_random_generator<boost::rand48> uuid_gen2(uuid_gen1);

    return 1;
}
