//
// parallel_grep.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <fstream>
#include <iostream>
#include <string>

using boost::asio::dispatch;
using boost::asio::spawn;
using boost::asio::strand;
using boost::asio::thread_pool;
using boost::asio::yield_context;

int main(int argc, char* argv[])
{
  try
  {
    if (argc < 2)
    {
      std::cerr << "Usage: parallel_grep <string> <files...>\n";
      return 1;
    }

    // We use a fixed size pool of threads for reading the input files. The
    // number of threads is automatically determined based on the number of
    // CPUs available in the system.
    thread_pool pool;

    // To prevent the output from being garbled, we use a strand to synchronise
    // printing.
    strand<thread_pool::executor_type> output_strand(pool.get_executor());

    // Spawn a new coroutine for each file specified on the command line.
    std::string search_string = argv[1];
    for (int argn = 2; argn < argc; ++argn)
    {
      std::string input_file = argv[argn];
      spawn(pool,
        [=](yield_context yield)
        {
          std::ifstream is(input_file.c_str());
          std::string line;
          std::size_t line_num = 0;
          while (std::getline(is, line))
          {
            // If we find a match, send a message to the output.
            if (line.find(search_string) != std::string::npos)
            {
              dispatch(output_strand,
                  [=]
                  {
                    std::cout << input_file << ':' << line << std::endl;
                  });
            }

            // Every so often we yield control to another coroutine.
            if (++line_num % 10 == 0)
              post(yield);
          }
        });
    }

    // Join the thread pool to wait for all the spawned tasks to complete.
    pool.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
