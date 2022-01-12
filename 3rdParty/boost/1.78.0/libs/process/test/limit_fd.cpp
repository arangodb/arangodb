// Copyright (c) 2019 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>

#include <iostream>

#include <boost/process.hpp>
#include <boost/process/handles.hpp>
#include <boost/process/pipe.hpp>
#include <boost/process/io.hpp>
#include <boost/process/async_pipe.hpp>
#include <boost/process/extend.hpp>

#include <boost/filesystem.hpp>

#include <system_error>
#include <string>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

#if defined(BOOST_WINDOWS_API)
#include <boost/winapi/get_current_thread.hpp>
#include <boost/winapi/get_current_process.hpp>
#endif

namespace fs = boost::filesystem;
namespace bp = boost::process;
namespace bt = boost::this_process;

BOOST_AUTO_TEST_CASE(leak_test, *boost::unit_test::timeout(5))
{
    using boost::unit_test::framework::master_test_suite;

#if defined(BOOST_WINDOWS_API)
    const auto get_handle       = [](FILE * f)                       {return reinterpret_cast<bt::native_handle_type>(_get_osfhandle(_fileno(f)));};
    const auto socket_to_handle = [](::boost::winapi::UINT_PTR_ sock){return reinterpret_cast<::boost::winapi::HANDLE_>(sock);};
#else
    const auto get_handle = [](FILE * f) {return fileno(f);};
    const auto socket_to_handle = [](int i){ return i;};
#endif

    std::error_code ec;
    auto fd_list = bt::get_handles(ec);

    BOOST_CHECK_EQUAL(std::count(fd_list.begin(), fd_list.end(), get_handle(stdin)),  1);
    BOOST_CHECK_EQUAL(std::count(fd_list.begin(), fd_list.end(), get_handle(stdout)), 1);
    BOOST_CHECK_EQUAL(std::count(fd_list.begin(), fd_list.end(), get_handle(stderr)), 1);

    BOOST_CHECK(bt::is_stream_handle(get_handle(stdin),  ec));      BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK(bt::is_stream_handle(get_handle(stdout), ec));      BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK(bt::is_stream_handle(get_handle(stderr), ec));      BOOST_CHECK_MESSAGE(!ec, ec.message());


    BOOST_CHECK_GE(fd_list.size(), 3u);
    BOOST_CHECK_GE(bt::get_handles(ec).size(), fd_list.size());

    bp::pipe p;

    {

        auto fd_list_new = bt::get_handles(ec);
        BOOST_CHECK_MESSAGE(!ec, ec);
        BOOST_CHECK_LE(fd_list.size() + 2u, fd_list_new.size());
        fd_list = std::move(fd_list_new);
    }



    BOOST_CHECK_EQUAL(std::count(fd_list.begin(), fd_list.end(), p.native_source()), 1u);
    BOOST_CHECK_EQUAL(std::count(fd_list.begin(), fd_list.end(), p.native_sink()),   1u);

    BOOST_CHECK(bt::is_stream_handle(p.native_source(), ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK(bt::is_stream_handle(p.native_sink(),   ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());


    p.close();
    fd_list = bt::get_handles(ec);

    BOOST_CHECK_EQUAL(std::count(fd_list.begin(), fd_list.end(), p.native_source()), 0u);
    BOOST_CHECK_EQUAL(std::count(fd_list.begin(), fd_list.end(), p.native_sink()),   0u);

#if defined( BOOST_WINDOWS_API )
    std::thread thr([]{});
    BOOST_CHECK(!bt::is_stream_handle(thr.native_handle(), ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
    thr.join();
#else
# if defined(TFD_CLOEXEC) //check timer
    int timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    BOOST_CHECK(!bt::is_stream_handle(timer_fd , ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
#endif
# if defined(EFD_CLOEXEC) && defined(EFD_NONBLOCK)
    int event_fd =::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    BOOST_CHECK(!bt::is_stream_handle(event_fd , ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
#endif
    int dir_fd = ::dirfd(::opendir("."));
    BOOST_CHECK(!bt::is_stream_handle(dir_fd , ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
#endif


    boost::asio::io_context ioc;
    boost::asio::ip::tcp::socket tcp_socket(ioc);
    boost::asio::ip::udp::socket udp_socket(ioc);
    bp::async_pipe ap(ioc);

    tcp_socket.open(boost::asio::ip::tcp::v4());
    udp_socket.open(boost::asio::ip::udp::v4());

    BOOST_CHECK(bt::is_stream_handle(socket_to_handle(tcp_socket.native_handle()), ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK(bt::is_stream_handle(socket_to_handle(udp_socket.native_handle()), ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK(bt::is_stream_handle(std::move(ap).sink().  native_handle(), ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK(bt::is_stream_handle(std::move(ap).source().native_handle(), ec)); BOOST_CHECK_MESSAGE(!ec, ec.message());
}

struct on_setup_t
{
    std::vector<bt::native_handle_type> &res;

    on_setup_t(std::vector<bt::native_handle_type> & res) : res(res) {}
    template<typename Executor>
    void operator()(Executor & e)
    {
        bp::extend::foreach_used_handle(e, [this](bt::native_handle_type handle)
        {
            res.push_back(handle);
        });
    }
};

BOOST_AUTO_TEST_CASE(iterate_handles, *boost::unit_test::timeout(5))
{
    using boost::unit_test::framework::master_test_suite;

    std::vector<bt::native_handle_type> res;

    bp::pipe p_in;
    bp::pipe p_out;

    auto source = p_in.native_source();
    auto   sink = p_out.native_sink();
    std::error_code ec;

    BOOST_WARN_NE(source, sink); //Sanity check

    const auto ret = bp::system(master_test_suite().argv[1], "--exit-code" , "42",
                   bp::std_in  < p_out,
                   bp::std_out > p_in,
                   bp::extend::on_setup(on_setup_t(res)), ec);

    BOOST_CHECK_MESSAGE(!ec, ec.message());

    BOOST_CHECK_EQUAL(ret, 42u);
    BOOST_CHECK_EQUAL(std::count(res.begin(), res.end(), p_in. native_sink()), 0u);
    BOOST_CHECK_EQUAL(std::count(res.begin(), res.end(), p_out.native_source()), 0u);
}

BOOST_AUTO_TEST_CASE(limit_fd, *boost::unit_test::timeout(5))
{
#if defined(BOOST_WINDOWS_API)
    const auto get_handle = [](FILE * f){return std::to_string(_get_osfhandle(_fileno(f)));};
#else
    const auto get_handle = [](FILE * f){return std::to_string(fileno(f));};
#endif

    auto p = fopen("./test-file", "w");

    using boost::unit_test::framework::master_test_suite;

    BOOST_CHECK_EQUAL(bp::system(master_test_suite().argv[1], "--has-handle", bp::limit_handles, get_handle(p),  bp::std_in  < p), EXIT_SUCCESS);
    BOOST_CHECK_EQUAL(bp::system(master_test_suite().argv[1], "--has-handle", bp::limit_handles, get_handle(p),  bp::std_err > p), EXIT_SUCCESS);
    BOOST_CHECK_EQUAL(bp::system(master_test_suite().argv[1], "--has-handle", bp::limit_handles, get_handle(p),  bp::std_out > p), EXIT_SUCCESS);
    BOOST_CHECK_EQUAL(bp::system(master_test_suite().argv[1], "--has-handle", bp::limit_handles, get_handle(p)), EXIT_FAILURE);

    fclose(p);
}
