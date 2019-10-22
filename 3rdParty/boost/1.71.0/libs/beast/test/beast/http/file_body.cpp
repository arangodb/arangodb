//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/file_body.hpp>

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/file_stdio.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/filesystem.hpp>

namespace boost {
namespace beast {
namespace http {

class file_body_test : public beast::unit_test::suite
{
public:
    struct lambda
    {
        flat_buffer buffer;

        template<class ConstBufferSequence>
        void
        operator()(error_code&, ConstBufferSequence const& buffers)
        {
            buffer.commit(net::buffer_copy(
                buffer.prepare(buffer_bytes(buffers)),
                buffers));
        }
    };

    template<class File>
    void
    doTestFileBody()
    {
        error_code ec;
        string_view const s =
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 3\r\n"
            "\r\n"
            "xyz";
        auto const temp = boost::filesystem::unique_path();
        {
            response_parser<basic_file_body<File>> p;
            p.eager(true);

            p.get().body().open(
                temp.string<std::string>().c_str(), file_mode::write, ec);
            BEAST_EXPECTS(! ec, ec.message());

            p.put(net::buffer(s.data(), s.size()), ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
        {
            File f;
            f.open(temp.string<std::string>().c_str(), file_mode::read, ec);
            auto size = f.size(ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(size == 3);
            std::string s1;
            s1.resize(3);
            f.read(&s1[0], s1.size(), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECTS(s1 == "xyz", s);
        }
        {
            lambda visit;
            {
                response<basic_file_body<File>> res{status::ok, 11};
                res.set(field::server, "test");
                res.body().open(temp.string<std::string>().c_str(),
                    file_mode::scan, ec);
                BEAST_EXPECTS(! ec, ec.message());
                res.prepare_payload();

                serializer<false, basic_file_body<File>, fields> sr{res};
                sr.next(ec, visit);
                BEAST_EXPECTS(! ec, ec.message());
                auto const b = buffers_front(visit.buffer.data());
                string_view const s1{
                    reinterpret_cast<char const*>(b.data()),
                    b.size()};
                BEAST_EXPECTS(s1 == s, s1);
            }
        }
        boost::filesystem::remove(temp, ec);
        BEAST_EXPECTS(! ec, ec.message());
    }
    void
    run() override
    {
        doTestFileBody<file_stdio>();
    #if BOOST_BEAST_USE_WIN32_FILE
        doTestFileBody<file_win32>();
    #endif
    #if BOOST_BEAST_USE_POSIX_FILE
        doTestFileBody<file_posix>();
    #endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,file_body);

} // http
} // beast
} // boost
