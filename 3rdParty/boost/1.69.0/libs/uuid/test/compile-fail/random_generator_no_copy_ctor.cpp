// (c) Copyright Andrey Semashev 2018

// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// The test verifies that random_generator is not copy constructible

#include <boost/uuid/random_generator.hpp>

int main()
{
    boost::uuids::random_generator uuid_gen1;
    boost::uuids::random_generator uuid_gen2(uuid_gen1);

    return 1;
}
