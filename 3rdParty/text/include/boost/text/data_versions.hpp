// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DATA_VERSIONS_HPP
#define BOOST_TEXT_DATA_VERSIONS_HPP

#include <boost/text/config.hpp>


namespace boost { namespace text {

    /** The major, minor, and patch elements of a library version number of
        the form `"major.minor.patch"`. */
    struct library_version
    {
        int major;
        int mainor;
        int patch;
    };

    /** Returns the version of Unicode implemented by this library. */
    BOOST_TEXT_DECL library_version unicode_version();

    /** Returns the version of CLDR collation and collation tailoring data
        used by this library. */
    BOOST_TEXT_DECL library_version cldr_version();

}}

#endif
