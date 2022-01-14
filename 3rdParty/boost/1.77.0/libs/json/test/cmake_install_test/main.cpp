// Copyright 2018 Peter Dimov
// Copyright 2020 Richard Hodges
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#include <boost/json.hpp>
#include <cstdio>

int main()
{
    const boost::json::value value = boost::json::parse("{ \"test\": true }");
    std::puts(boost::json::to_string(value).c_str());
}
