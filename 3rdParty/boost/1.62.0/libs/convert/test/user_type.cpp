// Boost.Convert test and usage example
// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#ifdef BOOST_CONVERT_INTEL_SFINAE_BROKEN
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include <vector>

using std::string;
using std::wstring;

static
void
test_user_type()
{
    boost::cnv::cstream cnv; // stringstream-based char converter

    direction const            up_dir1 = direction::up;
    direction const            dn_dir1 = direction::dn;
//  string const           up_dir0_str = boost::convert<string, direction>(direction::up, cnv).value();
//  string const           dn_dir0_str = boost::convert<string, direction>(direction::dn, cnv).value();
    string const           up_dir1_str = boost::convert<string>(up_dir1, cnv).value();
    string const           dn_dir1_str = boost::convert<string>(dn_dir1, cnv).value();
    direction const            up_dir2 = boost::convert<direction>(up_dir1_str, cnv).value();
    direction const            dn_dir2 = boost::convert<direction>(dn_dir1_str, cnv).value();
    direction const            up_dir3 = boost::convert<direction>(up_dir1_str, cnv).value();
    direction const            dn_dir3 = boost::convert<direction>(dn_dir1_str, cnv).value();
    direction const            dn_dir4 = boost::convert<direction>("junk", cnv).value_or(direction::dn);
    boost::optional<direction> up_dir4 = boost::convert<direction>("junk", cnv);

//  BOOST_TEST(up_dir0_str == "up");
//  BOOST_TEST(dn_dir0_str == "dn");
    BOOST_TEST(up_dir1_str == "up");
    BOOST_TEST(dn_dir1_str == "dn");
    BOOST_TEST(up_dir2 == up_dir1);
    BOOST_TEST(dn_dir2 == dn_dir1);
    BOOST_TEST(up_dir3 == direction::up);
    BOOST_TEST(dn_dir3 == direction::dn);
    BOOST_TEST(dn_dir4 == direction::dn);
    BOOST_TEST(!up_dir4); // Failed conversion
}

static
void
test_algorithms()
{
//[algorithm_example6
    boost::array<change, 3>             chgs1 = {{ change::no, change::up, change::dn }};
    boost::array<change::value_type, 3> chgs2 = {{ change::no, change::up, change::dn }};
    std::vector<std::string>            strs1;
    std::vector<std::string>            strs2;
    std::vector<std::string>            strs3;
    boost::cnv::cstream                   cnv;

    std::transform(chgs1.begin(), chgs1.end(), std::back_inserter(strs1), 
        boost::cnv::apply<string>(boost::cref(cnv))); // Deduced TypeIn is 'change'

    std::transform(chgs2.begin(), chgs2.end(), std::back_inserter(strs2), 
        boost::cnv::apply<string>(boost::cref(cnv))); // Deduced TypeIn is 'change::value_type'

    BOOST_TEST(strs1.size() == 3);
    BOOST_TEST(strs1[0] == "no");
    BOOST_TEST(strs1[1] == "up");
    BOOST_TEST(strs1[2] == "dn");

    BOOST_TEST(strs2.size() == 3);
    BOOST_TEST(strs2[0] == "0");
    BOOST_TEST(strs2[1] == "1");
    BOOST_TEST(strs2[2] == "2");
//]
//[algorithm_example7

    std::transform(chgs2.begin(), chgs2.end(), std::back_inserter(strs3), 
        boost::cnv::apply<string, change>(boost::cref(cnv)));

    BOOST_TEST(strs3.size() == 3);
    BOOST_TEST(strs3[0] == "no");
    BOOST_TEST(strs3[1] == "up");
    BOOST_TEST(strs3[2] == "dn");
//]
}

int
main(int, char const* [])
{
    test_user_type();
    test_algorithms();

    return boost::report_errors();
}

#endif
