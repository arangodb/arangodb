//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2020 Paul Dreik (github@pauldreik.se)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// This file must be manually included when
// using basic_parser to implement a parser.
#include <boost/json/basic_parser_impl.hpp>

#include <iomanip>
#include <iostream>

using namespace boost::json;

class null_parser
{
    struct handler
    {
        constexpr static std::size_t max_object_size = std::size_t(-1);
        constexpr static std::size_t max_array_size = std::size_t(-1);
        constexpr static std::size_t max_key_size = std::size_t(-1);
        constexpr static std::size_t max_string_size = std::size_t(-1);

        bool on_document_begin( error_code& ) { return true; }
        bool on_document_end( error_code& ) { return true; }
        bool on_object_begin( error_code& ) { return true; }
        bool on_object_end( std::size_t, error_code& ) { return true; }
        bool on_array_begin( error_code& ) { return true; }
        bool on_array_end( std::size_t, error_code& ) { return true; }
        bool on_key_part( string_view, std::size_t, error_code& ) { return true; }
        bool on_key( string_view, std::size_t, error_code& ) { return true; }
        bool on_string_part( string_view, std::size_t, error_code& ) { return true; }
        bool on_string( string_view, std::size_t, error_code& ) { return true; }
        bool on_number_part( string_view, error_code& ) { return true; }
        bool on_int64( std::int64_t, string_view, error_code& ) { return true; }
        bool on_uint64( std::uint64_t, string_view, error_code& ) { return true; }
        bool on_double( double, string_view, error_code& ) { return true; }
        bool on_bool( bool, error_code& ) { return true; }
        bool on_null( error_code& ) { return true; }
        bool on_comment_part(string_view, error_code&) { return true; }
        bool on_comment(string_view, error_code&) { return true; }
    };

    basic_parser<handler> p_;

public:
    null_parser()
        : p_(parse_options())
    {
    }

    ~null_parser()
    {
    }

    std::size_t
    write(
        char const* data,
        std::size_t size,
        error_code& ec)
    {
        auto const n = p_.write_some( false, data, size, ec );
        if(! ec && n < size)
            ec = error::extra_data;
        return n;
    }
};

bool
validate( string_view s )
{
    // Parse with the null parser and return false on error
    null_parser p;
    error_code ec;
    p.write( s.data(), s.size(), ec );
    if( ec )
        return false;

    // The string is valid JSON.
    return true;
}

extern "C"
int
LLVMFuzzerTestOneInput(
    const uint8_t* data, size_t size)
{
    try
    {
        validate(string_view{
            reinterpret_cast<const char*
                >(data), size});
    }
    catch(...)
    {
    }
    return 0;
}
