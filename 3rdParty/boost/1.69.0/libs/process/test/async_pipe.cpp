// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#define BOOST_TEST_MAIN

#include <boost/test/included/unit_test.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/process/async_pipe.hpp>
#include <boost/process/pipe.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/streambuf.hpp>

using namespace std;
namespace bp = boost::process;
namespace asio = boost::asio;

BOOST_AUTO_TEST_CASE(plain_async, *boost::unit_test::timeout(5))
{
    asio::io_context ios;
    bp::async_pipe pipe{ios};

    std::string st = "test-string\n";

    asio::streambuf buf;

    asio::async_write(pipe, asio::buffer(st), [](const boost::system::error_code &, std::size_t){});
    asio::async_read_until(pipe, buf, '\n', [](const boost::system::error_code &, std::size_t){});

    ios.run();

    std::string line;
    std::istream istr(&buf);
    BOOST_CHECK(std::getline(istr, line));

    line.resize(11);
    BOOST_CHECK_EQUAL(line, "test-string");

}

BOOST_AUTO_TEST_CASE(closed_transform)
{
    asio::io_context ios;

    bp::async_pipe ap{ios};

    BOOST_CHECK(ap.is_open());
    bp::pipe p2 = static_cast<bp::pipe>(ap);
    BOOST_CHECK(p2.is_open());

    ap.close();
    BOOST_CHECK(!ap.is_open());

    bp::pipe p  = static_cast<bp::pipe>(ap);
    BOOST_CHECK(!p.is_open());

}

BOOST_AUTO_TEST_CASE(multithreaded_async_pipe)
{
    asio::io_context ioc;

    std::vector<std::thread> threads;
    for (int i = 0; i < std::thread::hardware_concurrency(); i++)
    {
        threads.emplace_back([&ioc]
        {
            std::vector<bp::async_pipe*> pipes;
            for (size_t i = 0; i < 100; i++)
                pipes.push_back(new bp::async_pipe(ioc));
            for (auto &p : pipes)
                delete p;
        });
    }
    for (auto &t : threads)
        t.join();
}
