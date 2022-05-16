//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json.hpp>

using namespace boost::json;

value&
path( value& jv, std::size_t index)
{
    array *arr;
    if(jv.is_null())
        arr = &jv.emplace_array();
    else
        arr = &jv.as_array();
    if(arr->size() <= index)
        arr->resize(index + 1);
    return (*arr)[index];
}

value&
path( value& jv, string_view key )
{
    object* obj;
    if(jv.is_null())
        obj = &jv.emplace_object();
    else
        obj = &jv.as_object();
    return (*obj)[key];
}

template<class Arg0, class Arg1, class... Args>
value&
path( value& jv, Arg0 const& arg0, Arg1 const& arg1, Args const&... args )
{
    return path( path( jv, arg0 ), arg1, args... );
}

int
main(int, char**)
{
    value jv;
    path( jv, "user:config", "authority", "router", 0, "users" ) = 42;
    return EXIT_SUCCESS;
}
