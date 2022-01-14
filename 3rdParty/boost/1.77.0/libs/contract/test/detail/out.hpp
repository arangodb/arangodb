
#ifndef BOOST_CONTRACT_TEST_DETAIL_OUT_HPP_
#define BOOST_CONTRACT_TEST_DETAIL_OUT_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// To declare test string functions across shared libraries.

#include <string>

namespace boost { namespace contract { namespace test { namespace detail {

std::string BOOST_CONTRACT_TEST_DETAIL_OUT_DECLSPEC out();

void BOOST_CONTRACT_TEST_DETAIL_OUT_DECLSPEC out(std::string const& text);

} } } }

#endif // #include guard

