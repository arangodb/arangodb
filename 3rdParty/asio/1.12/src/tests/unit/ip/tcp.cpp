//
// tcp.cpp
// ~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Enable cancel() support on Windows.
#define ASIO_ENABLE_CANCELIO 1

// Test that header file is self-contained.
#include "asio/ip/tcp.hpp"

#include <cstring>
#include "asio/io_context.hpp"
#include "asio/read.hpp"
#include "asio/write.hpp"
#include "../unit_test.hpp"
#include "../archetypes/async_result.hpp"
#include "../archetypes/deprecated_async_result.hpp"
#include "../archetypes/gettable_socket_option.hpp"
#include "../archetypes/io_control_command.hpp"
#include "../archetypes/settable_socket_option.hpp"

#if defined(ASIO_HAS_BOOST_ARRAY)
# include <boost/array.hpp>
#else // defined(ASIO_HAS_BOOST_ARRAY)
# include <array>
#endif // defined(ASIO_HAS_BOOST_ARRAY)

#if defined(ASIO_HAS_BOOST_BIND)
# include <boost/bind.hpp>
#else // defined(ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(ASIO_HAS_BOOST_BIND)

//------------------------------------------------------------------------------

// ip_tcp_compile test
// ~~~~~~~~~~~~~~~~~~~
// The following test checks that all nested classes, enums and constants in
// ip::tcp compile and link correctly. Runtime failures are ignored.

namespace ip_tcp_compile {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_context ioc;
    ip::tcp::socket sock(ioc);

    // no_delay class.

    ip::tcp::no_delay no_delay1(true);
    sock.set_option(no_delay1);
    ip::tcp::no_delay no_delay2;
    sock.get_option(no_delay2);
    no_delay1 = true;
    (void)static_cast<bool>(no_delay1);
    (void)static_cast<bool>(!no_delay1);
    (void)static_cast<bool>(no_delay1.value());
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_tcp_compile

//------------------------------------------------------------------------------

// ip_tcp_runtime test
// ~~~~~~~~~~~~~~~~~~~
// The following test checks the runtime operation of the ip::tcp class.

namespace ip_tcp_runtime {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  io_context ioc;
  ip::tcp::socket sock(ioc, ip::tcp::v4());
  asio::error_code ec;

  // no_delay class.

  ip::tcp::no_delay no_delay1(true);
  ASIO_CHECK(no_delay1.value());
  ASIO_CHECK(static_cast<bool>(no_delay1));
  ASIO_CHECK(!!no_delay1);
  sock.set_option(no_delay1, ec);
  ASIO_CHECK(!ec);

