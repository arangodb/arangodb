// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

//! [main]
#include <boost/hana/version.hpp>


#if BOOST_HANA_VERSION < 0x01020003
    // Hana's version is < 1.2.3
#else
    // Hana's version is >= 1.2.3
#endif
//! [main]

int main() { }
