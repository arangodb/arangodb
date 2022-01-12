//
// random_access_file.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
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
#include <boost/asio/random_access_file.hpp>

#include "archetypes/async_result.hpp"
#include <boost/asio/io_context.hpp>
#include "unit_test.hpp"

// random_access_file_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// random_access_file compile and link correctly. Runtime failures are ignored.

namespace random_access_file_compile {

struct write_some_at_handler
{
  write_some_at_handler() {}
  void operator()(const boost::system::error_code&, std::size_t) {}
#if defined(BOOST_ASIO_HAS_MOVE)
  write_some_at_handler(write_some_at_handler&&) {}
private:
  write_some_at_handler(const write_some_at_handler&);
#endif // defined(BOOST_ASIO_HAS_MOVE)
};

struct read_some_at_handler
{
  read_some_at_handler() {}
  void operator()(const boost::system::error_code&, std::size_t) {}
#if defined(BOOST_ASIO_HAS_MOVE)
  read_some_at_handler(read_some_at_handler&&) {}
private:
  read_some_at_handler(const read_some_at_handler&);
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

    // basic_random_access_file constructors.

    random_access_file file1(ioc);
    random_access_file file2(ioc, "", random_access_file::read_only);
    random_access_file file3(ioc, path, random_access_file::read_only);
    random_access_file::native_handle_type native_file1 = file1.native_handle();
    random_access_file file4(ioc, native_file1);

    random_access_file file5(ioc_ex);
    random_access_file file6(ioc_ex, "", random_access_file::read_only);
    random_access_file file7(ioc_ex, path, random_access_file::read_only);
    random_access_file::native_handle_type native_file2 = file1.native_handle();
    random_access_file file8(ioc_ex, native_file2);

#if defined(BOOST_ASIO_HAS_MOVE)
    random_access_file file9(std::move(file8));
#endif // defined(BOOST_ASIO_HAS_MOVE)

    // basic_random_access_file operators.

#if defined(BOOST_ASIO_HAS_MOVE)
    file1 = random_access_file(ioc);
    file1 = std::move(file2);
#endif // defined(BOOST_ASIO_HAS_MOVE)

    // basic_io_object functions.

    random_access_file::executor_type ex = file1.get_executor();
    (void)ex;

    // basic_random_access_file functions.

    file1.open("", random_access_file::read_only);
    file1.open("", random_access_file::read_only, ec);

    file1.open(path, random_access_file::read_only);
    file1.open(path, random_access_file::read_only, ec);

    random_access_file::native_handle_type native_file3 = file1.native_handle();
    file1.assign(native_file3);
    random_access_file::native_handle_type native_file4 = file1.native_handle();
    file1.assign(native_file4, ec);

    bool is_open = file1.is_open();
    (void)is_open;

    file1.close();
    file1.close(ec);

    random_access_file::native_handle_type native_file5 = file1.native_handle();
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

    file1.write_some_at(0, buffer(mutable_char_buffer));
    file1.write_some_at(0, buffer(const_char_buffer));
    file1.write_some_at(0, buffer(mutable_char_buffer), ec);
    file1.write_some_at(0, buffer(const_char_buffer), ec);

    file1.async_write_some_at(0, buffer(mutable_char_buffer),
        write_some_at_handler());
    file1.async_write_some_at(0, buffer(const_char_buffer),
        write_some_at_handler());
    int i1 = file1.async_write_some_at(0, buffer(mutable_char_buffer), lazy);
    (void)i1;
    int i2 = file1.async_write_some_at(0, buffer(const_char_buffer), lazy);
    (void)i2;

    file1.read_some_at(0, buffer(mutable_char_buffer));
    file1.read_some_at(0, buffer(mutable_char_buffer), ec);

    file1.async_read_some_at(0, buffer(mutable_char_buffer),
        read_some_at_handler());
    int i3 = file1.async_read_some_at(0, buffer(mutable_char_buffer), lazy);
    (void)i3;
  }
  catch (std::exception&)
  {
  }
#endif // defined(BOOST_ASIO_HAS_FILE)
}

} // namespace random_access_file_compile

BOOST_ASIO_TEST_SUITE
(
  "random_access_file",
  BOOST_ASIO_COMPILE_TEST_CASE(random_access_file_compile::test)
)
