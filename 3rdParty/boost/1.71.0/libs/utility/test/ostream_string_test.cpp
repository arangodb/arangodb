/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/lightweight_test.hpp>
#include <boost/utility/ostream_string.hpp>
#include <sstream>
#include <string>

int main()
{
    {
        std::ostringstream os;
        os.width(1);
        os.fill('.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        boost::ostream_string(os, "xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "xy");
    }
    {
        std::wostringstream os;
        os.width(1);
        os.fill('.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        boost::ostream_string(os, L"xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"xy");
    }
    {
        std::ostringstream os;
        os.width(1);
        os.fill('.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        boost::ostream_string(os, "xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "xy");
    }
    {
        std::wostringstream os;
        os.width(1);
        os.fill('.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        boost::ostream_string(os, L"xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"xy");
    }
    {
        std::ostringstream os;
        os.width(4);
        os.fill('.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        boost::ostream_string(os, "xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "xy..");
    }
    {
        std::wostringstream os;
        os.width(4);
        os.fill(L'.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        boost::ostream_string(os, L"xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"xy..");
    }
    {
        std::ostringstream os;
        os.width(4);
        os.fill('.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        boost::ostream_string(os, "xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "..xy");
    }
    {
        std::wostringstream os;
        os.width(4);
        os.fill(L'.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        boost::ostream_string(os, L"xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"..xy");
    }
    {
        std::ostringstream os;
        os.width(12);
        os.fill('.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        boost::ostream_string(os, "xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "xy..........");
    }
    {
        std::wostringstream os;
        os.width(12);
        os.fill(L'.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        boost::ostream_string(os, L"xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"xy..........");
    }
    {
        std::ostringstream os;
        os.width(12);
        os.fill('.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        boost::ostream_string(os, "xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "..........xy");
    }
    {
        std::wostringstream os;
        os.width(12);
        os.fill(L'.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        boost::ostream_string(os, L"xy", 2);
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"..........xy");
    }
    return boost::report_errors();
}
