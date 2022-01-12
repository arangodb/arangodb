//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2020 Paul Dreik (github@pauldreik.se)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json/parse.hpp>

using namespace boost::json;

bool
fuzz_parse(string_view sv)
{
    error_code ec;
    value jv = parse( sv, ec );
    if(ec)
        return false;
    return jv.is_number();
}

extern "C"
int
LLVMFuzzerTestOneInput(
    const uint8_t* data, size_t size)
{
    try
    {
        string_view s{reinterpret_cast<
            const char*>(data), size};
        fuzz_parse(s);
    }
    catch(...)
    {
    }
    return 0;
}
