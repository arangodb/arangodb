
// no #include guard

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract/assert.hpp>
#include <boost/contract/core/exception.hpp>
#include <boost/detail/lightweight_test.hpp>

int main() {
    bool threw = false;
    try {
        #ifdef BOOST_CONTRACT_TEST_ERROR
            BOOST_CONTRACT_ASSERT_AUDIT(
                    BOOST_CONTRACT_TEST_ERROR_expected_undeclared_identifier);
        #else
            BOOST_CONTRACT_ASSERT_AUDIT(false);
        #endif
    } catch(boost::contract::assertion_failure const&) { threw = true; }
    
    #if defined(BOOST_CONTRACT_AUDITS) && !defined(BOOST_CONTRACT_NO_ALL)
        BOOST_TEST(threw);
    #else
        BOOST_TEST(!threw);
    #endif
    
    return boost::report_errors();
}

