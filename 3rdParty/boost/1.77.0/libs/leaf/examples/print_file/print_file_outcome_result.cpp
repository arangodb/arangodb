// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This is the program presented in
// https://boostorg.github.io/leaf/#introduction-result, converted to use
// outcome::result instead of leaf::result.

// It reads a text file in a buffer and prints it to std::cout, using LEAF to
// handle errors. This version does not use exception handling.

#include <boost/outcome/std_result.hpp>
#include <boost/leaf.hpp>
#include <iostream>
#include <memory>
#include <stdio.h>

namespace outcome = boost::outcome_v2;
namespace leaf = boost::leaf;


// First, we need an enum to define our error codes:
enum error_code
{
    bad_command_line = 1,
    open_error,
    read_error,
    size_error,
    eof_error,
    output_error
};


template <class T>
using result = outcome::std_result<T>;

// To enable LEAF to work with outcome::result, we need to specialize the
// is_result_type template:
namespace boost { namespace leaf {
    template <class T> struct is_result_type<outcome::std_result<T>>: std::true_type { };
} }


// We will handle all failures in our main function, but first, here are the
// declarations of the functions it calls, each communicating failures using
// result<T>:

// Parse the command line, return the file name.
result<char const *> parse_command_line( int argc, char const * argv[] );

// Open a file for reading.
result<std::shared_ptr<FILE>> file_open( char const * file_name );

// Return the size of the file.
result<int> file_size( FILE & f );

// Read size bytes from f into buf.
result<void> file_read( FILE & f, void * buf, int size );


// The main function, which handles all errors.
int main( int argc, char const * argv[] )
{
    return leaf::try_handle_all(

        [&]() -> result<int>
        {
            BOOST_LEAF_AUTO(file_name, parse_command_line(argc,argv));

            auto load = leaf::on_error( leaf::e_file_name{file_name} );

            BOOST_LEAF_AUTO(f, file_open(file_name));

            BOOST_LEAF_AUTO(s, file_size(*f));

            std::string buffer(1 + s, '\0');
            BOOST_LEAF_CHECK(file_read(*f, &buffer[0], buffer.size()-1));

            std::cout << buffer;
            std::cout.flush();
            if( std::cout.fail() )
                return leaf::new_error(output_error, leaf::e_errno{errno}).to_error_code();

            return 0;
        },

        // Each of the lambdas below is an error handler. LEAF will consider
        // them, in order, and call the first one that matches the available
        // error objects.

        // This handler will be called if the error includes:
        // - an object of type error_code equal to open_error, and
        // - an object of type leaf::e_errno that has .value equal to ENOENT,
        //   and
        // - an object of type leaf::e_file_name.
        []( leaf::match<error_code, open_error>, leaf::match_value<leaf::e_errno, ENOENT>, leaf::e_file_name const & fn )
        {
            std::cerr << "File not found: " << fn.value << std::endl;
            return 1;
        },

        // This handler will be called if the error includes:
        // - an object of type error_code equal to open_error, and
        // - an object of type leaf::e_errno (regardless of its .value), and
        // - an object of type leaf::e_file_name.
        []( leaf::match<error_code, open_error>, leaf::e_errno const & errn, leaf::e_file_name const & fn )
        {
            std::cerr << "Failed to open " << fn.value << ", errno=" << errn << std::endl;
            return 2;
        },

        // This handler will be called if the error includes:
        // - an object of type error_code equal to any of size_error,
        //   read_error, eof_error, and
        // - an optional object of type leaf::e_errno (regardless of its
        //   .value), and
        // - an object of type leaf::e_file_name.
        []( leaf::match<error_code, size_error, read_error, eof_error>, leaf::e_errno const * errn, leaf::e_file_name const & fn )
        {
            std::cerr << "Failed to access " << fn.value;
            if( errn )
                std::cerr << ", errno=" << *errn;
            std::cerr << std::endl;
            return 3;
        },

        // This handler will be called if the error includes:
        // - an object of type error_code equal to output_error, and
        // - an object of type leaf::e_errno (regardless of its .value),
        []( leaf::match<error_code, output_error>, leaf::e_errno const & errn )
        {
            std::cerr << "Output error, errno=" << errn << std::endl;
            return 4;
        },

        // This handler will be called if we've got a bad_command_line
        []( leaf::match<error_code, bad_command_line> )
        {
            std::cout << "Bad command line argument" << std::endl;
            return 5;
        },

        // This last handler matches any error: it prints diagnostic information
        // to help debug logic errors in the program, since it failed to match
        // an appropriate error handler to the error condition it encountered.
        // In this program this handler will never be called.
        []( leaf::error_info const & unmatched )
        {
            std::cerr <<
                "Unknown failure detected" << std::endl <<
                "Cryptic diagnostic information follows" << std::endl <<
                unmatched;
            return 6;
        } );
}


// Implementations of the functions called by main:


// Parse the command line, return the file name.
result<char const *> parse_command_line( int argc, char const * argv[] )
{
    if( argc==2 )
        return argv[1];
    else
        return leaf::new_error(bad_command_line).to_error_code();
}


// Open a file for reading.
result<std::shared_ptr<FILE>> file_open( char const * file_name )
{
    if( FILE * f = fopen(file_name, "rb") )
        return std::shared_ptr<FILE>(f, &fclose);
    else
        return leaf::new_error(open_error, leaf::e_errno{errno}).to_error_code();
}


// Return the size of the file.
result<int> file_size( FILE & f )
{
    auto load = leaf::on_error([] { return leaf::e_errno{errno}; });

    if( fseek(&f, 0, SEEK_END) )
        return leaf::new_error(size_error).to_error_code();

    int s = ftell(&f);
    if( s==-1L )
        return leaf::new_error(size_error).to_error_code();

    if( fseek(&f,0,SEEK_SET) )
        return leaf::new_error(size_error).to_error_code();

    return s;
}


// Read size bytes from f into buf.
result<void> file_read( FILE & f, void * buf, int size )
{
    int n = fread(buf, 1, size, &f);

    if( ferror(&f) )
        return leaf::new_error(read_error, leaf::e_errno{errno}).to_error_code();

    if( n!=size )
        return leaf::new_error(eof_error).to_error_code();

    return outcome::success();
}

////////////////////////////////////////

#ifdef BOOST_LEAF_NO_EXCEPTIONS

namespace boost
{
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e )
    {
        std::cerr << "Terminating due to a C++ exception under BOOST_LEAF_NO_EXCEPTIONS: " << e.what();
        std::terminate();
    }

    struct source_location;
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e, boost::source_location const & )
    {
        throw_exception(e);
    }
}

#endif
