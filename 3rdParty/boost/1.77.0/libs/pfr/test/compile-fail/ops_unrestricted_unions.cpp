// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/ops.hpp>

#include <string>

#if defined(_MSC_VER)
# pragma warning( disable: 4624 ) // destructor was implicitly defined as deleted
#endif

union test_unrestricted_union {
    int i;
    std::string s;
};

int main() {
    struct two_unions {
        test_unrestricted_union u1, u2;
    };

    // Not calling the destructor intentionally!
    auto v = new two_unions{{1}, {1}};

    return boost::pfr::eq(*v, *v);
}
