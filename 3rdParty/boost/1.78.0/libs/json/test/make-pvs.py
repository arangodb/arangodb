# Build test vector variable

import os

def chex(c):
    d1 = ord(c)/16;
    d2 = ord(c)%16;
    d = "0123456789ABCDEF";
    s = "\\x" + d[d1:d1+1] + d[d2:d2+1];
    return s;

def escape(c):
    if c == ' ' or c == '\t':
        return c;
    elif c == '\"':
        return "\\\"";
    elif c == '\\':
        return "\\\\";
    n = ord(c);
    if n >= 32 and n <= 127:
        return c;
    return chex(c) + "\"\"";

def tocpp(s):
    v0 = ""
    v = "\"";
    for c in s:
        v = v + escape(c);
        if len(v) > 80:
            if len(v0) > 50000:
                return v0 + v + "\"";
            v0 += v + "\"\n               \"";
            v = "";
    return v0 + v + "\"";

def do_files(directory):
    for root, directories, files in os.walk(directory):
        for filename in files:
            filepath = os.path.join(root, filename)
            with open(filepath, 'r') as file:
                data = file.read();
                print("        { '" + filename[0:1] + "', \"" + filename[2:-5] + "\", lit(" + tocpp(data) + ") },");

print("""
//
// Copyright (c) 2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef PARSE_VECTORS
#define PARSE_VECTORS

#include <boost/utility/string_view.hpp>
#include <cstdlib>
#include <type_traits>

struct parse_vectors
{
    struct item
    {
        char result;
        ::boost::string_view name;
        ::boost::string_view text;
    };

    using iterator = item const*;

    iterator begin() const noexcept
    {
        return first_;
    }

    iterator end() const noexcept
    {
        return last_;
    }

    std::size_t
    size() const noexcept
    {
        return last_ - first_;
    }

    inline parse_vectors() noexcept;

private:
    template<std::size_t N>
    static
    ::boost::string_view
    lit(char const (&s)[N])
    {
        return {s, N - 1};
    }

    iterator first_;
    iterator last_;
};

parse_vectors::
parse_vectors() noexcept
{
    static item const list[] = {""");

do_files("parse-vectors");

print("""        { ' ', "", "" }
    };
    first_ = &list[0];
    last_ = &list[std::extent<
        decltype(list)>::value - 1];
}

#endif""");
