//
// blocking_file_copy.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <boost/asio.hpp>

#if defined(BOOST_ASIO_HAS_FILE)

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: blocking_file_copy <from> <to>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    boost::asio::stream_file from_file(io_context, argv[1],
        boost::asio::stream_file::read_only);

    boost::asio::stream_file to_file(io_context, argv[2],
        boost::asio::stream_file::write_only
          | boost::asio::stream_file::create
          | boost::asio::stream_file::truncate);

    char data[4096];
    boost::system::error_code error;
    for (;;)
    {
      std::size_t n = from_file.read_some(boost::asio::buffer(data), error);
      if (error)
        break;
      boost::asio::write(to_file, boost::asio::buffer(data, n), error);
      if (error)
        break;
    }

    if (error && error != boost::asio::error::eof)
    {
      std::cerr << "Error copying file: " << error.message() << "\n";
      return 1;
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}

#else // defined(BOOST_ASIO_HAS_FILE)
int main() {}
#endif // defined(BOOST_ASIO_HAS_FILE)
