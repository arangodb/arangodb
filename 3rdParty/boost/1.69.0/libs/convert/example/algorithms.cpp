// Copyright (c) 2009-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <boost/convert/lexical_cast.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <vector>

using std::string;

static
void
introduction()
{
//[algorithm_introduction

   /*`The following code demonstrates conversion of an array of integers from their textual ['hexadecimal]
      representation. It assigns -1 to those which fail to convert:
   */

    boost::array<char const*, 3> strs = {{ " 5", "0XF", "not an int" }};
    std::vector<int>             ints;
    boost::cnv::cstream           cnv;

    // Configure converter to read hexadecimal, skip (leading) white spaces.
    cnv(std::hex)(std::skipws);

    std::transform(strs.begin(), strs.end(), std::back_inserter(ints),
        boost::cnv::apply<int>(boost::cref(cnv)).value_or(-1));

    BOOST_TEST(ints.size() == 3); // Number of values processed.
    BOOST_TEST(ints[0] ==  5);    // " 5"
    BOOST_TEST(ints[1] == 15);    // "0XF"
    BOOST_TEST(ints[2] == -1);    // "not an int"
//]
}

static
void
example1()
{
//[algorithm_example1
    /*`The following code demonstrates a failed attempt (and one of the reasons ['Boost.Convert]
       has been developed) to convert a few `string`s to `int`s with `boost::lexical_cast`:
    */

    boost::array<char const*, 3> strs = {{ " 5", "0XF", "not an int" }};
    std::vector<int>             ints;

    try
    {
        std::transform(strs.begin(), strs.end(), std::back_inserter(ints),
            boost::bind(boost::lexical_cast<int, string>, _1));

        BOOST_TEST(0 && "Never reached!");
    }
    catch (std::exception&)
    {
        BOOST_TEST(ints.size() == 0); // No strings converted.
    }
//]
}

static
void
example2()
{
//[algorithm_example2
    /*`If the exception-throwing behavior is the desired behavior, then ['Boost.Convert] supports that.
      In addition, it also supports a non-throwing process-flow:
    */
    boost::array<char const*, 3> strs = {{ " 5", "0XF", "not an int" }};
    std::vector<int>             ints;

    std::transform(strs.begin(), strs.end(), std::back_inserter(ints),
        boost::cnv::apply<int>(boost::cnv::lexical_cast()).value_or(-1));

    BOOST_TEST(ints.size() == 3);
    BOOST_TEST(ints[0] == -1); // Failed conversion does not throw.
    BOOST_TEST(ints[1] == -1); // Failed conversion does not throw.
    BOOST_TEST(ints[2] == -1); // Failed conversion does not throw.
//]
}

static
void
example3()
{
//[algorithm_example3
    /*`Deploying `boost::cnv::cstream` with better formatting capabilities yields
       better results with exception-throwing and non-throwing process-flows still supported:
    */

    boost::array<char const*, 3> strs = {{ " 5", "0XF", "not an int" }};
    std::vector<int>             ints;
    boost::cnv::cstream           cnv;

    try
    {
        std::transform(strs.begin(), strs.end(), std::back_inserter(ints),
            boost::cnv::apply<int>(boost::cref(cnv(std::hex)(std::skipws))));

        BOOST_TEST(0 && "Never reached!");
    }
    catch (boost::bad_optional_access const&)
    {
        BOOST_TEST(ints.size() == 2); // Only the first two strings converted.
        BOOST_TEST(ints[0] ==  5);    // " 5"
        BOOST_TEST(ints[1] == 15);    // "0XF"

        // "not an int" causes the exception thrown.
    }
//]
}

static
void
example4()
{
    boost::array<char const*, 3> strs = {{ " 5", "0XF", "not an int" }};
    std::vector<int>             ints;
    boost::cnv::cstream           cnv;

//[algorithm_example4

    std::transform(strs.begin(), strs.end(), std::back_inserter(ints),
        boost::cnv::apply<int>(boost::cref(cnv(std::hex)(std::skipws))).value_or(-1));

    BOOST_TEST(ints.size() == 3);
    BOOST_TEST(ints[0] ==  5);
    BOOST_TEST(ints[1] == 15);
    BOOST_TEST(ints[2] == -1); // Failed conversion

    /*`[important One notable difference in the deployment of `boost::cnv::cstream` with algorithms is
       the use of `boost::cref` (or `std::cref` in C++11).

       It needs to be remembered that with standard algorithms the deployed converter needs to be
       [@http://en.cppreference.com/w/cpp/concept/TriviallyCopyable copyable] or
       [@http://en.cppreference.com/w/cpp/concept/MoveAssignable movable (C++11)]
       and is, in fact, copied or moved by the respective algorithm before being used.
       Given that `std::cstringstream` is not copyable, `boost::cnv::cstream` is not copyable either.
       That limitation is routinely worked-around using `boost::ref` or `boost::cref`.]
    */
//]
}

static
void
example5()
{
//[algorithm_example5
    /*`And now an example of algorithm-based integer-to-string formatted conversion with
       `std::hex`, `std::uppercase` and `std::showbase` formatting applied:
    */
    boost::array<int, 3>     ints = {{ 15, 16, 17 }};
    std::vector<std::string> strs;
    boost::cnv::cstream       cnv;

    cnv(std::hex)(std::uppercase)(std::showbase);

    std::transform(ints.begin(), ints.end(), std::back_inserter(strs),
        boost::cnv::apply<string>(boost::cref(cnv)));

    BOOST_TEST(strs.size() == 3);
    BOOST_TEST(strs[0] ==  "0XF"); // 15
    BOOST_TEST(strs[1] == "0X10"); // 16
    BOOST_TEST(strs[2] == "0X11"); // 17
//]
}

int
main(int, char const* [])
{
    introduction();
    example1();
    example2();
    example3();
    example4();
    example5();

    return boost::report_errors();
}
