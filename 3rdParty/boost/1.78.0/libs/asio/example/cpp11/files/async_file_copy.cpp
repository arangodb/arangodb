//
// async_file_copy.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <boost/asio.hpp>

#if defined(BOOST_ASIO_HAS_FILE)

class file_copier
{
public:
  file_copier(boost::asio::io_context& io_context,
      const char* from, const char* to)
    : from_file_(io_context, from,
        boost::asio::stream_file::read_only),
      to_file_(io_context, to,
        boost::asio::stream_file::write_only
          | boost::asio::stream_file::create
          | boost::asio::stream_file::truncate)
  {
  }

  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    from_file_.async_read_some(boost::asio::buffer(data_),
        [this](boost::system::error_code error, std::size_t n)
        {
          if (!error)
          {
            do_write(n);
          }
          else if (error != boost::asio::error::eof)
          {
            std::cerr << "Error copying file: " << error.message() << "\n";
          }
        });
  }

  void do_write(std::size_t n)
  {
    boost::asio::async_write(to_file_, boost::asio::buffer(data_, n),
        [this](boost::system::error_code error, std::size_t /*n*/)
        {
          if (!error)
          {
            do_read();
          }
          else
          {
            std::cerr << "Error copying file: " << error.message() << "\n";
          }
        });
  }

  boost::asio::stream_file from_file_;
  boost::asio::stream_file to_file_;
  char data_[4096];
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: async_file_copy <from> <to>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    file_copier copier(io_context, argv[1], argv[2]);
    copier.start();

    io_context.run();
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
