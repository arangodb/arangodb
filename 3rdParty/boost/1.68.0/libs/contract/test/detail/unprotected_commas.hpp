
#ifndef BOOST_CONTRACT_TEST_DETAIL_UNPROTECTED_COMMAS_HPP_
#define BOOST_CONTRACT_TEST_DETAIL_UNPROTECTED_COMMAS_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

namespace boost { namespace contract { namespace test { namespace detail {

// Used to test passing unprotected commas into macro parameters.
template<typename T1, typename Unused2, typename Unused3>
struct unprotected_commas {
    typedef T1 type1; // For type macro parameters.

    static void call() {} // For code block macro parameters.

    // For value macro parameters.
    template<typename U> static U& same(U& x) { return x; }
    template<typename U> static U* same(U* x) { return x; }
};

} } } } // namespace

#endif // #include guard