  ip::tcp::no_delay no_delay2;
  sock.get_option(no_delay2, ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(no_delay2.value());
  ASIO_CHECK(static_cast<bool>(no_delay2));
  ASIO_CHECK(!!no_delay2);

  ip::tcp::no_delay no_delay3(false);
  ASIO_CHECK(!no_delay3.value());
  ASIO_CHECK(!static_cast<bool>(no_delay3));
  ASIO_CHECK(!no_delay3);
  sock.set_option(no_delay3, ec);
  ASIO_CHECK(!ec);

  ip::tcp::no_delay no_delay4;
  sock.get_option(no_delay4, ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(!no_delay4.value());
  ASIO_CHECK(!static_cast<bool>(no_delay4));
  ASIO_CHECK(!no_delay4);
}

} // namespace ip_tcp_runtime

//------------------------------------------------------------------------------

// ip_tcp_socket_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// ip::tcp::socket compile and link correctly. Runtime failures are ignored.

namespace ip_tcp_socket_compile {

struct connect_handler
{
  connect_handler() {}
  void operator()(const asio::error_code&) {}
#if defined(ASIO_HAS_MOVE)
  connect_handler(connect_handler&&) {}
private:
  connect_handler(const connect_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

struct wait_handler
{
  wait_handler() {}
  void operator()(const asio::error_code&) {}
#if defined(ASIO_HAS_MOVE)
  wait_handler(wait_handler&&) {}
private:
  wait_handler(const wait_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

struct send_handler
{
  send_handler() {}
  void operator()(const asio::error_code&, std::size_t) {}
#if defined(ASIO_HAS_MOVE)
  send_handler(send_handler&&) {}
private:
  send_handler(const send_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

struct receive_handler
{
  receive_handler() {}
  void operator()(const asio::error_code&, std::size_t) {}
#if defined(ASIO_HAS_MOVE)
  receive_handler(receive_handler&&) {}
private:
  receive_handler(const receive_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

struct write_some_handler
{
  write_some_handler() {}
  void operator()(const asio::error_code&, std::size_t) {}
#if defined(ASIO_HAS_MOVE)
  write_some_handler(write_some_handler&&) {}
private:
  write_some_handler(const write_some_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

struct read_some_handler
{
  read_some_handler() {}
  void operator()(const asio::error_code&, std::size_t) {}
#if defined(ASIO_HAS_MOVE)
  read_some_handler(read_some_handler&&) {}
private:
  read_some_handler(const read_some_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

void test()
{
#if defined(ASIO_HAS_BOOST_ARRAY)
  using boost::array;
#else // defined(ASIO_HAS_BOOST_ARRAY)
  using std::array;
#endif // defined(ASIO_HAS_BOOST_ARRAY)

  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_context ioc;
    char mutable_char_buffer[128] = "";
    const char const_char_buffer[128] = "";
    array<asio::mutable_buffer, 2> mutable_buffers = {{
        asio::buffer(mutable_char_buffer, 10),
        asio::buffer(mutable_char_buffer + 10, 10) }};
    array<asio::const_buffer, 2> const_buffers = {{
        asio::buffer(const_char_buffer, 10),
        asio::buffer(const_char_buffer + 10, 10) }};
    socket_base::message_flags in_flags = 0;
    archetypes::settable_socket_option<void> settable_socket_option1;
    archetypes::settable_socket_option<int> settable_socket_option2;
    archetypes::settable_socket_option<double> settable_socket_option3;
    archetypes::gettable_socket_option<void> gettable_socket_option1;
    archetypes::gettable_socket_option<int> gettable_socket_option2;
    archetypes::gettable_socket_option<double> gettable_socket_option3;
    archetypes::io_control_command io_control_command;
    archetypes::lazy_handler lazy;
#if !defined(ASIO_NO_DEPRECATED)
    archetypes::deprecated_lazy_handler dlazy;
#endif // !defined(ASIO_NO_DEPRECATED)
    asio::error_code ec;

    // basic_stream_socket constructors.

    ip::tcp::socket socket1(ioc);
    ip::tcp::socket socket2(ioc, ip::tcp::v4());
    ip::tcp::socket socket3(ioc, ip::tcp::v6());
    ip::tcp::socket socket4(ioc, ip::tcp::endpoint(ip::tcp::v4(), 0));
    ip::tcp::socket socket5(ioc, ip::tcp::endpoint(ip::tcp::v6(), 0));
#if !defined(ASIO_WINDOWS_RUNTIME)
    ip::tcp::socket::native_handle_type native_socket1
      = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ip::tcp::socket socket6(ioc, ip::tcp::v4(), native_socket1);
#endif // !defined(ASIO_WINDOWS_RUNTIME)

#if defined(ASIO_HAS_MOVE)
    ip::tcp::socket socket7(std::move(socket5));
#endif // defined(ASIO_HAS_MOVE)

    // basic_stream_socket operators.

#if defined(ASIO_HAS_MOVE)
    socket1 = ip::tcp::socket(ioc);
    socket1 = std::move(socket2);
#endif // defined(ASIO_HAS_MOVE)

    // basic_io_object functions.

#if !defined(ASIO_NO_DEPRECATED)
    io_context& ioc_ref = socket1.get_io_context();
    (void)ioc_ref;
#endif // !defined(ASIO_NO_DEPRECATED)

    ip::tcp::socket::executor_type ex = socket1.get_executor();
    (void)ex;

    // basic_socket functions.

    ip::tcp::socket::lowest_layer_type& lowest_layer = socket1.lowest_layer();
    (void)lowest_layer;

    const ip::tcp::socket& socket8 = socket1;
    const ip::tcp::socket::lowest_layer_type& lowest_layer2
      = socket8.lowest_layer();
    (void)lowest_layer2;

    socket1.open(ip::tcp::v4());
    socket1.open(ip::tcp::v6());
    socket1.open(ip::tcp::v4(), ec);
    socket1.open(ip::tcp::v6(), ec);

#if !defined(ASIO_WINDOWS_RUNTIME)
    ip::tcp::socket::native_handle_type native_socket2
      = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    socket1.assign(ip::tcp::v4(), native_socket2);
    ip::tcp::socket::native_handle_type native_socket3
      = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    socket1.assign(ip::tcp::v4(), native_socket3, ec);
#endif // !defined(ASIO_WINDOWS_RUNTIME)

    bool is_open = socket1.is_open();
    (void)is_open;

    socket1.close();
    socket1.close(ec);

    socket1.release();
    socket1.release(ec);

    ip::tcp::socket::native_handle_type native_socket4
      = socket1.native_handle();
    (void)native_socket4;

    socket1.cancel();
    socket1.cancel(ec);

    bool at_mark1 = socket1.at_mark();
    (void)at_mark1;
    bool at_mark2 = socket1.at_mark(ec);
    (void)at_mark2;

    std::size_t available1 = socket1.available();
    (void)available1;
    std::size_t available2 = socket1.available(ec);
    (void)available2;

    socket1.bind(ip::tcp::endpoint(ip::tcp::v4(), 0));
    socket1.bind(ip::tcp::endpoint(ip::tcp::v6(), 0));
    socket1.bind(ip::tcp::endpoint(ip::tcp::v4(), 0), ec);
    socket1.bind(ip::tcp::endpoint(ip::tcp::v6(), 0), ec);

    socket1.connect(ip::tcp::endpoint(ip::tcp::v4(), 0));
    socket1.connect(ip::tcp::endpoint(ip::tcp::v6(), 0));
    socket1.connect(ip::tcp::endpoint(ip::tcp::v4(), 0), ec);
    socket1.connect(ip::tcp::endpoint(ip::tcp::v6(), 0), ec);

    socket1.async_connect(ip::tcp::endpoint(ip::tcp::v4(), 0),
        connect_handler());
    socket1.async_connect(ip::tcp::endpoint(ip::tcp::v6(), 0),
        connect_handler());
    int i1 = socket1.async_connect(ip::tcp::endpoint(ip::tcp::v4(), 0), lazy);
    (void)i1;
    int i2 = socket1.async_connect(ip::tcp::endpoint(ip::tcp::v6(), 0), lazy);
    (void)i2;
#if !defined(ASIO_NO_DEPRECATED)
    double d1 = socket1.async_connect(
        ip::tcp::endpoint(ip::tcp::v4(), 0), dlazy);
    (void)d1;
    double d2 = socket1.async_connect(
        ip::tcp::endpoint(ip::tcp::v6(), 0), dlazy);
    (void)d2;
#endif // !defined(ASIO_NO_DEPRECATED)

    socket1.set_option(settable_socket_option1);
    socket1.set_option(settable_socket_option1, ec);
    socket1.set_option(settable_socket_option2);
    socket1.set_option(settable_socket_option2, ec);
    socket1.set_option(settable_socket_option3);
    socket1.set_option(settable_socket_option3, ec);

    socket1.get_option(gettable_socket_option1);
    socket1.get_option(gettable_socket_option1, ec);
    socket1.get_option(gettable_socket_option2);
    socket1.get_option(gettable_socket_option2, ec);
    socket1.get_option(gettable_socket_option3);
    socket1.get_option(gettable_socket_option3, ec);

    socket1.io_control(io_control_command);
    socket1.io_control(io_control_command, ec);

    bool non_blocking1 = socket1.non_blocking();
    (void)non_blocking1;
    socket1.non_blocking(true);
    socket1.non_blocking(false, ec);

    bool non_blocking2 = socket1.native_non_blocking();
    (void)non_blocking2;
    socket1.native_non_blocking(true);
    socket1.native_non_blocking(false, ec);

    ip::tcp::endpoint endpoint1 = socket1.local_endpoint();
    ip::tcp::endpoint endpoint2 = socket1.local_endpoint(ec);

    ip::tcp::endpoint endpoint3 = socket1.remote_endpoint();
    ip::tcp::endpoint endpoint4 = socket1.remote_endpoint(ec);

    socket1.shutdown(socket_base::shutdown_both);
    socket1.shutdown(socket_base::shutdown_both, ec);

    socket1.wait(socket_base::wait_read);
    socket1.wait(socket_base::wait_write, ec);

    socket1.async_wait(socket_base::wait_read, wait_handler());
    int i3 = socket1.async_wait(socket_base::wait_write, lazy);
    (void)i3;
#if !defined(ASIO_NO_DEPRECATED)
    double d3 = socket1.async_wait(socket_base::wait_write, dlazy);
    (void)d3;
#endif // !defined(ASIO_NO_DEPRECATED)

    // basic_stream_socket functions.

    socket1.send(buffer(mutable_char_buffer));
    socket1.send(buffer(const_char_buffer));
    socket1.send(mutable_buffers);
    socket1.send(const_buffers);
    socket1.send(null_buffers());
    socket1.send(buffer(mutable_char_buffer), in_flags);
    socket1.send(buffer(const_char_buffer), in_flags);
    socket1.send(mutable_buffers, in_flags);
    socket1.send(const_buffers, in_flags);
    socket1.send(null_buffers(), in_flags);
    socket1.send(buffer(mutable_char_buffer), in_flags, ec);
    socket1.send(buffer(const_char_buffer), in_flags, ec);
    socket1.send(mutable_buffers, in_flags, ec);
    socket1.send(const_buffers, in_flags, ec);
    socket1.send(null_buffers(), in_flags, ec);

    socket1.async_send(buffer(mutable_char_buffer), send_handler());
    socket1.async_send(buffer(const_char_buffer), send_handler());
    socket1.async_send(mutable_buffers, send_handler());
    socket1.async_send(const_buffers, send_handler());
    socket1.async_send(null_buffers(), send_handler());
    socket1.async_send(buffer(mutable_char_buffer), in_flags, send_handler());
    socket1.async_send(buffer(const_char_buffer), in_flags, send_handler());
    socket1.async_send(mutable_buffers, in_flags, send_handler());
    socket1.async_send(const_buffers, in_flags, send_handler());
    socket1.async_send(null_buffers(), in_flags, send_handler());
    int i4 = socket1.async_send(buffer(mutable_char_buffer), lazy);
    (void)i4;
    int i5 = socket1.async_send(buffer(const_char_buffer), lazy);
    (void)i5;
    int i6 = socket1.async_send(mutable_buffers, lazy);
    (void)i6;
    int i7 = socket1.async_send(const_buffers, lazy);
    (void)i7;
    int i8 = socket1.async_send(null_buffers(), lazy);
    (void)i8;
    int i9 = socket1.async_send(buffer(mutable_char_buffer), in_flags, lazy);
    (void)i9;
    int i10 = socket1.async_send(buffer(const_char_buffer), in_flags, lazy);
    (void)i10;
    int i11 = socket1.async_send(mutable_buffers, in_flags, lazy);
    (void)i11;
    int i12 = socket1.async_send(const_buffers, in_flags, lazy);
    (void)i12;
    int i13 = socket1.async_send(null_buffers(), in_flags, lazy);
    (void)i13;
#if !defined(ASIO_NO_DEPRECATED)
    double d4 = socket1.async_send(buffer(mutable_char_buffer), dlazy);
    (void)d4;
    double d5 = socket1.async_send(buffer(const_char_buffer), dlazy);
    (void)d5;
    double d6 = socket1.async_send(mutable_buffers, dlazy);
    (void)d6;
    double d7 = socket1.async_send(const_buffers, dlazy);
    (void)d7;
    double d8 = socket1.async_send(null_buffers(), dlazy);
    (void)d8;
    double d9 = socket1.async_send(
        buffer(mutable_char_buffer), in_flags, dlazy);
    (void)d9;
    double d10 = socket1.async_send(buffer(const_char_buffer), in_flags, dlazy);
    (void)d10;
    double d11 = socket1.async_send(mutable_buffers, in_flags, dlazy);
    (void)d11;
    double d12 = socket1.async_send(const_buffers, in_flags, dlazy);
    (void)d12;
    double d13 = socket1.async_send(null_buffers(), in_flags, dlazy);
    (void)d13;
#endif // !defined(ASIO_NO_DEPRECATED)

    socket1.receive(buffer(mutable_char_buffer));
    socket1.receive(mutable_buffers);
    socket1.receive(null_buffers());
    socket1.receive(buffer(mutable_char_buffer), in_flags);
    socket1.receive(mutable_buffers, in_flags);
    socket1.receive(null_buffers(), in_flags);
    socket1.receive(buffer(mutable_char_buffer), in_flags, ec);
    socket1.receive(mutable_buffers, in_flags, ec);
    socket1.receive(null_buffers(), in_flags, ec);

    socket1.async_receive(buffer(mutable_char_buffer), receive_handler());
    socket1.async_receive(mutable_buffers, receive_handler());
    socket1.async_receive(null_buffers(), receive_handler());
    socket1.async_receive(buffer(mutable_char_buffer), in_flags,
        receive_handler());
    socket1.async_receive(mutable_buffers, in_flags, receive_handler());
    socket1.async_receive(null_buffers(), in_flags, receive_handler());
    int i14 = socket1.async_receive(buffer(mutable_char_buffer), lazy);
    (void)i14;
    int i15 = socket1.async_receive(mutable_buffers, lazy);
    (void)i15;
    int i16 = socket1.async_receive(null_buffers(), lazy);
    (void)i16;
    int i17 = socket1.async_receive(buffer(mutable_char_buffer), in_flags,
        lazy);
    (void)i17;
    int i18 = socket1.async_receive(mutable_buffers, in_flags, lazy);
    (void)i18;
    int i19 = socket1.async_receive(null_buffers(), in_flags, lazy);
    (void)i19;
#if !defined(ASIO_NO_DEPRECATED)
    double d14 = socket1.async_receive(buffer(mutable_char_buffer), dlazy);
    (void)d14;
    double d15 = socket1.async_receive(mutable_buffers, dlazy);
    (void)d15;
    double d16 = socket1.async_receive(null_buffers(), dlazy);
    (void)d16;
    double d17 = socket1.async_receive(buffer(mutable_char_buffer), in_flags,
        dlazy);
    (void)d17;
    double d18 = socket1.async_receive(mutable_buffers, in_flags, dlazy);
    (void)d18;
    double d19 = socket1.async_receive(null_buffers(), in_flags, dlazy);
    (void)d19;
#endif // !defined(ASIO_NO_DEPRECATED)

    socket1.write_some(buffer(mutable_char_buffer));
    socket1.write_some(buffer(const_char_buffer));
    socket1.write_some(mutable_buffers);
    socket1.write_some(const_buffers);
    socket1.write_some(null_buffers());
    socket1.write_some(buffer(mutable_char_buffer), ec);
    socket1.write_some(buffer(const_char_buffer), ec);
    socket1.write_some(mutable_buffers, ec);
    socket1.write_some(const_buffers, ec);
    socket1.write_some(null_buffers(), ec);

    socket1.async_write_some(buffer(mutable_char_buffer), write_some_handler());
    socket1.async_write_some(buffer(const_char_buffer), write_some_handler());
    socket1.async_write_some(mutable_buffers, write_some_handler());
    socket1.async_write_some(const_buffers, write_some_handler());
    socket1.async_write_some(null_buffers(), write_some_handler());
    int i20 = socket1.async_write_some(buffer(mutable_char_buffer), lazy);
    (void)i20;
    int i21 = socket1.async_write_some(buffer(const_char_buffer), lazy);
    (void)i21;
    int i22 = socket1.async_write_some(mutable_buffers, lazy);
    (void)i22;
    int i23 = socket1.async_write_some(const_buffers, lazy);
    (void)i23;
    int i24 = socket1.async_write_some(null_buffers(), lazy);
    (void)i24;
#if !defined(ASIO_NO_DEPRECATED)
    double d20 = socket1.async_write_some(buffer(mutable_char_buffer), dlazy);
    (void)d20;
    double d21 = socket1.async_write_some(buffer(const_char_buffer), dlazy);
    (void)d21;
    double d22 = socket1.async_write_some(mutable_buffers, dlazy);
    (void)d22;
    double d23 = socket1.async_write_some(const_buffers, dlazy);
    (void)d23;
    double d24 = socket1.async_write_some(null_buffers(), dlazy);
    (void)d24;
#endif // !defined(ASIO_NO_DEPRECATED)

    socket1.read_some(buffer(mutable_char_buffer));
    socket1.read_some(mutable_buffers);
    socket1.read_some(null_buffers());
    socket1.read_some(buffer(mutable_char_buffer), ec);
    socket1.read_some(mutable_buffers, ec);
    socket1.read_some(null_buffers(), ec);

    socket1.async_read_some(buffer(mutable_char_buffer), read_some_handler());
    socket1.async_read_some(mutable_buffers, read_some_handler());
    socket1.async_read_some(null_buffers(), read_some_handler());
    int i25 = socket1.async_read_some(buffer(mutable_char_buffer), lazy);
    (void)i25;
    int i26 = socket1.async_read_some(mutable_buffers, lazy);
    (void)i26;
    int i27 = socket1.async_read_some(null_buffers(), lazy);
    (void)i27;
#if !defined(ASIO_NO_DEPRECATED)
    double d25 = socket1.async_read_some(buffer(mutable_char_buffer), dlazy);
    (void)d25;
    double d26 = socket1.async_read_some(mutable_buffers, dlazy);
    (void)d26;
    double d27 = socket1.async_read_some(null_buffers(), dlazy);
    (void)d27;
#endif // !defined(ASIO_NO_DEPRECATED)
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_tcp_socket_compile

//------------------------------------------------------------------------------

// ip_tcp_socket_runtime test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks the runtime operation of the ip::tcp::socket class.

namespace ip_tcp_socket_runtime {

static const char write_data[]
  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void handle_read_noop(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  ASIO_CHECK(!err);
  ASIO_CHECK(bytes_transferred == 0);
}

void handle_write_noop(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  ASIO_CHECK(!err);
  ASIO_CHECK(bytes_transferred == 0);
}

void handle_read(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  ASIO_CHECK(!err);
  ASIO_CHECK(bytes_transferred == sizeof(write_data));
}

void handle_write(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  ASIO_CHECK(!err);
  ASIO_CHECK(bytes_transferred == sizeof(write_data));
}

void handle_read_cancel(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  ASIO_CHECK(err == asio::error::operation_aborted);
  ASIO_CHECK(bytes_transferred == 0);
}

void handle_read_eof(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  ASIO_CHECK(err == asio::error::eof);
  ASIO_CHECK(bytes_transferred == 0);
}

void test()
{
  using namespace std; // For memcmp.
  using namespace asio;
  namespace ip = asio::ip;

#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

  io_context ioc;

  ip::tcp::acceptor acceptor(ioc, ip::tcp::endpoint(ip::tcp::v4(), 0));
  ip::tcp::endpoint server_endpoint = acceptor.local_endpoint();
  server_endpoint.address(ip::address_v4::loopback());

  ip::tcp::socket client_side_socket(ioc);
  ip::tcp::socket server_side_socket(ioc);

  client_side_socket.connect(server_endpoint);
  acceptor.accept(server_side_socket);

  // No-op read.

  bool read_noop_completed = false;
  client_side_socket.async_read_some(
      asio::mutable_buffer(0, 0),
      bindns::bind(handle_read_noop,
        _1, _2, &read_noop_completed));

  ioc.run();
  ASIO_CHECK(read_noop_completed);

  // No-op write.

  bool write_noop_completed = false;
  client_side_socket.async_write_some(
      asio::const_buffer(0, 0),
      bindns::bind(handle_write_noop,
        _1, _2, &write_noop_completed));

  ioc.restart();
  ioc.run();
  ASIO_CHECK(write_noop_completed);

  // Read and write to transfer data.

  char read_buffer[sizeof(write_data)];
  bool read_completed = false;
  asio::async_read(client_side_socket,
      asio::buffer(read_buffer),
      bindns::bind(handle_read,
        _1, _2, &read_completed));

  bool write_completed = false;
  asio::async_write(server_side_socket,
      asio::buffer(write_data),
      bindns::bind(handle_write,
        _1, _2, &write_completed));

  ioc.restart();
  ioc.run();
  ASIO_CHECK(read_completed);
  ASIO_CHECK(write_completed);
  ASIO_CHECK(memcmp(read_buffer, write_data, sizeof(write_data)) == 0);

  // Cancelled read.

  bool read_cancel_completed = false;
  asio::async_read(server_side_socket,
      asio::buffer(read_buffer),
      bindns::bind(handle_read_cancel,
        _1, _2, &read_cancel_completed));

  ioc.restart();
  ioc.poll();
  ASIO_CHECK(!read_cancel_completed);

  server_side_socket.cancel();

  ioc.restart();
  ioc.run();
  ASIO_CHECK(read_cancel_completed);

  // A read when the peer closes socket should fail with eof.

  bool read_eof_completed = false;
  asio::async_read(client_side_socket,
      asio::buffer(read_buffer),
      bindns::bind(handle_read_eof,
        _1, _2, &read_eof_completed));

  server_side_socket.close();

  ioc.restart();
  ioc.run();
  ASIO_CHECK(read_eof_completed);
}

} // namespace ip_tcp_socket_runtime

//------------------------------------------------------------------------------

// ip_tcp_acceptor_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// ip::tcp::acceptor compile and link correctly. Runtime failures are ignored.

namespace ip_tcp_acceptor_compile {

struct wait_handler
{
  wait_handler() {}
  void operator()(const asio::error_code&) {}
#if defined(ASIO_HAS_MOVE)
  wait_handler(wait_handler&&) {}
private:
  wait_handler(const wait_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

struct accept_handler
{
  accept_handler() {}
  void operator()(const asio::error_code&) {}
#if defined(ASIO_HAS_MOVE)
  accept_handler(accept_handler&&) {}
private:
  accept_handler(const accept_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

#if defined(ASIO_HAS_MOVE)
struct move_accept_handler
{
  move_accept_handler() {}
  void operator()(
      const asio::error_code&, asio::ip::tcp::socket) {}
  move_accept_handler(move_accept_handler&&) {}
private:
  move_accept_handler(const move_accept_handler&) {}
};
#endif // defined(ASIO_HAS_MOVE)

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_context ioc;
    ip::tcp::socket peer_socket(ioc);
    ip::tcp::endpoint peer_endpoint;
    archetypes::settable_socket_option<void> settable_socket_option1;
    archetypes::settable_socket_option<int> settable_socket_option2;
    archetypes::settable_socket_option<double> settable_socket_option3;
    archetypes::gettable_socket_option<void> gettable_socket_option1;
    archetypes::gettable_socket_option<int> gettable_socket_option2;
    archetypes::gettable_socket_option<double> gettable_socket_option3;
    archetypes::io_control_command io_control_command;
    archetypes::lazy_handler lazy;
#if !defined(ASIO_NO_DEPRECATED)
    archetypes::deprecated_lazy_handler dlazy;
#endif // !defined(ASIO_NO_DEPRECATED)
    asio::error_code ec;

    // basic_socket_acceptor constructors.

    ip::tcp::acceptor acceptor1(ioc);
    ip::tcp::acceptor acceptor2(ioc, ip::tcp::v4());
    ip::tcp::acceptor acceptor3(ioc, ip::tcp::v6());
    ip::tcp::acceptor acceptor4(ioc, ip::tcp::endpoint(ip::tcp::v4(), 0));
    ip::tcp::acceptor acceptor5(ioc, ip::tcp::endpoint(ip::tcp::v6(), 0));
#if !defined(ASIO_WINDOWS_RUNTIME)
    ip::tcp::acceptor::native_handle_type native_acceptor1
      = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ip::tcp::acceptor acceptor6(ioc, ip::tcp::v4(), native_acceptor1);
#endif // !defined(ASIO_WINDOWS_RUNTIME)

#if defined(ASIO_HAS_MOVE)
    ip::tcp::acceptor acceptor7(std::move(acceptor5));
#endif // defined(ASIO_HAS_MOVE)

    // basic_socket_acceptor operators.

#if defined(ASIO_HAS_MOVE)
    acceptor1 = ip::tcp::acceptor(ioc);
    acceptor1 = std::move(acceptor2);
#endif // defined(ASIO_HAS_MOVE)

    // basic_io_object functions.

#if !defined(ASIO_NO_DEPRECATED)
    io_context& ioc_ref = acceptor1.get_io_context();
    (void)ioc_ref;
#endif // !defined(ASIO_NO_DEPRECATED)

    ip::tcp::acceptor::executor_type ex = acceptor1.get_executor();
    (void)ex;

    // basic_socket_acceptor functions.

    acceptor1.open(ip::tcp::v4());
    acceptor1.open(ip::tcp::v6());
    acceptor1.open(ip::tcp::v4(), ec);
    acceptor1.open(ip::tcp::v6(), ec);

#if !defined(ASIO_WINDOWS_RUNTIME)
    ip::tcp::acceptor::native_handle_type native_acceptor2
      = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    acceptor1.assign(ip::tcp::v4(), native_acceptor2);
    ip::tcp::acceptor::native_handle_type native_acceptor3
      = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    acceptor1.assign(ip::tcp::v4(), native_acceptor3, ec);
#endif // !defined(ASIO_WINDOWS_RUNTIME)

    bool is_open = acceptor1.is_open();
    (void)is_open;

    acceptor1.close();
    acceptor1.close(ec);

    acceptor1.release();
    acceptor1.release(ec);

    ip::tcp::acceptor::native_handle_type native_acceptor4
      = acceptor1.native_handle();
    (void)native_acceptor4;

    acceptor1.cancel();
    acceptor1.cancel(ec);

    acceptor1.bind(ip::tcp::endpoint(ip::tcp::v4(), 0));
    acceptor1.bind(ip::tcp::endpoint(ip::tcp::v6(), 0));
    acceptor1.bind(ip::tcp::endpoint(ip::tcp::v4(), 0), ec);
    acceptor1.bind(ip::tcp::endpoint(ip::tcp::v6(), 0), ec);

    acceptor1.set_option(settable_socket_option1);
    acceptor1.set_option(settable_socket_option1, ec);
    acceptor1.set_option(settable_socket_option2);
    acceptor1.set_option(settable_socket_option2, ec);
    acceptor1.set_option(settable_socket_option3);
    acceptor1.set_option(settable_socket_option3, ec);

    acceptor1.get_option(gettable_socket_option1);
    acceptor1.get_option(gettable_socket_option1, ec);
    acceptor1.get_option(gettable_socket_option2);
    acceptor1.get_option(gettable_socket_option2, ec);
    acceptor1.get_option(gettable_socket_option3);
    acceptor1.get_option(gettable_socket_option3, ec);

    acceptor1.io_control(io_control_command);
    acceptor1.io_control(io_control_command, ec);

    bool non_blocking1 = acceptor1.non_blocking();
    (void)non_blocking1;
    acceptor1.non_blocking(true);
    acceptor1.non_blocking(false, ec);

    bool non_blocking2 = acceptor1.native_non_blocking();
    (void)non_blocking2;
    acceptor1.native_non_blocking(true);
    acceptor1.native_non_blocking(false, ec);

    ip::tcp::endpoint endpoint1 = acceptor1.local_endpoint();
    ip::tcp::endpoint endpoint2 = acceptor1.local_endpoint(ec);

    acceptor1.wait(socket_base::wait_read);
    acceptor1.wait(socket_base::wait_write, ec);

    acceptor1.async_wait(socket_base::wait_read, wait_handler());
    int i1 = acceptor1.async_wait(socket_base::wait_write, lazy);
    (void)i1;
#if !defined(ASIO_NO_DEPRECATED)
    double d1 = acceptor1.async_wait(socket_base::wait_write, dlazy);
    (void)d1;
#endif // !defined(ASIO_NO_DEPRECATED)

    acceptor1.accept(peer_socket);
    acceptor1.accept(peer_socket, ec);
    acceptor1.accept(peer_socket, peer_endpoint);
    acceptor1.accept(peer_socket, peer_endpoint, ec);

#if defined(ASIO_HAS_MOVE)
    peer_socket = acceptor1.accept();
    peer_socket = acceptor1.accept(ioc);
    peer_socket = acceptor1.accept(peer_endpoint);
    peer_socket = acceptor1.accept(ioc, peer_endpoint);
    (void)peer_socket;
#endif // defined(ASIO_HAS_MOVE)

    acceptor1.async_accept(peer_socket, accept_handler());
    acceptor1.async_accept(peer_socket, peer_endpoint, accept_handler());
    int i2 = acceptor1.async_accept(peer_socket, lazy);
    (void)i2;
    int i3 = acceptor1.async_accept(peer_socket, peer_endpoint, lazy);
    (void)i3;
#if !defined(ASIO_NO_DEPRECATED)
    double d2 = acceptor1.async_accept(peer_socket, dlazy);
    (void)d2;
    double d3 = acceptor1.async_accept(peer_socket, peer_endpoint, dlazy);
    (void)d3;
#endif // !defined(ASIO_NO_DEPRECATED)

#if defined(ASIO_HAS_MOVE)
    acceptor1.async_accept(move_accept_handler());
    acceptor1.async_accept(ioc, move_accept_handler());
    acceptor1.async_accept(peer_endpoint, move_accept_handler());
    acceptor1.async_accept(ioc, peer_endpoint, move_accept_handler());
#endif // defined(ASIO_HAS_MOVE)
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_tcp_acceptor_compile

//------------------------------------------------------------------------------

// ip_tcp_acceptor_runtime test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks the runtime operation of the ip::tcp::acceptor
// class.

namespace ip_tcp_acceptor_runtime {

void handle_accept(const asio::error_code& err)
{
  ASIO_CHECK(!err);
}

void handle_connect(const asio::error_code& err)
{
  ASIO_CHECK(!err);
}

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  io_context ioc;

  ip::tcp::acceptor acceptor(ioc, ip::tcp::endpoint(ip::tcp::v4(), 0));
  ip::tcp::endpoint server_endpoint = acceptor.local_endpoint();
  server_endpoint.address(ip::address_v4::loopback());

  ip::tcp::socket client_side_socket(ioc);
  ip::tcp::socket server_side_socket(ioc);

  client_side_socket.connect(server_endpoint);
  acceptor.accept(server_side_socket);

  client_side_socket.close();
  server_side_socket.close();

  client_side_socket.connect(server_endpoint);
  ip::tcp::endpoint client_endpoint;
  acceptor.accept(server_side_socket, client_endpoint);

  ip::tcp::endpoint client_side_local_endpoint
    = client_side_socket.local_endpoint();
  ASIO_CHECK(client_side_local_endpoint.port() == client_endpoint.port());

  ip::tcp::endpoint server_side_remote_endpoint
    = server_side_socket.remote_endpoint();
  ASIO_CHECK(server_side_remote_endpoint.port()
      == client_endpoint.port());

  client_side_socket.close();
  server_side_socket.close();

  acceptor.async_accept(server_side_socket, &handle_accept);
  client_side_socket.async_connect(server_endpoint, &handle_connect);

  ioc.run();

  client_side_socket.close();
  server_side_socket.close();

  acceptor.async_accept(server_side_socket, client_endpoint, &handle_accept);
  client_side_socket.async_connect(server_endpoint, &handle_connect);

  ioc.restart();
  ioc.run();

  client_side_local_endpoint = client_side_socket.local_endpoint();
  ASIO_CHECK(client_side_local_endpoint.port() == client_endpoint.port());

  server_side_remote_endpoint = server_side_socket.remote_endpoint();
  ASIO_CHECK(server_side_remote_endpoint.port()
      == client_endpoint.port());
}

} // namespace ip_tcp_acceptor_runtime

//------------------------------------------------------------------------------

// ip_tcp_resolver_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// ip::tcp::resolver compile and link correctly. Runtime failures are ignored.

namespace ip_tcp_resolver_compile {

struct resolve_handler
{
  resolve_handler() {}
  void operator()(const asio::error_code&,
      asio::ip::tcp::resolver::results_type) {}
#if defined(ASIO_HAS_MOVE)
  resolve_handler(resolve_handler&&) {}
private:
  resolve_handler(const resolve_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

struct legacy_resolve_handler
{
  legacy_resolve_handler() {}
  void operator()(const asio::error_code&,
      asio::ip::tcp::resolver::iterator) {}
#if defined(ASIO_HAS_MOVE)
  legacy_resolve_handler(legacy_resolve_handler&&) {}
private:
  legacy_resolve_handler(const legacy_resolve_handler&);
#endif // defined(ASIO_HAS_MOVE)
};

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_context ioc;
    archetypes::lazy_handler lazy;
#if !defined(ASIO_NO_DEPRECATED)
    archetypes::deprecated_lazy_handler dlazy;
#endif // !defined(ASIO_NO_DEPRECATED)
    asio::error_code ec;
#if !defined(ASIO_NO_DEPRECATED)
    ip::tcp::resolver::query q(ip::tcp::v4(), "localhost", "0");
#endif // !defined(ASIO_NO_DEPRECATED)
    ip::tcp::endpoint e(ip::address_v4::loopback(), 0);

    // basic_resolver constructors.

    ip::tcp::resolver resolver(ioc);

#if defined(ASIO_HAS_MOVE)
    ip::tcp::resolver resolver2(std::move(resolver));
#endif // defined(ASIO_HAS_MOVE)

    // basic_resolver operators.

#if defined(ASIO_HAS_MOVE)
    resolver = ip::tcp::resolver(ioc);
    resolver = std::move(resolver2);
#endif // defined(ASIO_HAS_MOVE)

    // basic_io_object functions.

#if !defined(ASIO_NO_DEPRECATED)
    io_context& ioc_ref = resolver.get_io_context();
    (void)ioc_ref;
#endif // !defined(ASIO_NO_DEPRECATED)

    ip::tcp::resolver::executor_type ex = resolver.get_executor();
    (void)ex;

    // basic_resolver functions.

    resolver.cancel();

#if !defined(ASIO_NO_DEPRECATED)
    ip::tcp::resolver::results_type results1 = resolver.resolve(q);
    (void)results1;

    ip::tcp::resolver::results_type results2 = resolver.resolve(q, ec);
    (void)results2;
#endif // !defined(ASIO_NO_DEPRECATED)

    ip::tcp::resolver::results_type results3 = resolver.resolve("", "");
    (void)results3;

    ip::tcp::resolver::results_type results4 = resolver.resolve("", "", ec);
    (void)results4;

    ip::tcp::resolver::results_type results5 =
      resolver.resolve("", "", ip::tcp::resolver::flags());
    (void)results5;

    ip::tcp::resolver::results_type results6 =
      resolver.resolve("", "", ip::tcp::resolver::flags(), ec);
    (void)results6;

    ip::tcp::resolver::results_type results7 =
      resolver.resolve(ip::tcp::v4(), "", "");
    (void)results7;

    ip::tcp::resolver::results_type results8 =
      resolver.resolve(ip::tcp::v4(), "", "", ec);
    (void)results8;

    ip::tcp::resolver::results_type results9 =
      resolver.resolve(ip::tcp::v4(), "", "", ip::tcp::resolver::flags());
    (void)results9;

    ip::tcp::resolver::results_type results10 =
      resolver.resolve(ip::tcp::v4(), "", "", ip::tcp::resolver::flags(), ec);
    (void)results10;

    ip::tcp::resolver::results_type results11 = resolver.resolve(e);
    (void)results11;

    ip::tcp::resolver::results_type results12 = resolver.resolve(e, ec);
    (void)results12;

#if !defined(ASIO_NO_DEPRECATED)
    resolver.async_resolve(q, resolve_handler());
    resolver.async_resolve(q, legacy_resolve_handler());
    int i1 = resolver.async_resolve(q, lazy);
    (void)i1;
    double d1 = resolver.async_resolve(q, dlazy);
    (void)d1;
#endif // !defined(ASIO_NO_DEPRECATED)

    resolver.async_resolve("", "", resolve_handler());
    resolver.async_resolve("", "", legacy_resolve_handler());
    int i2 = resolver.async_resolve("", "", lazy);
    (void)i2;
#if !defined(ASIO_NO_DEPRECATED)
    double d2 = resolver.async_resolve("", "", dlazy);
    (void)d2;
#endif // !defined(ASIO_NO_DEPRECATED)

    resolver.async_resolve("", "",
        ip::tcp::resolver::flags(), resolve_handler());
    resolver.async_resolve("", "",
        ip::tcp::resolver::flags(), legacy_resolve_handler());
    int i3 = resolver.async_resolve("", "",
        ip::tcp::resolver::flags(), lazy);
    (void)i3;
#if !defined(ASIO_NO_DEPRECATED)
    double d3 = resolver.async_resolve("", "",
        ip::tcp::resolver::flags(), dlazy);
    (void)d3;
#endif // !defined(ASIO_NO_DEPRECATED)

    resolver.async_resolve(ip::tcp::v4(), "", "", resolve_handler());
    resolver.async_resolve(ip::tcp::v4(), "", "", legacy_resolve_handler());
    int i4 = resolver.async_resolve(ip::tcp::v4(), "", "", lazy);
    (void)i4;
#if !defined(ASIO_NO_DEPRECATED)
    double d4 = resolver.async_resolve(ip::tcp::v4(), "", "", dlazy);
    (void)d4;
#endif // !defined(ASIO_NO_DEPRECATED)

    resolver.async_resolve(ip::tcp::v4(),
        "", "", ip::tcp::resolver::flags(), resolve_handler());
    resolver.async_resolve(ip::tcp::v4(),
        "", "", ip::tcp::resolver::flags(), legacy_resolve_handler());
    int i5 = resolver.async_resolve(ip::tcp::v4(),
        "", "", ip::tcp::resolver::flags(), lazy);
    (void)i5;
#if !defined(ASIO_NO_DEPRECATED)
    double d5 = resolver.async_resolve(ip::tcp::v4(),
        "", "", ip::tcp::resolver::flags(), dlazy);
    (void)d5;
#endif // !defined(ASIO_NO_DEPRECATED)

    resolver.async_resolve(e, resolve_handler());
    resolver.async_resolve(e, legacy_resolve_handler());
    int i6 = resolver.async_resolve(e, lazy);
    (void)i6;
#if !defined(ASIO_NO_DEPRECATED)
    double d6 = resolver.async_resolve(e, dlazy);
    (void)d6;
#endif // !defined(ASIO_NO_DEPRECATED)
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_tcp_resolver_compile

//------------------------------------------------------------------------------

// ip_tcp_resolver_entry_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// ip::tcp::resolver::entry compile and link correctly. Runtime failures are
// ignored.

namespace ip_tcp_resolver_entry_compile {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;
  const ip::tcp::endpoint endpoint;
  const std::string host_name;
  const std::string service_name;
  const std::allocator<char> alloc;

  try
  {
    // basic_resolver_entry constructors.

    const ip::basic_resolver_entry<ip::tcp> entry1;
    ip::basic_resolver_entry<ip::tcp> entry2(endpoint, host_name, service_name);
    ip::basic_resolver_entry<ip::tcp> entry3(entry1);
#if defined(ASIO_HAS_MOVE)
    ip::basic_resolver_entry<ip::tcp> entry4(std::move(entry2));
#endif // defined(ASIO_HAS_MOVE)

    // basic_resolver_entry functions.

    ip::tcp::endpoint e1 = entry1.endpoint();
    (void)e1;

    ip::tcp::endpoint e2 = entry1;
    (void)e2;

    std::string s1 = entry1.host_name();
    (void)s1;

    std::string s2 = entry1.host_name(alloc);
    (void)s2;

    std::string s3 = entry1.service_name();
    (void)s3;

    std::string s4 = entry1.service_name(alloc);
    (void)s4;
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_tcp_resolver_entry_compile

//------------------------------------------------------------------------------

// ip_tcp_iostream_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public types and member functions on the
// class ip::tcp::iostream compile and link correctly. Runtime failures are
// ignored.

namespace ip_tcp_iostream_compile {

void test()
{
#if !defined(ASIO_NO_IOSTREAM)
  using namespace asio;
  namespace ip = asio::ip;

  asio::io_context ioc;
  asio::ip::tcp::socket sock(ioc);

  // basic_socket_iostream typedefs.

  (void)static_cast<ip::tcp::iostream::protocol_type*>(0);
  (void)static_cast<ip::tcp::iostream::endpoint_type*>(0);
  (void)static_cast<ip::tcp::iostream::clock_type*>(0);
  (void)static_cast<ip::tcp::iostream::time_point*>(0);
  (void)static_cast<ip::tcp::iostream::duration*>(0);
  (void)static_cast<ip::tcp::iostream::traits_type*>(0);

  // basic_socket_iostream constructors.

  ip::tcp::iostream ios1;

#if defined(ASIO_HAS_STD_IOSTREAM_MOVE)
  ip::tcp::iostream ios2(std::move(sock));
#endif // defined(ASIO_HAS_STD_IOSTREAM_MOVE)

  ip::tcp::iostream ios3("hostname", "service");

  // basic_socket_iostream operators.

#if defined(ASIO_HAS_STD_IOSTREAM_MOVE)
  ios1 = ip::tcp::iostream();

  ios2 = std::move(ios1);
#endif // defined(ASIO_HAS_STD_IOSTREAM_MOVE)

  // basic_socket_iostream members.

  ios1.connect("hostname", "service");

  ios1.close();

  (void)static_cast<std::streambuf*>(ios1.rdbuf());

#if defined(ASIO_ENABLE_OLD_SERVICES)
  basic_socket<ip::tcp, stream_socket_service<ip::tcp> >& sref = ios1.socket();
#else // defined(ASIO_ENABLE_OLD_SERVICES)
  basic_socket<ip::tcp>& sref = ios1.socket();
#endif // defined(ASIO_ENABLE_OLD_SERVICES)
  (void)sref;

  asio::error_code ec = ios1.error();
  (void)ec;

  ip::tcp::iostream::time_point tp = ios1.expiry();
  (void)tp;

  ios1.expires_at(tp);

  ip::tcp::iostream::duration d;
  ios1.expires_after(d);

  // iostream operators.

  int i = 0;
  ios1 >> i;
  ios1 << i;
#endif // !defined(ASIO_NO_IOSTREAM)
}

} // namespace ip_tcp_iostream_compile

//------------------------------------------------------------------------------

ASIO_TEST_SUITE
(
  "ip/tcp",
  ASIO_TEST_CASE(ip_tcp_compile::test)
  ASIO_TEST_CASE(ip_tcp_runtime::test)
  ASIO_TEST_CASE(ip_tcp_socket_compile::test)
  ASIO_TEST_CASE(ip_tcp_socket_runtime::test)
  ASIO_TEST_CASE(ip_tcp_acceptor_compile::test)
  ASIO_TEST_CASE(ip_tcp_acceptor_runtime::test)
  ASIO_TEST_CASE(ip_tcp_resolver_compile::test)
  ASIO_TEST_CASE(ip_tcp_resolver_entry_compile::test)
  ASIO_TEST_CASE(ip_tcp_resolver_entry_compile::test)
  ASIO_COMPILE_TEST_CASE(ip_tcp_iostream_compile::test)
)
