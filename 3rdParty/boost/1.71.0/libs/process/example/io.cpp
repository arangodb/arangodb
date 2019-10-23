// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/process.hpp>
#include <string>

namespace bp = boost::process;

int main()
{
    //
    bp::system(
        "test.exe",
        bp::std_out > stdout, //forward
        bp::std_err.close(), //close
        bp::std_in < bp::null //null in
    );

    boost::filesystem::path p = "input.txt";

    bp::system(
        "test.exe",
        (bp::std_out & bp::std_err) > "output.txt", //redirect both to one file
        bp::std_in < p //read input from file
    );

    {
        bp::opstream p1;
        bp::ipstream p2;
        bp::system(
            "test.exe",
            bp::std_out > p2,
            bp::std_in < p1
        );
        p1 << "my_text";
        int i = 0;
        p2 >> i;

    }
    {
        boost::asio::io_context io_context;
        bp::async_pipe p1(io_context);
        bp::async_pipe p2(io_context);
        bp::system(
            "test.exe",
            bp::std_out > p2,
            bp::std_in < p1,
            io_context,
            bp::on_exit([&](int exit, const std::error_code& ec_in)
                {
                    p1.async_close();
                    p2.async_close();
                })
        );
        std::vector<char> in_buf;
        std::string value = "my_string";
        boost::asio::async_write(p1, boost::asio::buffer(value),  []( const boost::system::error_code&, std::size_t){});
        boost::asio::async_read (p2, boost::asio::buffer(in_buf), []( const boost::system::error_code&, std::size_t){});
    }
    {
        boost::asio::io_context io_context;
        std::vector<char> in_buf;
        std::string value = "my_string";
        bp::system(
        "test.exe",
        bp::std_out > bp::buffer(in_buf),
        bp::std_in  < bp::buffer(value)
        );
    }

    {
        boost::asio::io_context io_context;
        std::future<std::vector<char>> in_buf;
        std::future<void> write_fut;
        std::string value = "my_string";
        bp::system(
            "test.exe",
            bp::std_out > in_buf,
            bp::std_in  < bp::buffer(value) > write_fut
            );

        write_fut.get();
        in_buf.get();
    }
}
