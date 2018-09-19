
#ifndef BOOST_CONTRACT_TEST_OUT_INLINED_HPP_
#define BOOST_CONTRACT_TEST_OUT_INLINED_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include "out.hpp"
#include <iostream>

namespace boost { namespace contract { namespace test { namespace detail {

namespace out_ {
    std::string out;
}

std::string out() { return out_::out; }

void out(std::string const& text) {
    if(text == "") out_::out = "";
    else {
        out_::out = out_::out + text;
        std::clog << text;
        std::clog.flush();
    }
}

} } } }

#endif // #include guard

