//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/zlib/error.hpp>

#include <boost/beast/unit_test/suite.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace zlib {

class error_test : public unit_test::suite
{
public:
    void check(char const* name, error ev)
    {
        auto const ec = make_error_code(ev);
        BEAST_EXPECT(std::string{ec.category().name()} == name);
        BEAST_EXPECT(! ec.message().empty());
        BEAST_EXPECT(std::addressof(ec.category()) ==
            std::addressof(detail::get_error_category()));
        BEAST_EXPECT(detail::get_error_category().equivalent(
            static_cast<std::underlying_type<error>::type>(ev),
                ec.category().default_error_condition(
                    static_cast<std::underlying_type<error>::type>(ev))));
        BEAST_EXPECT(detail::get_error_category().equivalent(
            ec, static_cast<std::underlying_type<error>::type>(ev)));
    }

    void run() override
    {
        check("beast.zlib", error::need_buffers);
        check("beast.zlib", error::end_of_stream);
        check("beast.zlib", error::stream_error);

        check("beast.zlib", error::invalid_block_type);
        check("beast.zlib", error::invalid_stored_length);
        check("beast.zlib", error::too_many_symbols);
        check("beast.zlib", error::invalid_code_lenths);
        check("beast.zlib", error::invalid_bit_length_repeat);
        check("beast.zlib", error::missing_eob);
        check("beast.zlib", error::invalid_literal_length);
        check("beast.zlib", error::invalid_distance_code);
        check("beast.zlib", error::invalid_distance);

        check("beast.zlib", error::over_subscribed_length);
        check("beast.zlib", error::incomplete_length_set);

        check("beast.zlib", error::general);
    }
};

BEAST_DEFINE_TESTSUITE(beast,zlib,error);

} // zlib
} // beast
} // boost
