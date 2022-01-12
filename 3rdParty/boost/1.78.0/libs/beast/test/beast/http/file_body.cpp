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
#include <boost/beast/core/static_string.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/error.hpp>

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

    struct temp_file
    {
        temp_file(std::ostream& logger)
        : path_(boost::filesystem::unique_path())
        , log_(logger)
        {}

        ~temp_file()
        {
            if (!path_.empty())
            {
                auto ec = error_code();
                boost::filesystem::remove(path_, ec);
                if (ec)
                {
                    log_ << "warning: failed to remove temporary file: " << path_ << "\n";
                }
            }

        }

        temp_file(temp_file&&) = default;
        temp_file(temp_file const&) = delete;
        temp_file& operator=(temp_file&&) = delete;
        temp_file& operator=(temp_file const&) = delete;

        boost::filesystem::path const& path() const
        {
            return path_;
        }

    private:

        boost::filesystem::path path_;
        std::ostream& log_;
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

    template<class File>
    void
    fileBodyUnexpectedEofOnGet()
    {
        auto temp = temp_file(log);

        error_code ec;
        string_view const ten =
            "0123456789"; // 40
        // create the temporary file
        {
            std::ofstream fstemp(temp.path().native());
            std::size_t written = 0;
            std::size_t to_write = 4097;
            while (written < to_write)
            {
                auto size = (std::min)(ten.size(), to_write - written);
                fstemp << ten.substr(0, size);
                BEAST_EXPECT(fstemp.good());
                written += size;
            }
            fstemp.close();
        }

        // open the file and read the header
        {
            using file_body_type = basic_file_body<File>;

            typename file_body_type::value_type value;
            // opened in write mode so we can truncate later
            value.open(temp.path().string<std::string>().c_str(), file_mode::read, ec);
            BEAST_EXPECTS(! ec, ec.message());

            response_header<> header;
            header.version(11);
            header.result(status::accepted);
            header.set(field::server, "test");
            header.set(field::content_length, to_static_string(4097));

            typename file_body_type::writer w(header, value);
            auto maybe_range = w.get(ec);
            BEAST_EXPECTS(!ec, ec.message());
            BEAST_EXPECTS(maybe_range.has_value(), "no range returned");
            BEAST_EXPECT(maybe_range.value().second);

            value.file().seek(4097, ec);
            BEAST_EXPECTS(!ec, ec.message());

            maybe_range = w.get(ec);
            BEAST_EXPECTS(ec == error::short_read, ec.message());
            BEAST_EXPECTS(!maybe_range.has_value(), "range returned on error");
        }
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

        fileBodyUnexpectedEofOnGet<file_stdio>();
    #if BOOST_BEAST_USE_POSIX_FILE
        fileBodyUnexpectedEofOnGet<file_posix>();
    #endif
    #if BOOST_BEAST_USE_WIN32_FILE
        fileBodyUnexpectedEofOnGet<file_win32>();
    #endif

    }
};

BEAST_DEFINE_TESTSUITE(beast,http,file_body);

} // http
} // beast
} // boost
