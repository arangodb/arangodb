//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/verb.hpp>

#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http {

class verb_test
    : public beast::unit_test::suite
{
public:
    void
    testVerb()
    {
        auto const good =
            [&](verb v)
            {
                BEAST_EXPECT(string_to_verb(to_string(v)) == v);
            };

        good(verb::unknown);

        good(verb::delete_);
        good(verb::get);
        good(verb::head);
        good(verb::post);
        good(verb::put);
        good(verb::connect);
        good(verb::options);
        good(verb::trace);
        good(verb::copy);
        good(verb::lock);
        good(verb::mkcol);
        good(verb::move);
        good(verb::propfind);
        good(verb::proppatch);
        good(verb::search);
        good(verb::unlock);
        good(verb::bind);
        good(verb::rebind);
        good(verb::unbind);
        good(verb::acl);
        good(verb::report);
        good(verb::mkactivity);
        good(verb::checkout);
        good(verb::merge);
        good(verb::msearch);
        good(verb::notify);
        good(verb::subscribe);
        good(verb::unsubscribe);
        good(verb::patch);
        good(verb::purge);
        good(verb::mkcalendar);
        good(verb::link);
        good(verb::unlink);

        auto const bad =
            [&](string_view s)
            {
                auto const v = string_to_verb(s);
                BEAST_EXPECTS(v == verb::unknown, to_string(v));
            };

        bad("AC_");
        bad("BIN_");
        bad("CHECKOU_");
        bad("CONNEC_");
        bad("COP_");
        bad("DELET_");
        bad("GE_");
        bad("HEA_");
        bad("LIN_");
        bad("LOC_");
        bad("M-SEARC_");
        bad("MERG_");
        bad("MKACTIVIT_");
        bad("MKCALENDA_");
        bad("MKCO_");
        bad("MOV_");
        bad("NOTIF_");
        bad("OPTION_");
        bad("PATC_");
        bad("POS_");
        bad("PROPFIN_");
        bad("PROPPATC_");
        bad("PURG_");
        bad("PU_");
        bad("REBIN_");
        bad("REPOR_");
        bad("SEARC_");
        bad("SUBSCRIB_");
        bad("TRAC_");
        bad("UNBIN_");
        bad("UNLIN_");
        bad("UNLOC_");
        bad("UNSUBSCRIB_");

        try
        {
            to_string(static_cast<verb>(-1));
            fail("", __FILE__, __LINE__);
        }
        catch(std::exception const&)
        {
            pass();
        }
    }

    void
    run()
    {
        testVerb();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,verb);

} // http
} // beast
} // boost
