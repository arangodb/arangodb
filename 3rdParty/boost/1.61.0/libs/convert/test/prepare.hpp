// Boost.Convert test and usage example
// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#ifndef BOOST_CONVERT_TEST_PREPARE_HPP
#define BOOST_CONVERT_TEST_PREPARE_HPP

#include <boost/array.hpp>
#include <ctime>
#include <cstdlib>

// boostinspect:nounnamed
namespace { namespace local
{
    // C1. 18 = 9 positive + 9 negative numbers with the number of digits from 1 to 9.
    //     Even though INT_MAX(32) = 2147483647, i.e. 10 digits (not to mention long int)
    //     we only test up to 9 digits as Spirit does not handle more than 9.

    typedef boost::array<my_string, 18> strings; //C1
    ///////////////////////////////////////////////////////////////////////////
    // Generate a random number string with N digits
    std::string
    gen_int(int digits, bool negative)
    {
        std::string result;

        if (negative)                       // Prepend a '-'
            result += '-';

        result += '1' + (std::rand()%9);         // The first digit cannot be '0'

        for (int i = 1; i < digits; ++i)    // Generate the remaining digits
            result += '0' + (std::rand()%10);
        return result;
    }

    local::strings const&
    get_strs()
    {
        static local::strings strings;
        static bool            filled;
        static bool          negative = true;

        if (!filled)
        {
            // Seed the random generator
            std::srand(std::time(0));

            for (size_t k = 0; k < strings.size(); ++k)
                strings[k] = local::gen_int(k/2 + 1, negative = !negative).c_str();

            filled = true;
        }
        return strings;
    }
}}

#endif // BOOST_CONVERT_TEST_PREPARE_HPP
