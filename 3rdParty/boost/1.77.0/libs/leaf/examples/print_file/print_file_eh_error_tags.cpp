// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This is the program presented in
// https://boostorg.github.io/leaf/#introduction-eh.

// It reads a text file in a buffer and prints it to std::cout, using LEAF to
// handle errors. This version uses exception handling. The version that does
// not use exception handling is in print_file_result.cpp.

#include <boost/leaf.hpp>
#include <iostream>
#include <memory>
#include <stdio.h>

namespace leaf = boost::leaf;


// Error types
struct bad_command_line { };
struct input_error { };
struct open_error { };
struct read_error { };
struct size_error { };
struct eof_error { };
struct output_error { };


// We will handle all failures in our main function, but first, here are the
// declarations of the functions it calls, each communicating failures by
// throwing exceptions

// Parse the command line, return the file name.
char const * parse_command_line( int argc, char const * argv[] );

// Open a file for reading.
std::shared_ptr<FILE> file_open( char const * file_name );

// Return the size of the file.
int file_size( FILE & f );

// Read size bytes from f into buf.
void file_read( FILE & f, void * buf, int size );


// The main function, which handles all errors.
int main( int argc, char const * argv[] )
{
    return leaf::try_catch(

        [&]
        {
            char const * file_name = parse_command_line(argc,argv);

            auto load = leaf::on_error( leaf::e_file_name{file_name} );

            std::shared_ptr<FILE> f = file_open(file_name);

            int s = file_size(*f);

            std::string buffer(1 + s, '\0');
            file_read(*f, &buffer[0], buffer.size()-1);

            std::cout << buffer;
            std::cout.flush();
            if( std::cout.fail() )
                throw leaf::exception(output_error{}, leaf::e_errno{errno});

            return 0;
        },

        // Each of the lambdas below is an error handler. LEAF will consider
        // them, in order, and call the first one that matches the available
        // error objects.

        // This handler will be called if the error includes:
        // - an object of type open_error, and
        // - an object of type leaf::e_errno that has .value equal to ENOENT,
        //   and
        // - an object of type leaf::e_file_name.
        []( open_error &, leaf::match_value<leaf::e_errno, ENOENT>, leaf::e_file_name const & fn )
        {
            std::cerr << "File not found: " << fn.value << std::endl;
            return 1;
        },

        // This handler will be called if the error includes:
        // - an object of type open_error, and
        // - an object of type leaf::e_errno (regardless of its .value), and
        // - an object of type leaf::e_file_name.
        []( open_error &, leaf::e_errno const & errn, leaf::e_file_name const & fn )
        {
            std::cerr << "Failed to open " << fn.value << ", errno=" << errn << std::endl;
            return 2;
        },

        // This handler will be called if the error includes:
        // - an object of type input_error, and
        // - an optional object of type leaf::e_errno (regardless of its
        //   .value), and
        // - an object of type leaf::e_file_name.
        []( input_error &, leaf::e_errno const * errn, leaf::e_file_name const & fn )
        {
            std::cerr << "Failed to access " << fn.value;
            if( errn )
                std::cerr << ", errno=" << *errn;
            std::cerr << std::endl;
            return 3;
        },

        // This handler will be called if the error includes:
        // - an object of type output_error, and
        // - an object of type leaf::e_errno (regardless of its .value),
        []( output_error &, leaf::e_errno const & errn )
        {
            std::cerr << "Output error, errno=" << errn << std::endl;
            return 4;
        },

        // This handler will be called if we've got a bad_command_line
        []( bad_command_line & )
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
char const * parse_command_line( int argc, char const * argv[] )
{
    if( argc==2 )
        return argv[1];
    else
        throw leaf::exception(bad_command_line{});
}


// Open a file for reading.
std::shared_ptr<FILE> file_open( char const * file_name )
{
    auto load = leaf::on_error(input_error{});

    if( FILE * f = fopen(file_name, "rb") )
        return std::shared_ptr<FILE>(f, &fclose);
    else
        throw leaf::exception(open_error{}, leaf::e_errno{errno});
}


// Return the size of the file.
int file_size( FILE & f )
{
    auto load = leaf::on_error(input_error{}, size_error{}, []{ return leaf::e_errno{errno}; });

    if( fseek(&f, 0, SEEK_END) )
        throw leaf::exception();

    int s = ftell(&f);
    if( s==-1L )
        throw leaf::exception();

    if( fseek(&f,0,SEEK_SET) )
        throw leaf::exception();

    return s;
}


// Read size bytes from f into buf.
void file_read( FILE & f, void * buf, int size )
{
    auto load = leaf::on_error(input_error{});

    int n = fread(buf, 1, size, &f);

    if( ferror(&f) )
        throw leaf::exception(read_error{}, leaf::e_errno{errno});

    if( n!=size )
        throw leaf::exception(eof_error{});
}
