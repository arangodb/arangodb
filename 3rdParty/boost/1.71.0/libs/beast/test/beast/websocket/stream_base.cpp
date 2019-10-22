//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream_base.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {

class stream_base_test : public unit_test::suite
{
public:
    struct T1
    {
        void operator()(request_type&);
    };

    struct T2
    {
        void operator()(response_type&);
    };

    struct T3
    {
        void operator()(request_type&);
        void operator()(response_type&);
    };

    struct T4
    {
        template<class T>
        void operator()(T&);
    };

    struct T5
    {
        void operator()();
    };

    BOOST_STATIC_ASSERT(detail::is_decorator<T1>::value);
    BOOST_STATIC_ASSERT(detail::is_decorator<T2>::value);
    BOOST_STATIC_ASSERT(detail::is_decorator<T3>::value);
    BOOST_STATIC_ASSERT(detail::is_decorator<T4>::value);
    BOOST_STATIC_ASSERT(! detail::is_decorator<T5>::value);
    BOOST_STATIC_ASSERT(! detail::is_decorator<int>::value);

    void
    testJavadoc()
    {
    }

    void
    run() override
    {
        testJavadoc();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream_base);

} // websocket
} // beast
} // boost
