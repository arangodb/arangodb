// Copyright (c) 2009-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#if defined(BOOST_CONVERT_IS_NOT_SUPPORTED)
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/detail/lightweight_test.hpp>

static
bool
my_cypher(std::string const& value_in, boost::optional<std::string>& value_out)
{
    size_t const cypher = 'A' - '1';

    value_out = value_in;

    for (std::string::iterator it = value_out->begin(); it != value_out->end(); ++it)
        (*it < 'A') ? (*it += cypher) : (*it -= cypher);

    return true;
}

int
main(int, char const* [])
{
    ////////////////////////////////////////////////////////////////////////////
    // Testing custom converter.
    ////////////////////////////////////////////////////////////////////////////

    std::string encrypted = boost::convert<std::string>("ABC", my_cypher).value();
    std::string decrypted = boost::convert<std::string>(encrypted, my_cypher).value();

    BOOST_TEST(encrypted == "123");
    BOOST_TEST(decrypted == "ABC");

    return boost::report_errors();
}

#endif
