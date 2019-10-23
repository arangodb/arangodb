//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/rate_policy.hpp>
#include <boost/core/ignore_unused.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

//[concept_RatePolicy
class RatePolicy
{
    friend class rate_policy_access;

    std::size_t
    available_read_bytes();

    std::size_t
    available_write_bytes();
    
    void
    transfer_read_bytes(std::size_t);
    
    void
    transfer_write_bytes(std::size_t);

    void
    on_timer();
};
//]

namespace boost {
namespace beast {

class rate_policy_test : public unit_test::suite
{
public:
    void
    run() override
    {
        boost::ignore_unused(unlimited_rate_policy{});
        boost::ignore_unused(simple_rate_policy{});

        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,rate_policy);

} // beast
} // boost
