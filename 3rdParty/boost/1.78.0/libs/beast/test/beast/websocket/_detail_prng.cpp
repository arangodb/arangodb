//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/detail/prng.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

#ifdef BOOST_BEAST_TEST_STATIC_PRNG_SEED
auto prng_init = []()
{
    // Workaround for https://bugs.launchpad.net/ubuntu/+source/valgrind/+bug/1501545
    std::seed_seq seq{{0xDEAD, 0xBEEF}};
    detail::prng_seed(&seq);
    return 0;
}();
#endif // BOOST_BEAST_TEST_STATIC_PRNG_SEED

class prng_test
    : public beast::unit_test::suite
{
public:
#if 0
    char const* name() const noexcept //override
    {
        return "boost.beast.websocket.detail.prng";
    }
#endif

    template <class F>
    void
    testPrng(F const& f)
    {
        auto const mn = (std::numeric_limits<std::uint32_t>::min)();
        auto const mx = (std::numeric_limits<std::uint32_t>::max)();

        {
            auto v = f()();
            BEAST_EXPECT(
                v >= mn &&
                v <= mx);
        }
        {
            auto v = f()();
            BEAST_EXPECT(
                v >= mn &&
                v <= mx);
        }
    }

    void
    run() override
    {
        testPrng([]{ return make_prng(true); });
        testPrng([]{ return make_prng(false); });
    }
};

//static prng_test t;
BEAST_DEFINE_TESTSUITE(beast,websocket,prng);

} // detail
} // websocket
} // beast
} // boost
