//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/error.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace http {

class error_test : public unit_test::suite
{
public:
    void
    check(char const* name, error ev)
    {
        auto const ec = make_error_code(ev);
        auto const& cat = make_error_code(
            static_cast<http::error>(0)).category();
        BEAST_EXPECT(std::string(ec.category().name()) == name);
        BEAST_EXPECT(! ec.message().empty());
        BEAST_EXPECT(
            std::addressof(ec.category()) == std::addressof(cat));
        BEAST_EXPECT(cat.equivalent(
            static_cast<std::underlying_type<error>::type>(ev),
                ec.category().default_error_condition(
                    static_cast<std::underlying_type<error>::type>(ev))));
        BEAST_EXPECT(cat.equivalent(ec,
            static_cast<std::underlying_type<error>::type>(ev)));
    }

    void
    run() override
    {
        check("beast.http", error::end_of_stream);
        check("beast.http", error::partial_message);
        check("beast.http", error::need_more);
        check("beast.http", error::unexpected_body);
        check("beast.http", error::need_buffer);
        check("beast.http", error::end_of_chunk);
        check("beast.http", error::buffer_overflow);
        check("beast.http", error::header_limit);
        check("beast.http", error::body_limit);
        check("beast.http", error::bad_alloc);

        check("beast.http", error::bad_line_ending);
        check("beast.http", error::bad_method);
        check("beast.http", error::bad_target);
        check("beast.http", error::bad_version);
        check("beast.http", error::bad_status);
        check("beast.http", error::bad_reason);
        check("beast.http", error::bad_field);
        check("beast.http", error::bad_value);
        check("beast.http", error::bad_content_length);
        check("beast.http", error::bad_transfer_encoding);
        check("beast.http", error::bad_chunk);
        check("beast.http", error::bad_chunk_extension);
        check("beast.http", error::bad_obs_fold);

        check("beast.http", error::stale_parser);
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,error);

} // http
} // beast
} // boost
