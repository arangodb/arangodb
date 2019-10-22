//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "nodejs_parser.hpp"

#include "test/beast/http/message_fuzz.hpp"

#include <boost/beast/http.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <chrono>
#include <iostream>
#include <vector>

namespace boost {
namespace beast {
namespace http {

class parser_test : public beast::unit_test::suite
{
public:
    static std::size_t constexpr N = 2000;

    //using corpus = std::vector<multi_buffer>;
    using corpus = std::vector<flat_buffer>;

    corpus creq_;
    corpus cres_;
    std::size_t size_ = 0;

    corpus
    build_corpus(std::size_t n, std::true_type)
    {
        corpus v;
        v.resize(n);
        message_fuzz mg;
        for(std::size_t i = 0; i < n; ++i)
        {
            mg.request(v[i]);
            size_ += v[i].size();
            BEAST_EXPECT(v[i].size() > 0);
        }
        return v;
    }

    corpus
    build_corpus(std::size_t n, std::false_type)
    {
        corpus v;
        v.resize(n);
        message_fuzz mg;
        for(std::size_t i = 0; i < n; ++i)
        {
            mg.response(v[i]);
            size_ += v[i].size();
            BEAST_EXPECT(v[i].size() > 0);
        }
        return v;
    }

    template<class ConstBufferSequence,
        bool isRequest>
    static
    std::size_t
    feed(ConstBufferSequence const& buffers,
        basic_parser<isRequest>& parser,
            error_code& ec)
    {
        beast::buffers_suffix<
            ConstBufferSequence> cb{buffers};
        std::size_t used = 0;
        for(;;)
        {
            auto const n =
                parser.put(cb, ec);
            if(ec)
                return 0;
            if(n == 0)
                break;
            cb.consume(n);
            used += n;
            if(parser.is_done())
                break;
            if(buffer_bytes(cb) == 0)
                break;
        }
        return used;
    }

    template<class Parser>
    void
    testParser1(std::size_t repeat, corpus const& v)
    {
        while(repeat--)
            for(auto const& b : v)
            {
                Parser p;
                error_code ec;
                p.write(b.data(), ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    log << buffers_to_string(b.data()) << std::endl;
            }
    }

    template<class Parser>
    void
    testParser2(std::size_t repeat, corpus const& v)
    {
        while(repeat--)
            for(auto const& b : v)
            {
                Parser p;
                p.header_limit((std::numeric_limits<std::uint32_t>::max)());
                error_code ec;
                feed(b.data(), p, ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    log << buffers_to_string(b.data()) << std::endl;
            }
    }

    template<class Function>
    void
    timedTest(std::size_t repeat, std::string const& name, Function&& f)
    {
        using namespace std::chrono;
        using clock_type = std::chrono::high_resolution_clock;
        log << name << std::endl;
        for(std::size_t trial = 1; trial <= repeat; ++trial)
        {
            auto const t0 = clock_type::now();
            f();
            auto const elapsed = clock_type::now() - t0;
            log <<
                "Trial " << trial << ": " <<
                duration_cast<milliseconds>(elapsed).count() << " ms" << std::endl;
        }
    }

    template<bool isRequest>
    struct null_parser :
        basic_parser<isRequest>
    {
        void
        on_request_impl(
            verb, string_view, string_view,
            int, error_code&) override
        {
        }

        void
        on_response_impl(
            int, string_view, int,
            error_code&) override
        {
        }

        void
        on_field_impl(
            field, string_view, string_view,
            error_code&) override
        {
        }

        void
        on_header_impl(error_code&) override
        {
        }

        void
        on_body_init_impl(
            boost::optional<std::uint64_t> const&,
            error_code&) override
        {
        }

        std::size_t
        on_body_impl(
            string_view,
            error_code&) override
        {
            return 0;
        }

        void
        on_chunk_header_impl(
            std::uint64_t,
            string_view,
            error_code&) override
        {
        }

        std::size_t
        on_chunk_body_impl(
            std::uint64_t,
            string_view,
            error_code&) override
        {
            return 0;
        }

        void
        on_finish_impl(error_code&) override
        {
        }
    };

    template<bool isRequest, class Body, class Fields>
    struct bench_parser : basic_parser<isRequest>
    {
        using mutable_buffers_type =
            net::mutable_buffer;

        void
        on_request_impl(verb, string_view,
            string_view, int, error_code&) override
        {
        }

        void
        on_response_impl(int,
            string_view, int, error_code&) override
        {
        }

        void
        on_field_impl(field,
            string_view, string_view, error_code&) override
        {
        }

        void
        on_header_impl(error_code&) override
        {
        }

        void
        on_body_init_impl(
            boost::optional<std::uint64_t> const&,
            error_code&) override
        {
        }

        std::size_t
        on_body_impl(
            string_view s, error_code&) override
        {
            return s.size();
        }

        void
        on_chunk_header_impl(std::uint64_t,
            string_view, error_code&) override
        {
        }

        std::size_t
        on_chunk_body_impl(std::uint64_t,
            string_view s, error_code&) override
        {
            return s.size();
        }

        void
        on_finish_impl(error_code&) override
        {
        }
    };

    void
    testSpeed()
    {
        static std::size_t constexpr Trials = 5;
        static std::size_t constexpr Repeat = 500;

        creq_ = build_corpus(N/2, std::true_type{});
        cres_ = build_corpus(N/2, std::false_type{});

        log << "sizeof(request parser)  == " <<
            sizeof(null_parser<true>) << '\n';

        log << "sizeof(response parser) == " <<
            sizeof(null_parser<false>)<< '\n';

        testcase << "Parser speed test, " <<
            ((Repeat * size_ + 512) / 1024) << "KB in " <<
                (Repeat * (creq_.size() + cres_.size())) << " messages";

#if 0
        timedTest(Trials, "http::parser",
            [&]
            {
                testParser2<request_parser<dynamic_body>>(Repeat, creq_);
                testParser2<response_parser<dynamic_body>>(Repeat, cres_);
            });
#endif
#if 1
        timedTest(Trials, "http::basic_parser",
            [&]
            {
                testParser2<bench_parser<
                    true, dynamic_body, fields> >(
                        Repeat, creq_);
                testParser2<bench_parser<
                    false, dynamic_body, fields>>(
                        Repeat, cres_);
            });
#if 1
        timedTest(Trials, "nodejs_parser",
            [&]
            {
                testParser1<nodejs_parser<
                    true, dynamic_body, fields>>(
                        Repeat, creq_);
                testParser1<nodejs_parser<
                    false, dynamic_body, fields>>(
                        Repeat, cres_);
            });
#endif
#endif
        pass();
    }

    void run() override
    {
        pass();
        testSpeed();
    }
};

BEAST_DEFINE_TESTSUITE(beast,benchmarks,parser);

} // http
} // beast
} // boost

