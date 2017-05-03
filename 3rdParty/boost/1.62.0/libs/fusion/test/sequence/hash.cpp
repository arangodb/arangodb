/*=============================================================================
    Copyright (c) 2014 Christoph Weiss

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <string>

#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/sequence/hash.hpp>

struct test_struct
{
    test_struct(bool bb, int ii, char cc, std::string const& ss) :
        b(bb),
        i(ii),
        c(cc),
        s(ss) {}

    bool b;
    int i;
    char c;
    std::string s;
};

BOOST_FUSION_ADAPT_STRUCT(
    test_struct,
    (bool, b)
    (int, i)
    (char, c)
    (std::string, s)
)

int main()
{
    using boost::fusion::hash_value;

    const test_struct a0(false, 1, 'c', "Hello Nurse"),
                      a1(false, 1, 'c', "Hello Nurse"),
                      b(true, 1, 'c', "Hello Nurse"),
                      c(false, 0, 'c', "Hello Nurse"),
                      d(false, 1, 'd', "Hello Nurse"),
                      e(false, 1, 'c', "Hello World");

    BOOST_TEST(hash_value(a0) == hash_value(a1));
    BOOST_TEST(hash_value(a0) != hash_value(b));
    BOOST_TEST(hash_value(a0) != hash_value(c));
    BOOST_TEST(hash_value(a0) != hash_value(d));
    BOOST_TEST(hash_value(a0) != hash_value(e));
    BOOST_TEST(hash_value(b) != hash_value(c));
    BOOST_TEST(hash_value(b) != hash_value(d));
    BOOST_TEST(hash_value(b) != hash_value(d));
    BOOST_TEST(hash_value(c) != hash_value(d));
    BOOST_TEST(hash_value(c) != hash_value(e));
    BOOST_TEST(hash_value(d) != hash_value(e));
}
