/*
Copyright 2019-2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/io/quoted.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>
#include <string>

int main()
{
    {
        std::ostringstream os;
        os.width(2);
        os.fill('.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        os << boost::io::quoted("xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "\"xy\"");
    }
    {
        std::wostringstream os;
        os.width(2);
        os.fill(L'.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        os << boost::io::quoted(L"xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"\"xy\"");
    }
    {
        std::ostringstream os;
        os.width(2);
        os.fill('.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        os << boost::io::quoted("xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "\"xy\"");
    }
    {
        std::wostringstream os;
        os.width(2);
        os.fill(L'.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        os << boost::io::quoted(L"xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"\"xy\"");
    }
    {
        std::ostringstream os;
        os.width(6);
        os.fill('.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        os << boost::io::quoted("xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "\"xy\"..");
    }
    {
        std::wostringstream os;
        os.width(6);
        os.fill(L'.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        os << boost::io::quoted(L"xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"\"xy\"..");
    }
    {
        std::ostringstream os;
        os.width(6);
        os.fill('.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        os << boost::io::quoted("xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "..\"xy\"");
    }
    {
        std::wostringstream os;
        os.width(6);
        os.fill(L'.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        os << boost::io::quoted(L"xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"..\"xy\"");
    }
    {
        std::ostringstream os;
        os.width(14);
        os.fill('.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        os << boost::io::quoted("xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "\"xy\"..........");
    }
    {
        std::wostringstream os;
        os.width(14);
        os.fill(L'.');
        os.setf(std::ios_base::left, std::ios_base::adjustfield);
        os << boost::io::quoted(L"xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"\"xy\"..........");
    }
    {
        std::ostringstream os;
        os.width(14);
        os.fill('.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        os << boost::io::quoted("xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == "..........\"xy\"");
    }
    {
        std::wostringstream os;
        os.width(14);
        os.fill(L'.');
        os.setf(std::ios_base::right, std::ios_base::adjustfield);
        os << boost::io::quoted(L"xy");
        BOOST_TEST(os.good());
        BOOST_TEST(os.width() == 0);
        BOOST_TEST(os.str() == L"..........\"xy\"");
    }
    return boost::report_errors();
}
