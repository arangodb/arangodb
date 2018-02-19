//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/rfc7230.hpp>

#include <boost/beast/http/detail/rfc7230.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <string>
#include <vector>

#include <boost/beast/core/detail/empty_base_optimization.hpp>

namespace boost {
namespace beast {
namespace http {

class rfc7230_test : public beast::unit_test::suite
{
public:
    static
    std::string
    fmt(std::string const& s)
    {
        return '\'' + s + '\'';
    }

    static
    std::string
    str(string_view s)
    {
        return std::string(s.data(), s.size());
    }

    static
    std::string
    str(param_list const& c)
    {
        std::string s;
        for(auto const& p : c)
        {
            s.push_back(';');
            s.append(str(p.first));
            if(! p.second.empty())
            {
                s.push_back('=');
                s.append(str(p.second));
            }
        }
        return s;
    }

    void
    testParamList()
    {
        auto const ce =
            [&](std::string const& s)
            {
                auto const got = str(param_list{s});
                BEAST_EXPECTS(got == s, fmt(got));
            };
        auto const cs =
            [&](std::string const& s, std::string const& answer)
            {
                ce(answer);
                auto const got = str(param_list{s});
                ce(got);
                BEAST_EXPECTS(got == answer, fmt(got));
            };
        auto const cq =
            [&](std::string const& s, std::string const& answer)
            {
                auto const got = str(param_list{s});
                BEAST_EXPECTS(got == answer, fmt(got));
            };

        ce("");
        ce(";x");
        ce(";xy");
        ce(";x;y");

        ce("");
        cs(" ;\t i =\t 1 \t", ";i=1");
        cq("\t; \t xyz=1 ; ijk=\"q\\\"t\"", ";xyz=1;ijk=q\"t");
        ce(";x;y");

        // invalid strings
        cs(";", "");
        cs(";,", "");
        cq(";x=,", "");
        cq(";xy=\"", "");
        cq(";xy=\"\x7f", "");
        cq(";xy=\"\\", "");
        cq(";xy=\"\\\x01\"", "");
    }

    static
    std::string
    str(ext_list const& ex)
    {
        std::string s;
        for(auto const& e : ex)
        {
            if(! s.empty())
                s += ',';
            s.append(str(e.first));
            s += str(e.second);
        }
        return s;
    }

    void
    testExtList()
    {
        auto const ce =
            [&](std::string const& s)
            {
                auto const got = str(ext_list{s});
                BEAST_EXPECTS(got == s, fmt(got));
            };
        auto const cs =
            [&](std::string const& s, std::string const& good)
            {
                ce(good);
                auto const got = str(ext_list{s});
                ce(got);
                BEAST_EXPECTS(got == good, fmt(got));
            };
        auto const cq =
            [&](std::string const& s, std::string const& good)
            {
                auto const got = str(ext_list{s});
                BEAST_EXPECTS(got == good, fmt(got));
            };
    /*
        ext-basic_parsed_list    = *( "," OWS ) ext *( OWS "," [ OWS ext ] )
        ext         = token param-basic_parsed_list
        param-basic_parsed_list  = *( OWS ";" OWS param )
        param       = token OWS "=" OWS ( token / quoted-string )
    */
        cs(",", "");
        cs(", ", "");
        cs(",\t", "");
        cs(", \t", "");
        cs(" ", "");
        cs(" ,", "");
        cs("\t,", "");
        cs("\t , \t", "");
        cs(",,", "");
        cs(" , \t,, \t,", "");
        cs( "permessage-deflate; client_no_context_takeover; client_max_window_bits",
            "permessage-deflate;client_no_context_takeover;client_max_window_bits");

        ce("a");
        ce("ab");
        ce("a,b");
        cs(" a ", "a");
        cs("\t a, b\t  ,  c\t", "a,b,c");
        ce("a;b");
        ce("a;b;c");

        cs("a; \t i\t=\t \t1\t ", "a;i=1");
        ce("a;i=1;j=2;k=3");
        ce("a;i=1;j=2;k=3,b;i=4;j=5;k=6");

        cq("ab;x=\" \"", "ab;x= ");
        cq("ab;x=\"\\\"\"", "ab;x=\"");

        BEAST_EXPECT(ext_list{"a,b;i=1,c;j=2;k=3"}.exists("A"));
        BEAST_EXPECT(ext_list{"a,b;i=1,c;j=2;k=3"}.exists("b"));
        BEAST_EXPECT(! ext_list{"a,b;i=1,c;j=2;k=3"}.exists("d"));

        // invalid strings
        cs("i j", "i");
        cs(";", "");
    }

