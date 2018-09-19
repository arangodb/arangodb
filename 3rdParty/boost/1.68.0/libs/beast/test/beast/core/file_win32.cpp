//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/file_win32.hpp>

#if BOOST_BEAST_USE_WIN32_FILE

#include "file_test.hpp"

#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {

BOOST_STATIC_ASSERT(! std::is_copy_constructible<file_win32>::value);

class file_win32_test
    : public beast::unit_test::suite
{
public:
    void
    run()
    {
        doTestFile<file_win32>(*this);
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,file_win32);

} // beast
} // boost

#endif
