// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>

#include <boost/process.hpp>
#include <boost/process/posix.hpp>

#include <boost/filesystem.hpp>

#include <system_error>


#include <string>
#include <sys/wait.h>
#include <errno.h>

namespace fs = boost::filesystem;
namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(bind_fd, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::pipe p;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test", "--posix-echo-one", "3", "hello",
        bp::posix::fd.bind(3, p.native_sink()),
        ec
    );
    BOOST_CHECK(!ec);


    bp::ipstream is(std::move(p));

    std::string s;
    is >> s;
    BOOST_CHECK_EQUAL(s, "hello");
}

BOOST_AUTO_TEST_CASE(bind_fds, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::pipe p1;
    bp::pipe p2;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test","--posix-echo-two","3","hello","99","bye",
        bp::posix::fd.bind(3,  p1.native_sink()),
        bp::posix::fd.bind(99, p2.native_sink()),
        ec
    );
    BOOST_CHECK(!ec);

    bp::ipstream is1(std::move(p1));
    bp::ipstream is2(std::move(p2));

    std::string s1;
    is1 >> s1;
    BOOST_CHECK_EQUAL(s1, "hello");

    std::string s2;
    is2 >> s2;
    BOOST_CHECK_EQUAL(s2, "bye");
}

BOOST_AUTO_TEST_CASE(execve_set_on_error, *boost::unit_test::timeout(2))
{
    std::error_code ec;
    bp::spawn(
        "doesnt-exist",
        ec
    );
    BOOST_CHECK(ec);
    BOOST_CHECK_EQUAL(ec.value(), ENOENT);
}

BOOST_AUTO_TEST_CASE(execve_throw_on_error, *boost::unit_test::timeout(2))
{
    try
    {
        bp::spawn("doesnt-exist");
        BOOST_CHECK(false);
    }
    catch (bp::process_error &e)
    {
        BOOST_CHECK(e.code());
        BOOST_CHECK_EQUAL(e.code().value(), ENOENT);
    }
}

BOOST_AUTO_TEST_CASE(leak_test, *boost::unit_test::timeout(5))
{
    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;

    const auto pid = boost::this_process::get_id();
    const auto  fd_path = fs::path("/proc") / std::to_string(pid) / "fd";

    auto get_fds = [&]{
        std::vector<int> fds;
        for (auto && fd : fs::directory_iterator(fd_path))
            fds.push_back(std::stoi(fd.path().filename().string()));
        return fds;
    };

    std::vector<int> fd_list = get_fds();
    if (fd_list.empty()) //then there's no /proc in the current linux distribution.
        return;


    BOOST_CHECK(std::find(fd_list.begin(), fd_list.end(), STDOUT_FILENO) != fd_list.end());
    BOOST_CHECK(std::find(fd_list.begin(), fd_list.end(),  STDIN_FILENO) != fd_list.end());
    BOOST_CHECK(std::find(fd_list.begin(), fd_list.end(), STDERR_FILENO) != fd_list.end());

    bp::pipe p; //should add two descriptors.

    auto fd_list_new = get_fds();

    BOOST_CHECK_EQUAL(fd_list_new.size(), fd_list.size() + 2);
    fd_list.push_back(p.native_source());
    fd_list.push_back(p.native_sink());


    BOOST_CHECK_EQUAL(
            bp::system(
                    master_test_suite().argv[1],
                    "test", "--exit-code", "123",  ec), 123);

    fd_list_new = get_fds();
    BOOST_CHECK_EQUAL(fd_list.size(), fd_list_new.size());


    const int native_source = p.native_source();
    BOOST_CHECK_EQUAL(
            bp::system(
                    master_test_suite().argv[1],
                    bp::std_in < p,
                    "test", "--exit-code", "123",  ec), 123);

    BOOST_CHECK(!ec);

   ////now, p.source should be closed, so we remove it from fd_list

   const auto itr = std::find(fd_list.begin(), fd_list.end(), native_source);
   if (itr != fd_list.end())
       fd_list.erase(itr);

    fd_list_new = get_fds();

    BOOST_CHECK_EQUAL(fd_list.size(), fd_list_new.size());

}