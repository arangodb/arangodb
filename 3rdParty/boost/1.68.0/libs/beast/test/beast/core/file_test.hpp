//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_CORE_FILE_TEST_HPP
#define BOOST_BEAST_TEST_CORE_FILE_TEST_HPP

#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/config.hpp>
#include <boost/filesystem.hpp>
#include <string>

namespace boost {
namespace beast {

template<class File>
void
doTestFile(beast::unit_test::suite& test)
{
    BOOST_STATIC_ASSERT(is_file<File>::value);

    error_code ec;
    auto const temp = boost::filesystem::unique_path();

    {
        {
            File f1;
            test.BEAST_EXPECT(! f1.is_open());
            f1.open(temp.string<std::string>().c_str(), file_mode::write, ec);
            test.BEAST_EXPECT(! ec);
            File f2{std::move(f1)};
            test.BEAST_EXPECT(! f1.is_open());
            test.BEAST_EXPECT(f2.is_open());
            File f3;
            f3 = std::move(f2);
            test.BEAST_EXPECT(! f2.is_open());
            test.BEAST_EXPECT(f3.is_open());
            f1.close(ec);
            test.BEAST_EXPECT(! ec);
            auto const temp2 = boost::filesystem::unique_path();
            f3.open(temp2.string<std::string>().c_str(), file_mode::read, ec);
            test.BEAST_EXPECT(ec);
            ec.assign(0, ec.category());
        }
        boost::filesystem::remove(temp, ec);
        test.BEAST_EXPECT(! ec);
    }

    File f;

    test.BEAST_EXPECT(! f.is_open());

    f.size(ec);
    test.BEAST_EXPECT(ec == errc::invalid_argument);
    ec.assign(0, ec.category());

    f.pos(ec);
    test.BEAST_EXPECT(ec == errc::invalid_argument);
    ec.assign(0, ec.category());

    f.seek(0, ec);
    test.BEAST_EXPECT(ec == errc::invalid_argument);
    ec.assign(0, ec.category());

    f.read(nullptr, 0, ec);
    test.BEAST_EXPECT(ec == errc::invalid_argument);
    ec.assign(0, ec.category());

    f.write(nullptr, 0, ec);
    test.BEAST_EXPECT(ec == errc::invalid_argument);
    ec.assign(0, ec.category());

    f.open(temp.string<std::string>().c_str(), file_mode::write, ec);
    test.BEAST_EXPECT(! ec);

    std::string const s = "Hello, world!";
    f.write(s.data(), s.size(), ec);
    test.BEAST_EXPECT(! ec);

    auto size = f.size(ec);
    test.BEAST_EXPECT(! ec);
    test.BEAST_EXPECT(size == s.size());

    auto pos = f.pos(ec);
    test.BEAST_EXPECT(! ec);
    test.BEAST_EXPECT(pos == size);

    f.close(ec);
    test.BEAST_EXPECT(! ec);

    f.open(temp.string<std::string>().c_str(), file_mode::read, ec);
    test.BEAST_EXPECT(! ec);

    std::string buf;
    buf.resize(s.size());
    f.read(&buf[0], buf.size(), ec);
    test.BEAST_EXPECT(! ec);
    test.BEAST_EXPECT(buf == s);

    f.seek(1, ec);
    test.BEAST_EXPECT(! ec);
    buf.resize(3);
    f.read(&buf[0], buf.size(), ec);
    test.BEAST_EXPECT(! ec);
    test.BEAST_EXPECT(buf == "ell");

    pos = f.pos(ec);
    test.BEAST_EXPECT(! ec);
    test.BEAST_EXPECT(pos == 4);

    f.close(ec);
    test.BEAST_EXPECT(! ec);
    boost::filesystem::remove(temp, ec);
    test.BEAST_EXPECT(! ec);
}

} // beast
} // boost

#endif
