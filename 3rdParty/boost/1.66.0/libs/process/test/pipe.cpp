// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#define BOOST_TEST_MAIN

#include <boost/test/included/unit_test.hpp>
#include <iostream>
#include <thread>

#include <boost/process/pipe.hpp>

using namespace std;
namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(plain, *boost::unit_test::timeout(2))
{
    bp::pipe pipe;

    std::string in  = "test";
    pipe.write(in.c_str(), in.size());

    std::string out;
    out.resize(4);
    pipe.read(&out.front(), out.size());

    BOOST_CHECK_EQUAL(out, in);
}

BOOST_AUTO_TEST_CASE(named, *boost::unit_test::timeout(2))
{

#if defined( BOOST_WINDOWS_API )
    bp::pipe pipe("\\\\.\\pipe\\pipe_name");
#elif defined( BOOST_POSIX_API )
    bp::pipe pipe("./test_pipe");
#endif

    std::string in  = "xyz";
    pipe.write(in.c_str(), in.size());


    std::string out;
    out.resize(3);
    pipe.read(&out.front(), out.size());


    BOOST_CHECK_EQUAL(out, in);
}

BOOST_AUTO_TEST_CASE(copy_pipe, *boost::unit_test::timeout(2))
{
    bp::pipe pipe;

    std::string in  = "test";
    pipe.write(in.c_str(), in.size());

    std::string out;
    out.resize(4);
    auto p2 = pipe;
    p2.read(&out.front(), out.size());

    BOOST_CHECK_EQUAL(out, in);
}

BOOST_AUTO_TEST_CASE(move_pipe, *boost::unit_test::timeout(2))
{
    bp::pipe pipe;

    std::string in  = "test";
    pipe.write(in.c_str(), in.size());

    std::string out;
    out.resize(4);
    auto p2 = std::move(pipe);
    p2.read(&out.front(), out.size());

    BOOST_CHECK_EQUAL(out, in);
}

BOOST_AUTO_TEST_CASE(stream, *boost::unit_test::timeout(2))
{

    bp::pipe pipe;

    bp::pstream os(pipe);
    bp::ipstream is(pipe);

    int i = 42, j = 0;

    os << i << std::endl;
    os << std::endl;
    is >> j;

    BOOST_CHECK_EQUAL(i, j);
}

BOOST_AUTO_TEST_CASE(stream_line, *boost::unit_test::timeout(2))
{

    bp::pstream os;

    std::string s = "My Test String";

    std::string out;

    os << s <<  std::endl;

    std::getline(os, out);

    auto size = (out.size() < s.size()) ? out.size() : s.size();


    BOOST_CHECK_EQUAL_COLLECTIONS(
               s.begin(),   s.  begin() + size,
               out.begin(), out.begin() + size
               );
}


BOOST_AUTO_TEST_CASE(large_data, *boost::unit_test::timeout(20))
{
    bp::pipe pipe;

    bp::pipebuf is_buf(pipe);
    bp::pipebuf os_buf(std::move(pipe));

    std::istream is(&is_buf);
    std::ostream os(&os_buf);

    std::string in(1000000, '0');
    std::string out;

    int cnt = 0;
    for (auto & c: in)
        c = (cnt++ % 26) + 'A';

    std::thread th([&]{os << in << std::endl;});

    is >> out;
    BOOST_REQUIRE_EQUAL_COLLECTIONS(out.begin(), out.end(), in.begin(), in.end());
    th.join();
}

BOOST_AUTO_TEST_CASE(closed, *boost::unit_test::timeout(2))
{
    bp::opstream os;
    bp::ipstream is;

    os.pipe().close();
    is.pipe().close();

    int i;

    BOOST_CHECK(!(os << 42 << endl));
    BOOST_CHECK(!(is >> i));
}


BOOST_AUTO_TEST_CASE(coverage, *boost::unit_test::timeout(5))
{
    //more of a syntax check, since template.
    {
        bp::pipe p1;
        bp::ipstream is1(p1);
        bp::ipstream is2(std::move(p1));

        is2.pipe(is1.pipe());

        bp::pipe p2_;
        bp::pipe p2 = p2_;
        BOOST_REQUIRE_NO_THROW(p2_ == p2);
        BOOST_CHECK(p2_ == p2);

        bp::opstream os1(p2);
        bp::opstream os2(std::move(p2));

        os2.pipe(os1.pipe());

        bp::pipe p3;
        is1 = p3;
        is2 = std::move(p3);

        bp::pipe p4_;
        bp::pipe p4 = std::move(p4_);

        bp::pipe p5;
        BOOST_REQUIRE_NO_THROW(p4_ != p4);
        BOOST_CHECK(p4_ != p4);

        BOOST_REQUIRE_NO_THROW(p5 != p4);
        BOOST_CHECK(p4 != p5);

        is1 = p4;
        is2 = std::move(p4);
    }
    {
        bp::wpipe p;
        bp::wpstream ws1(p);
        bp::wpstream ws2(std::move(p));

        ws2.pipe(std::move(ws1.pipe()));

        bp::wpipe p2;

        ws1 = p2;
        ws2 = std::move(p2);

        const bp::wpstream & ws2c = ws2;
        ws1.pipe(ws2c.pipe());
    }

    {
        bp::wpipe p;
        bp::wpipebuf ws1(p);
        bp::wpipebuf ws2(std::move(p));

        ws2.pipe(std::move(ws1.pipe()));

        bp::wpipe p2;

        ws1 = p2;
        ws2 = std::move(p2);

        const bp::wpipebuf & ws2c = ws2;
        ws1.pipe(ws2c.pipe());

    }
}

