// Copyright (c) 2018 Adam Butcher, Antony Polukhin
// Copyright (c) 2019-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/core.hpp>
#include <boost/core/lightweight_test.hpp>

auto parseHex(char const* p, size_t limit = ~0u) {
    struct { size_t val; char const* rest; } res = { 0, p };
    while (limit) {
        int v = *res.rest;
        if (v >= '0' && v <= '9')
            v = v - '0';
        else if (v >= 'A' && v <= 'F')
            v = 10 + v - 'A';
        else if (v >= 'a' && v <= 'f')
            v = 10 + v - 'a';
        else
            break;
        res.val = (res.val << 4) + v;
        --limit;
        ++res.rest;
    }
    return res;
}

auto parseLinePrefix(char const* line) {
     struct {
          size_t byteCount, address, recordType; char const* rest;
     } res;
     using namespace boost::pfr;
     tie_from_structure (res.byteCount, line) = parseHex(line, 2);
     tie_from_structure (res.address, line) = parseHex(line, 4);
     tie_from_structure (res.recordType, line) = parseHex(line, 2);
     res.rest = line;
     return res;
}

int main() {
    auto line = "0860E000616263646566000063";
    auto meta = parseLinePrefix(line);
    BOOST_TEST_EQ(meta.byteCount, 8);
    BOOST_TEST_EQ(meta.address, 24800);
    BOOST_TEST_EQ(meta.recordType, 0);
    BOOST_TEST_EQ(meta.rest, line + 8);

    size_t val;
    using namespace boost::pfr;

    tie_from_structure (val, std::ignore) = parseHex("a73b");
    BOOST_TEST_EQ(val, 42811);

    tie_from_structure (std::ignore, line) = parseHex(line, 8);
    BOOST_TEST_EQ(line, meta.rest);

    return boost::report_errors();
}

