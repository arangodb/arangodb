// Boost.Convert test and usage example
// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

// This code has been adapted from libs/spirit/optimization/qi/int_parser.cpp.
// This code uses the performance testing framework from libs/spirit/optimization/measure.cpp.
// See these mentioned files for the copyright notice.

#include "./test.hpp"

#ifdef BOOST_CONVERT_INTEL_SFINAE_BROKEN
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/spirit.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/convert/lexical_cast.hpp>
#include "./prepare.hpp"

//#define main() old_str_to_int_test_spirit()
//#include <libs/spirit/optimization/qi/int_parser.cpp>
#include <libs/spirit/workbench/measure.hpp>
#include <string>
#include <vector>
#include <cstdlib>
#include <boost/spirit/include/qi.hpp>

namespace
{
    namespace local
    {
        struct base : test::base
        {
            base() : strings_(local::get_strs()) {}

            // Test strings are created as part of the object, i.e. on the stack to make sure
            // they are easily accessed.
            local::strings strings_;
        };
    }
    struct raw_lxcast_str_to_int_test : local::base
    {
        void benchmark()
        {
            for (size_t i = 0; i < strings_.size(); ++i)
                this->val += boost::lexical_cast<int>(strings_[i].c_str());
        }
    };
    struct cnv_lxcast_str_to_int_test : local::base
    {
        void benchmark()
        {
            for (size_t i = 0; i < strings_.size(); ++i)
                this->val += boost::convert<int>(strings_[i].c_str(), cnv).value();
        }
        boost::cnv::lexical_cast cnv;
    };
    struct raw_spirit_str_to_int_test : local::base
    {
        static int parse(char const* str)
        {
            char const* beg = str;
            char const* end = beg + strlen(str);
            int n;

            if (boost::spirit::qi::parse(beg, end, boost::spirit::qi::int_, n))
                if (beg == end)
                    return n;

            return (BOOST_ASSERT(0), 0);
        }
        void benchmark()
        {
            for (size_t i = 0; i < strings_.size(); ++i)
                this->val += parse(strings_[i].c_str());
        }
    };
    struct cnv_spirit_str_to_int_test : local::base
    {
        void benchmark()
        {
            for (size_t i = 0; i < strings_.size(); ++i)
                this->val += boost::convert<int>(strings_[i].c_str(), cnv).value();
        }
        boost::cnv::spirit cnv;
    };
}

int
main(int, char const* [])
{
    // This code has been adapted from libs/spirit/optimization/qi/int_parser.cpp.
    // This code uses the performance testing framework from libs/spirit/optimization/measure.cpp.
    // See these mentioned files for the copyright notice.

    BOOST_SPIRIT_TEST_BENCHMARK(
        10000000, // This is the maximum repetitions to execute
        (raw_lxcast_str_to_int_test)
        (cnv_lxcast_str_to_int_test)
        (raw_spirit_str_to_int_test)
        (cnv_spirit_str_to_int_test)
    )

    // This is ultimately responsible for preventing all the test code
    // from being optimized away.  Change this to return 0 and you
    // unplug the whole test's life support system.
    return test::live_code != 0;
}

#endif
