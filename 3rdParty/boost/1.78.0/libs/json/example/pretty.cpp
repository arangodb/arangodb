//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

//[example_pretty

/*
    This example parses a JSON file and pretty-prints
    it to standard output.
*/

#include <boost/json.hpp>
#include <iomanip>
#include <iostream>

#include "file.hpp"

namespace json = boost::json;

json::value
parse_file( char const* filename )
{
    file f( filename, "r" );
    json::stream_parser p;
    json::error_code ec;
    do
    {
        char buf[4096];
        auto const nread = f.read( buf, sizeof(buf) );
        p.write( buf, nread, ec );
    }
    while( ! f.eof() );
    if( ec )
        return nullptr;
    p.finish( ec );
    if( ec )
        return nullptr;
    return p.release();
}

void
pretty_print( std::ostream& os, json::value const& jv, std::string* indent = nullptr )
{
    std::string indent_;
    if(! indent)
        indent = &indent_;
    switch(jv.kind())
    {
    case json::kind::object:
    {
        os << "{\n";
        indent->append(4, ' ');
        auto const& obj = jv.get_object();
        if(! obj.empty())
        {
            auto it = obj.begin();
            for(;;)
            {
                os << *indent << json::serialize(it->key()) << " : ";
                pretty_print(os, it->value(), indent);
                if(++it == obj.end())
                    break;
                os << ",\n";
            }
        }
        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "}";
        break;
    }

    case json::kind::array:
    {
        os << "[\n";
        indent->append(4, ' ');
        auto const& arr = jv.get_array();
        if(! arr.empty())
        {
            auto it = arr.begin();
            for(;;)
            {
                os << *indent;
                pretty_print( os, *it, indent);
                if(++it == arr.end())
                    break;
                os << ",\n";
            }
        }
        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "]";
        break;
    }

    case json::kind::string:
    {
        os << json::serialize(jv.get_string());
        break;
    }

    case json::kind::uint64:
        os << jv.get_uint64();
        break;

    case json::kind::int64:
        os << jv.get_int64();
        break;

    case json::kind::double_:
        os << jv.get_double();
        break;

    case json::kind::bool_:
        if(jv.get_bool())
            os << "true";
        else
            os << "false";
        break;

    case json::kind::null:
        os << "null";
        break;
    }

    if(indent->empty())
        os << "\n";
}

int
main(int argc, char** argv)
{
    if(argc != 2)
    {
        std::cerr <<
            "Usage: pretty <filename>"
            << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        // Parse the file as JSON
        auto const jv = parse_file( argv[1] );

        // Now pretty-print the value
        pretty_print(std::cout, jv);
    }
    catch(std::exception const& e)
    {
        std::cerr <<
            "Caught exception: "
            << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//]