    static
    std::string
    str(token_list const& c)
    {
        bool first = true;
        std::string s;
        for(auto const& p : c)
        {
            if(! first)
                s.push_back(',');
            s.append(str(p));
            first = false;
        }
        return s;
    }

    void
    testTokenList()
    {
        auto const ce =
            [&](std::string const& s)
            {
                auto const got = str(token_list{s});
                BEAST_EXPECTS(got == s, fmt(got));
            };
        auto const cs =
            [&](std::string const& s, std::string const& good)
            {
                ce(good);
                auto const got = str(token_list{s});
                ce(got);
                BEAST_EXPECTS(got == good, fmt(got));
            };

        cs("", "");
        cs(" ", "");
        cs("  ", "");
        cs("\t", "");
        cs(" \t ", "");
        cs(",", "");
        cs(",,", "");
        cs(" ,", "");
        cs(" , ,", "");
        cs(" x", "x");
        cs(" \t x", "x");
        cs("x ", "x");
        cs("x \t", "x");
        cs(" \t x \t ", "x");
        ce("x,y");
        cs("x ,\ty ", "x,y");
        cs("x, y, z", "x,y,z");

        BEAST_EXPECT(token_list{"a,b,c"}.exists("A"));
        BEAST_EXPECT(token_list{"a,b,c"}.exists("b"));
        BEAST_EXPECT(! token_list{"a,b,c"}.exists("d"));

        // invalid
        cs("x y", "x");
    }

    template<class Policy>
    static
    std::vector<std::string>
    to_vector(string_view in)
    {
        std::vector<std::string> v;
        detail::basic_parsed_list<Policy> list{in};
        for(auto const& s :
                detail::basic_parsed_list<Policy>{in})
            v.emplace_back(s.data(), s.size());
        return v;
    }

    template<class Policy>
    void
    validate(string_view in,
        std::vector<std::string> const& v)
    {
        BEAST_EXPECT(to_vector<Policy>(in) == v);
    }

    template<class Policy>
    void
    good(string_view in)
    {
        BEAST_EXPECT(validate_list(
            detail::basic_parsed_list<Policy>{in}));
    }

    template<class Policy>
    void
    good(string_view in,
        std::vector<std::string> const& v)
    {
        BEAST_EXPECT(validate_list(
            detail::basic_parsed_list<Policy>{in}));
        validate<Policy>(in, v);
    }

    template<class Policy>
    void
    bad(string_view in)
    {
        BEAST_EXPECT(! validate_list(
            detail::basic_parsed_list<Policy>{in}));
    }

    void
    testOptTokenList()
    {
    /*
        #token = [ ( "," / token )   *( OWS "," [ OWS token ] ) ]
    */
        using type = detail::opt_token_list_policy;

        good<type>("",          {});
        good<type>(" ",         {});
        good<type>("\t",        {});
        good<type>(" \t",       {});
        good<type>(",",         {});
        good<type>(",,",        {});
        good<type>(", ,",       {});
        good<type>(",\t,",      {});
        good<type>(", \t,",     {});
        good<type>(", \t, ",    {});
        good<type>(", \t,\t",   {});
        good<type>(", \t, \t",  {});

        good<type>("x",         {"x"});
        good<type>(" x",        {"x"});
        good<type>("x,,",       {"x"});
        good<type>("x, ,",      {"x"});
        good<type>("x,, ",      {"x"});
        good<type>("x,,,",      {"x"});

        good<type>("x,y",       {"x","y"});
        good<type>("x ,y",      {"x","y"});
        good<type>("x\t,y",     {"x","y"});
        good<type>("x \t,y",    {"x","y"});
        good<type>(" x,y",      {"x","y"});
        good<type>(" x,y ",     {"x","y"});
        good<type>(",x,y",      {"x","y"});
        good<type>("x,y,",      {"x","y"});
        good<type>(",,x,y",     {"x","y"});
        good<type>(",x,,y",     {"x","y"});
        good<type>(",x,y,",     {"x","y"});
        good<type>("x ,, y",    {"x","y"});
        good<type>("x , ,y",    {"x","y"});

        good<type>("x,y,z",     {"x","y","z"});

        bad<type>("(");
        bad<type>("x(");
        bad<type>("(x");
        bad<type>(",(");
        bad<type>("(,");
        bad<type>("x,(");
        bad<type>("(,x");
        bad<type>("x y");
    }

    void
    run()
    {
        testOptTokenList();
#if 0
        testParamList();
        testExtList();
        testTokenList();
#endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,rfc7230);

} // http
} // beast
} // boost
