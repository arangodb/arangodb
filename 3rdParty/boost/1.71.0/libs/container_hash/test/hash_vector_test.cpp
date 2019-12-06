
// Copyright 2005-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "./config.hpp"

#ifdef BOOST_HASH_TEST_EXTENSIONS
#  ifdef BOOST_HASH_TEST_STD_INCLUDES
#    include <functional>
#  else
#    include <boost/container_hash/hash.hpp>
#  endif
#endif

#include <boost/core/lightweight_test.hpp>

#ifdef BOOST_HASH_TEST_EXTENSIONS

#include <vector>

using std::vector;
#define CONTAINER_TYPE vector
#include "./hash_sequence_test.hpp"

#endif // BOOST_HASH_TEST_EXTENSIONS

namespace vector_bool_tests
{
    void vector_bool_test() {
        std::vector<bool> x_empty1,x_empty2,x1,x1a,x2,x3;

        x1.push_back(0);
        x1a.push_back(0);
        x2.push_back(1);
        x3.push_back(0);
        x3.push_back(0);

        BOOST_HASH_TEST_NAMESPACE::hash<std::vector<bool> > hasher;

        BOOST_TEST_EQ(hasher(x_empty1), hasher(x_empty1));
        BOOST_TEST_EQ(hasher(x_empty1), hasher(x_empty2));
        BOOST_TEST_NE(hasher(x_empty1), hasher(x1));
        BOOST_TEST_NE(hasher(x_empty1), hasher(x2));
        BOOST_TEST_NE(hasher(x_empty1), hasher(x3));

        BOOST_TEST_EQ(hasher(x1), hasher(x1));
        BOOST_TEST_EQ(hasher(x1), hasher(x1a));
        BOOST_TEST_NE(hasher(x1), hasher(x2));
        BOOST_TEST_NE(hasher(x1), hasher(x3));

        BOOST_TEST_EQ(hasher(x2), hasher(x2));
        BOOST_TEST_NE(hasher(x2), hasher(x3));

        BOOST_TEST_EQ(hasher(x3), hasher(x3));
    }
}

int main()
{
#ifdef BOOST_HASH_TEST_EXTENSIONS
    vector_tests::vector_hash_integer_tests();
#endif

    vector_bool_tests::vector_bool_test();

    return boost::report_errors();
}
