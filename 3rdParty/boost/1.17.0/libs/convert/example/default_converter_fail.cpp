// Boost.Convert test and usage example
// Copyright (c) 2009-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include <boost/convert.hpp>

using std::string;
using boost::convert;

// This is expected to fail to compile:
//'boost::cnv::by_default' : class has no constructors.

int
main(int, char const* [])
{
    int    i = convert<int>("123").value();
    string s = convert<string>(123).value();
}

