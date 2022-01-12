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

class proxy
{
    value& jv_;

public:
    explicit
    proxy(value& jv) noexcept
        : jv_(jv)
    {
    }

    proxy
    operator[](string_view key)
    {
        object* obj;
        if(jv_.is_null())
            obj = &jv_.emplace_object();
        else
            obj = &jv_.as_object();
        return proxy((*obj)[key]);
    }

    proxy
    operator[](std::size_t index)
    {
        array *arr;
        if(jv_.is_null())
            arr = &jv_.emplace_array();
        else
            arr = &jv_.as_array();
        if(arr->size() <= index)
            arr->resize(index + 1);
        return proxy((*arr)[index]);
    }

    value&
    operator*() noexcept
    {
        return jv_;
    }
};


int
main(int, char**)
{
    value jv;
    *proxy(jv)["user:config"]["authority"]["router"][0]["users"] = 42;
    return EXIT_SUCCESS;
}
