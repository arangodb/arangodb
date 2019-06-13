//
// Copyright (c) 2018 James E. King III
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//   http://www.boost.org/LICENCE_1_0.txt)
//
// std::hash support for uuid
//

#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_hash.hpp>
#include <iostream>

#if !defined(BOOST_NO_CXX11_HDR_UNORDERED_SET) && !defined(BOOST_NO_CXX11_HDR_FUNCTIONAL)
#include <unordered_set>
#endif

int main(int, char*[])
{
#if !defined(BOOST_NO_CXX11_HDR_UNORDERED_SET) && !defined(BOOST_NO_CXX11_HDR_FUNCTIONAL)
    using namespace boost::uuids;
    string_generator gen;

    uuid u1 = gen("01234567-89AB-CDEF-0123-456789abcdef");
    uuid u2 = gen("fedcba98-7654-3210-fedc-ba9876543210");

    std::hash<uuid> hasher;
    BOOST_TEST(hasher(u1) != hasher(u2));

    std::unordered_set<boost::uuids::uuid> uns;
    uns.insert(u1);
    uns.insert(u2);

    BOOST_TEST_EQ(uns.size(), 2);

    return boost::report_errors();
#else
    std::cout << "test skipped - no library support for std::hash or std::unordered_set" << std::endl;
    return 0;
#endif
}
