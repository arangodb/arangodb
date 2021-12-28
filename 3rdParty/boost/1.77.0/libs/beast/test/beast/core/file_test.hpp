//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_FILE_TEST_HPP
#define BOOST_BEAST_FILE_TEST_HPP

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/file_base.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <cstdio>
#include <string>
#include <type_traits>

namespace boost {
namespace beast {

template<class File, bool append_unicode_suffix = false>
void
test_file()
{
    BOOST_STATIC_ASSERT(
        is_file<File>::value);
    BOOST_STATIC_ASSERT(
        ! std::is_copy_constructible<File>::value);
    BOOST_STATIC_ASSERT(
        ! std::is_copy_assignable<File>::value);

    namespace fs = boost::filesystem;

    static constexpr
#ifdef _WIN32
    boost::winapi::WCHAR_ unicode_suffix[] = { 0xd83e, 0xdd84, 0x0000 }; // UTF-16-LE unicorn
#else
    char                  unicode_suffix[] = { '\xf0', '\x9f', '\xa6', '\x84', '\x00' }; // UTF-8 unicorn
#endif

    class temp_path
    {
        fs::path path_;
        std::vector<char> utf8_str_;

    public:
        temp_path()
            : path_(fs::unique_path())
        {
            if (append_unicode_suffix)
                path_ += unicode_suffix;
#ifdef _WIN32
            constexpr auto cp = boost::winapi::CP_UTF8_;
            constexpr auto flags = boost::winapi::WC_ERR_INVALID_CHARS_;
            auto sz = boost::winapi::WideCharToMultiByte(
                cp, flags, path_.c_str(), -1, nullptr, 0,
                nullptr, nullptr);
            BEAST_EXPECT(sz != 0);
            utf8_str_.resize(sz);
            auto ret = boost::winapi::WideCharToMultiByte(
                cp, flags, path_.c_str(), -1,
                utf8_str_.data(), sz,
                nullptr, nullptr);
            BEAST_EXPECT(ret == sz);
#endif
        }

        operator fs::path const&()
        {
            return path_;
        }

        operator char const*()
        {
#ifdef _WIN32
            return utf8_str_.data();
#else
            return path_.c_str();
#endif
        }
    };

    auto const create =
        [](fs::path const& path)
        {
            BEAST_EXPECT(! fs::exists(path));
            fs::ofstream out(path);
            BEAST_EXPECT(out.is_open());
        };

    auto const remove =
        [](fs::path const& path)
        {
            fs::remove(path);
            BEAST_EXPECT(! fs::exists(path));
        };

    temp_path path;

    // bad file descriptor
    {
        File f;
        char buf[1];
        BEAST_EXPECT(! f.is_open());
        BEAST_EXPECT(! fs::exists(path));
        {
            error_code ec;
            f.size(ec);
            BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
        {
            error_code ec;
            f.pos(ec);
            BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
        {
            error_code ec;
            f.seek(0, ec);
            BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
        {
            error_code ec;
            f.read(buf, 0, ec);
            BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
        {
            error_code ec;
            f.write(buf, 0, ec);
            BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
    }

    // file_mode::read
    {
        {
            File f;
            error_code ec;
            create(path);
            f.open(path, file_mode::read, ec);
            BEAST_EXPECT(! ec);
        }
        remove(path);
    }
    
    // file_mode::scan
    {
        {
            File f;
            error_code ec;
            create(path);
            f.open(path, file_mode::scan, ec);
            BEAST_EXPECT(! ec);
        }
        remove(path);
    }
    
    // file_mode::write
    {
        {
            File f;
            error_code ec;
            BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fs::exists(path));
        }
        {
            File f;
            error_code ec;
            BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fs::exists(path));
        }
        remove(path);
    }
    
    // file_mode::write_new
    {
        {
            File f;
            error_code ec;
            BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::write_new, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(fs::exists(path));
        }
        {
            File f;
            error_code ec;
            BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::write_new, ec);
            BEAST_EXPECT(ec);
        }
        remove(path);
    }
    
    // file_mode::write_existing
    {
        {
            File f;
            error_code ec;
            BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::write_existing, ec);
            BEAST_EXPECT(ec);
            BEAST_EXPECT(! fs::exists(path));
        }
        {
            File f;
            error_code ec;
            create(path);
            BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::write_existing, ec);
            BEAST_EXPECT(! ec);
        }
        remove(path);
    }
    
    // file_mode::append
    {
        {
            File f;
            error_code ec;
            BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::append, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fs::exists(path));
        }
        {
            File f;
            error_code ec;
            BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::append, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(fs::exists(path));
        }
        remove(path);
    }
        
    // file_mode::append_existing
    {
        {
            File f;
            error_code ec;
            BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::append_existing, ec);
            BEAST_EXPECT(ec);
            BEAST_EXPECT(! fs::exists(path));
        }
        remove(path);
        {
            File f;
            error_code ec;
            create(path);
            BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::append_existing, ec);
            BEAST_EXPECT(! ec);
        }
        remove(path);
    }

    // special members
    {
        {
            File f1;
            error_code ec;
            f1.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(f1.is_open());

            // move constructor
            File f2(std::move(f1));
            BEAST_EXPECT(! f1.is_open());
            BEAST_EXPECT(f2.is_open());

            // move assignment
            File f3;
            f3 = std::move(f2);
            BEAST_EXPECT(! f2.is_open());
            BEAST_EXPECT(f3.is_open());
        }
        remove(path);
    }

    // re-open
    {
        {
            File f;
            error_code ec;
            f.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);
            f.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);
        }
        remove(path);
    }

    // re-assign
    {
        temp_path path2;
        {
            error_code ec;

            File f1;
            f1.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);

            File f2;
            f2.open(path2, file_mode::write, ec);
            BEAST_EXPECT(! ec);

            f2 = std::move(f1);
            BEAST_EXPECT(! f1.is_open());
            BEAST_EXPECT(f2.is_open());
        }
        remove(path);
        remove(path2);
    }

    // self-move
    {
        {
            File f;
            error_code ec;
            f.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);
            f = std::move(f);
            BEAST_EXPECT(f.is_open());
        }
        remove(path);
    }

    // native_handle
    {
        {
            File f;
            auto none = f.native_handle();
            error_code ec;
            f.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);
            auto fd = f.native_handle();
            BEAST_EXPECT(fd != none);
            f.native_handle(none);
            BEAST_EXPECT(! f.is_open());
        }
        remove(path);
    }

    // read and write
    {
        string_view const s = "Hello, world!";

        // write
        {
            File f;
            error_code ec;
            f.open(path, file_mode::write, ec);
            BEAST_EXPECT(! ec);

            f.write(s.data(), s.size(), ec);
            BEAST_EXPECT(! ec);

            auto size = f.size(ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(size == s.size());

            auto pos = f.pos(ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(pos == size);

            f.close(ec);
            BEAST_EXPECT(! ec);
        }

        // read
        {
            File f;
            error_code ec;
            f.open(path, file_mode::read, ec);
            BEAST_EXPECT(! ec);

            std::string buf;
            buf.resize(s.size());
            f.read(&buf[0], buf.size(), ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(buf == s);

            f.seek(1, ec);
            BEAST_EXPECT(! ec);
            buf.resize(3);
            f.read(&buf[0], buf.size(), ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(buf == "ell");

            auto pos = f.pos(ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(pos == 4);
        }
        remove(path);
    }

    BEAST_EXPECT(! fs::exists(path));
}

} // beast
} // boost

#endif
