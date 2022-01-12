//
// stream_file.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <boost/asio/stream_file.hpp>

#include "archetypes/async_result.hpp"
#include <boost/asio/io_context.hpp>
#include "unit_test.hpp"

// stream_file_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// stream_file compile and link correctly. Runtime failures are ignored.

namespace stream_file_compile {

struct write_some_handler
{
  write_some_handler() {}
  void operator()(const boost::system::error_code&, std::size_t) {}
#if defined(BOOST_ASIO_HAS_MOVE)
  write_some_handler(write_some_handler&&) {}
private:
  write_some_handler(const write_some_handler&);
#endif // defined(BOOST_ASIO_HAS_MOVE)
};

struct read_some_handler
{
  read_some_handler() {}
  void operator()(const boost::system::error_code&, std::size_t) {}
#if defined(BOOST_ASIO_HAS_MOVE)
  read_some_handler(read_some_handler&&) {}
private:
  read_some_handler(const read_some_handler&);
#endif // defined(BOOST_ASIO_HAS_MOVE)
};

void test()
{
#if defined(BOOST_ASIO_HAS_FILE)
  using namespace boost::asio;

  try
  {
    io_context ioc;
    const io_context::executor_type ioc_ex = ioc.get_executor();
    char mutable_char_buffer[128] = "";
    const char const_char_buffer[128] = "";
    archetypes::lazy_handler lazy;
    boost::system::error_code ec;
    const std::string path;

    // basic_stream_file constructors.

    stream_file file1(ioc);
    stream_file file2(ioc, "", stream_file::read_only);
    stream_file file3(ioc, path, stream_file::read_only);
    stream_file::native_handle_type native_file1 = file1.native_handle();
    stream_file file4(ioc, native_file1);

    stream_file file5(ioc_ex);
    stream_file file6(ioc_ex, "", stream_file::read_only);
    stream_file file7(ioc_ex, path, stream_file::read_only);
    stream_file::native_handle_type native_file2 = file1.native_handle();
    stream_file file8(ioc_ex, native_file2);

#if defined(BOOST_ASIO_HAS_MOVE)
    stream_file file9(std::move(file8));
#endif // defined(BOOST_ASIO_HAS_MOVE)

    // basic_stream_file operators.

#if defined(BOOST_ASIO_HAS_MOVE)
    file1 = stream_file(ioc);
    file1 = std::move(file2);
#endif // defined(BOOST_ASIO_HAS_MOVE)

    // basic_io_object functions.

    stream_file::executor_type ex = file1.get_executor();
    (void)ex;

    // basic_stream_file functions.

    file1.open("", stream_file::read_only);
    file1.open("", stream_file::read_only, ec);

    file1.open(path, stream_file::read_only);
    file1.open(path, stream_file::read_only, ec);

    stream_file::native_handle_type native_file3 = file1.native_handle();
    file1.assign(native_file3);
    stream_file::native_handle_type native_file4 = file1.native_handle();
    file1.assign(native_file4, ec);

    bool is_open = file1.is_open();
    (void)is_open;

    file1.close();
    file1.close(ec);

    stream_file::native_handle_type native_file5 = file1.native_handle();
    (void)native_file5;

    file1.cancel();
    file1.cancel(ec);

    boost::asio::uint64_t s1 = file1.size();
    (void)s1;
    boost::asio::uint64_t s2 = file1.size(ec);
    (void)s2;

    file1.resize(boost::asio::uint64_t(0));
    file1.resize(boost::asio::uint64_t(0), ec);

    file1.sync_all();
    file1.sync_all(ec);

    file1.sync_data();
    file1.sync_data(ec);

    boost::asio::uint64_t s3 = file1.seek(0, stream_file::seek_set);
    (void)s3;
    boost::asio::uint64_t s4 = file1.seek(0, stream_file::seek_set, ec);
    (void)s4;

    file1.write_some(buffer(mutable_char_buffer));
    file1.write_some(buffer(const_char_buffer));
    file1.write_some(buffer(mutable_char_buffer), ec);
    file1.write_some(buffer(const_char_buffer), ec);

    file1.async_write_some(buffer(mutable_char_buffer), write_some_handler());
    file1.async_write_some(buffer(const_char_buffer), write_some_handler());
    int i1 = file1.async_write_some(buffer(mutable_char_buffer), lazy);
    (void)i1;
    int i2 = file1.async_write_some(buffer(const_char_buffer), lazy);
    (void)i2;

    file1.read_some(buffer(mutable_char_buffer));
    file1.read_some(buffer(mutable_char_buffer), ec);

    file1.async_read_some(buffer(mutable_char_buffer), read_some_handler());
    int i3 = file1.async_read_some(buffer(mutable_char_buffer), lazy);
    (void)i3;
  }
  catch (std::exception&)
  {
  }
#endif // defined(BOOST_ASIO_HAS_FILE)
}

} // namespace stream_file_compile

BOOST_ASIO_TEST_SUITE
(
  "stream_file",
  BOOST_ASIO_COMPILE_TEST_CASE(stream_file_compile::test)
)
