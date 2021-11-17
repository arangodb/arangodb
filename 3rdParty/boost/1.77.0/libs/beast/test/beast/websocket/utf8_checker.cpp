//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/detail/utf8_checker.hpp>

#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <array>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

class utf8_checker_test : public beast::unit_test::suite
{
public:
    void
    testOneByteSequence()
    {
        // valid single-char code points
        for(unsigned char c = 0; c < 128; ++c)
        {
            utf8_checker u;
            BEAST_EXPECT(u.write(&c, 1));
            BEAST_EXPECT(u.finish());
        }

        // invalid lead bytes
        for(unsigned char c = 128; c < 192; ++c)
        {
            utf8_checker u;
            BEAST_EXPECT(! u.write(&c, 1));
        }

        // two byte sequences
        for(unsigned char c = 192; c < 224; ++c)
        {
            // fail fast
            utf8_checker u;
            if (c < 194)
                BEAST_EXPECT(! u.write(&c, 1));
            else
            {
                BEAST_EXPECT(u.write(&c, 1));
                BEAST_EXPECT(! u.finish());
            }
        }

        // three byte sequences
        for(unsigned char c = 224; c < 240; ++c)
        {
            utf8_checker u;
            BEAST_EXPECT(u.write(&c, 1));
            BEAST_EXPECT(! u.finish());
        }

        // four byte sequences
        for(unsigned char c = 240; c < 245; ++c)
        {
            // fail fast
            utf8_checker u;
            BEAST_EXPECT(u.write(&c, 1));
            BEAST_EXPECT(! u.finish());
        }

        // invalid lead bytes
        for(unsigned char c = 245; c; ++c)
        {
            utf8_checker u;
            BEAST_EXPECT(! u.write(&c, 1));
        }
    }

    void
    testTwoByteSequence()
    {
        // Autobahn 6.18.1
        {
            utf8_checker u;
            BEAST_EXPECT(! u.write(net::buffer("\xc1\xbf", 2)));
        }

        utf8_checker u;
        std::uint8_t buf[2];
        // First byte valid range 194-223
        for(auto i : {194, 223})
        {
            buf[0] = static_cast<std::uint8_t>(i);

            // Second byte valid range 128-191
            for(auto j : {128, 191})
            {
                buf[1] = static_cast<std::uint8_t>(j);
                BEAST_EXPECT(u.write(buf, 2));
                BEAST_EXPECT(u.finish());
            }

            // Second byte invalid range 0-127
            for(auto j : {0, 127})
            {
                buf[1] = static_cast<std::uint8_t>(j);
                BEAST_EXPECT(! u.write(buf, 2));
                u.reset();
            }

            // Second byte invalid range 192-255
            for(auto j : {192, 255})
            {
                buf[1] = static_cast<std::uint8_t>(j);
                BEAST_EXPECT(! u.write(buf, 2));
                u.reset();
            }

            // Segmented sequence second byte invalid
            BEAST_EXPECT(u.write(buf, 1));
            BEAST_EXPECT(! u.write(&buf[1], 1));
            u.reset();
        }
    }

    void
    testThreeByteSequence()
    {
        {
            utf8_checker u;
            BEAST_EXPECT(u.write(net::buffer("\xef\xbf\xbf", 3)));
            BEAST_EXPECT(u.finish());
        }
        utf8_checker u;
        std::uint8_t buf[3];
        // First byte valid range 224-239
        for(auto i : {224, 239})
        {
            buf[0] = static_cast<std::uint8_t>(i);

            // Second byte valid range 128-191 or 160-191 or 128-159
            std::int32_t const b = (i == 224 ? 160 : 128);
            std::int32_t const e = (i == 237 ? 159 : 191);
            for(auto j : {b, e})
            {
                buf[1] = static_cast<std::uint8_t>(j);

                // Third byte valid range 128-191
                for(auto k : {128, 191})
                {
                    buf[2] = static_cast<std::uint8_t>(k);
                    BEAST_EXPECT(u.write(buf, 3));
                    BEAST_EXPECT(u.finish());
                    // Segmented sequence
                    if (i == 224) 
                    {
                        BEAST_EXPECT(u.write(buf, 1));
                        BEAST_EXPECT(!u.finish());
                    }
                    else
                    {
                        BEAST_EXPECT(u.write(buf, 1));
                        BEAST_EXPECT(u.write(&buf[1], 2));
                    }
                    u.reset();
                    // Segmented sequence
                    BEAST_EXPECT(u.write(buf, 2));
                    BEAST_EXPECT(u.write(&buf[2], 1));
                    u.reset();

                    if (i == 224)
                    {
                        // Second byte invalid range 0-159
                        for (auto l : {0, 159})
                        {
                            buf[1] = static_cast<std::uint8_t>(l);
                            BEAST_EXPECT(! u.write(buf, 3));
                            u.reset();
                            // Segmented sequence second byte invalid
                            BEAST_EXPECT(!u.write(buf, 2));
                            u.reset();
                        }
                        // Second byte invalid range 192-255
                        for(auto l : {192, 255})
                        {
                            buf[1] = static_cast<std::uint8_t>(l);
                            BEAST_EXPECT(! u.write(buf, 3));
                            u.reset();
                            // Segmented sequence second byte invalid
                            BEAST_EXPECT(!u.write(buf, 2));
                            u.reset();
                        }
                        buf[1] = static_cast<std::uint8_t>(j);
                    }
                    else if (i == 237)
                    {
                        // Second byte invalid range 0-127
                        for(auto l : {0, 127})
                        {
                            buf[1] = static_cast<std::uint8_t>(l);
                            BEAST_EXPECT(! u.write(buf, 3));
                            u.reset();
                            // Segmented sequence second byte invalid
                            BEAST_EXPECT(!u.write(buf, 2));
                            u.reset();
                        }

                        // Second byte invalid range 160-255
                        for(auto l : {160, 255})
                        {
                            buf[1] = static_cast<std::uint8_t>(l);
                            BEAST_EXPECT(! u.write(buf, 3));
                            u.reset();
                            // Segmented sequence second byte invalid
                            BEAST_EXPECT(! u.write(buf, 2));
                            u.reset();
                        }
                        buf[1] = static_cast<std::uint8_t>(j);
                    }
                }

                // Third byte invalid range 0-127
                for(auto k : {0, 127})
                {
                    buf[2] = static_cast<std::uint8_t>(k);
                    BEAST_EXPECT(! u.write(buf, 3));
                    u.reset();
                }

                // Third byte invalid range 192-255
                for(auto k : {192, 255})
                {
                    buf[2] = static_cast<std::uint8_t>(k);
                    BEAST_EXPECT(! u.write(buf, 3));
                    u.reset();
                }

                // Segmented sequence third byte invalid
                BEAST_EXPECT(u.write(buf, 2));
                BEAST_EXPECT(! u.write(&buf[2], 1));
                u.reset();
            }

            // Second byte invalid range 0-127 or 0-159
            for(auto j : {0, b - 1})
            {
                buf[1] = static_cast<std::uint8_t>(j);
                BEAST_EXPECT(! u.write(buf, 3));
                u.reset();
            }

            // Second byte invalid range 160-255 or 192-255
            for(auto j : {e + 1, 255})
            {
                buf[1] = static_cast<std::uint8_t>(j);
                BEAST_EXPECT(! u.write(buf, 3));
                u.reset();
            }

            // Segmented sequence second byte invalid
            if (i == 224) {
                BEAST_EXPECT(u.write(buf, 1));
                BEAST_EXPECT(!u.finish());
            }
            else
            {
                BEAST_EXPECT(u.write(buf, 1));
                BEAST_EXPECT(!u.write(&buf[1], 1));
            }
            u.reset();
        }
    }

    void
    testFourByteSequence()
    {
        using net::const_buffer;
        utf8_checker u;
        std::uint8_t buf[4];
        // First byte valid range 240-244
        for(auto i : {240, 244})
        {
            buf[0] = static_cast<std::uint8_t>(i);

            std::int32_t const b = (i == 240 ? 144 : 128);
            std::int32_t const e = (i == 244 ? 143 : 191);
            for(auto j = b; j <= e; ++j)
            {
                buf[1] = static_cast<std::uint8_t>(j);

                // Second byte valid range 144-191 or 128-191 or 128-143
                for(auto k : {128, 191})
                {
                    buf[2] = static_cast<std::uint8_t>(k);

                    // Third byte valid range 128-191
                    for(auto n : {128, 191})
                    {
                        // Fourth byte valid range 128-191
                        buf[3] = static_cast<std::uint8_t>(n);
                        BEAST_EXPECT(u.write(buf, 4));
                        BEAST_EXPECT(u.finish());
                        // Segmented sequence
                        BEAST_EXPECT(u.write(buf, 1));
                        BEAST_EXPECT(u.write(&buf[1], 3));
                        u.reset();
                        // Segmented sequence
                        BEAST_EXPECT(u.write(buf, 2));
                        BEAST_EXPECT(u.write(&buf[2], 2));
                        u.reset();
                        // Segmented sequence
                        BEAST_EXPECT(u.write(buf, 3));
                        BEAST_EXPECT(u.write(&buf[3], 1));
                        u.reset();

                        if (i == 240)
                        {
                            // Second byte invalid range 0-143
                            for(auto r : {0, 143})
                            {
                                buf[1] = static_cast<std::uint8_t>(r);
                                BEAST_EXPECT(! u.write(buf, 4));
                                u.reset();
                                // Segmented sequence second byte invalid
                                BEAST_EXPECT(! u.write(buf, 2));
                                u.reset();
                            }

                            // Second byte invalid range 192-255
                            for(auto r : {192, 255})
                            {
                                buf[1] = static_cast<std::uint8_t>(r);
                                BEAST_EXPECT(! u.write(buf, 4));
                                u.reset();
                                // Segmented sequence second byte invalid
                                BEAST_EXPECT(!u.write(buf, 2));
                                u.reset();
                            }
                            buf[1] = static_cast<std::uint8_t>(j);
                        }
                        else if (i == 244)
                        {
                            // Second byte invalid range 0-127
                            for(auto r : {0, 127})
                            {
                                buf[1] = static_cast<std::uint8_t>(r);
                                BEAST_EXPECT(! u.write(buf, 4));
                                u.reset();
                                // Segmented sequence second byte invalid
                                BEAST_EXPECT(! u.write(buf, 2));
                                u.reset();
                            }
                            // Second byte invalid range 144-255
                            for(auto r : {144, 255})
                            {
                                buf[1] = static_cast<std::uint8_t>(r);
                                BEAST_EXPECT(! u.write(buf, 4));
                                u.reset();
                                // Segmented sequence second byte invalid
                                BEAST_EXPECT(! u.write(buf, 2));
                                u.reset();
                            }
                            buf[1] = static_cast<std::uint8_t>(j);
                        }
                    }

                    // Fourth byte invalid ranges 0-127, 192-255
                    for(auto r : {0, 127, 192, 255})
                    {
                        buf[3] = static_cast<std::uint8_t>(r);
                        BEAST_EXPECT(! u.write(buf, 4));
                        u.reset();
                    }

                    // Segmented sequence fourth byte invalid
                    BEAST_EXPECT(u.write(buf, 3));
                    BEAST_EXPECT(! u.write(&buf[3], 1));
                    u.reset();
                }

                // Third byte invalid ranges 0-127, 192-255
                for(auto r : {0, 127, 192, 255})
                {
                    buf[2] = static_cast<std::uint8_t>(r);
                    BEAST_EXPECT(! u.write(buf, 4));
                    u.reset();
                }

                // Segmented sequence third byte invalid
                BEAST_EXPECT(u.write(buf, 2));
                BEAST_EXPECT(! u.write(&buf[2], 1));
                u.reset();
            }

            // Second byte invalid range 0-127 or 0-143
            for(auto r : {0, b - 1})
            {
                buf[1] = static_cast<std::uint8_t>(r);
                BEAST_EXPECT(! u.write(buf, 4));
                u.reset();
            }

            // Second byte invalid range 144-255 or 192-255
            for(auto r : {e + 1, 255})
            {
                buf[1] = static_cast<std::uint8_t>(r);
                BEAST_EXPECT(! u.write(buf, 4));
                u.reset();
            }

            // Segmented sequence second byte invalid
            BEAST_EXPECT(u.write(buf, 1));
            BEAST_EXPECT(! u.write(&buf[1], 1));

            u.reset();
        }

        // First byte invalid range 245-255
        for(auto r : {245, 255})
        {
            buf[0] = static_cast<std::uint8_t>(r);
            BEAST_EXPECT(! u.write(buf, 4));
            u.reset();
        }
    }

    void
    testWithStreamBuffer()
    {
        {
            // Valid UTF8 encoded text
            std::vector<std::vector<std::uint8_t>> const data{{
                    0x48,0x65,0x69,0x7A,0xC3,0xB6,0x6C,0x72,0xC3,0xBC,0x63,0x6B,
                    0x73,0x74,0x6F,0xC3,0x9F,0x61,0x62,0x64,0xC3,0xA4,0x6D,0x70,
                    0x66,0x75,0x6E,0x67
                }, {
                    0xCE,0x93,0xCE,0xB1,0xCE,0xB6,0xCE,0xAD,0xCE,0xB5,0xCF,0x82,
                    0x20,0xCE,0xBA,0xCE,0xB1,0xE1,0xBD,0xB6,0x20,0xCE,0xBC,0xCF,
                    0x85,0xCF,0x81,0xCF,0x84,0xCE,0xB9,0xE1,0xBD,0xB2,0xCF,0x82,
                    0x20,0xCE,0xB4,0xE1,0xBD,0xB2,0xCE,0xBD,0x20,0xCE,0xB8,0xE1,
                    0xBD,0xB0,0x20,0xCE,0xB2,0xCF,0x81,0xE1,0xBF,0xB6,0x20,0xCF,
                    0x80,0xCE,0xB9,0xE1,0xBD,0xB0,0x20,0xCF,0x83,0xCF,0x84,0xE1,
                    0xBD,0xB8,0x20,0xCF,0x87,0xCF,0x81,0xCF,0x85,0xCF,0x83,0xCE,
                    0xB1,0xCF,0x86,0xE1,0xBD,0xB6,0x20,0xCE,0xBE,0xCE,0xAD,0xCF,
                    0x86,0xCF,0x89,0xCF,0x84,0xCE,0xBF
                }, {
                    0xC3,0x81,0x72,0x76,0xC3,0xAD,0x7A,0x74,0xC5,0xB1,0x72,0xC5,
                    0x91,0x20,0x74,0xC3,0xBC,0x6B,0xC3,0xB6,0x72,0x66,0xC3,0xBA,
                    0x72,0xC3,0xB3,0x67,0xC3,0xA9,0x70
                }, {
                    240, 144, 128, 128
                }
            };
            utf8_checker u;
            for(auto const& s : data)
            {
                static std::size_t constexpr size = 3;
                std::size_t n = s.size();
                buffers_suffix<
                    net::const_buffer> cb{
                        net::const_buffer(s.data(), n)};
                multi_buffer b;
                while(n)
                {
                    auto const amount = (std::min)(n, size);
                    b.commit(net::buffer_copy(
                        b.prepare(amount), cb));
                    cb.consume(amount);
                    n -= amount;
                }
                BEAST_EXPECT(u.write(b.data()));
                BEAST_EXPECT(u.finish());
            }
        }
    }

    void
    testBranches()
    {
        // switch to slow loop from alignment loop
        {
            char buf[32];
            for(unsigned i = 0; i < sizeof(buf); i += 2)
            {
                buf[i  ] = '\xc2';
                buf[i+1] = '\x80';
            }
            auto p = reinterpret_cast<char const*>(sizeof(std::size_t) * (
                (std::uintptr_t(buf) + sizeof(std::size_t) - 1) /
                    sizeof(std::size_t))) + 2;
            utf8_checker u;
            BEAST_EXPECT(u.write(
                reinterpret_cast<std::uint8_t const*>(p),
                    sizeof(buf)-(p-buf)));
            BEAST_EXPECT(u.finish());
        }

        // invalid code point in the last dword of a fast run
        {
            char buf[20];
            auto p = reinterpret_cast<char*>(sizeof(std::size_t) * (
                (std::uintptr_t(buf) + sizeof(std::size_t) - 1) /
                    sizeof(std::size_t)));
            BOOST_ASSERT(p + 12 <= buf + sizeof(buf));
            auto const in = p;
            *p++ = '*'; *p++ = '*'; *p++ = '*'; *p++ = '*';
            *p++ = '*'; *p++ = '*'; *p++ = '*'; *p++ = '*';
            p[0] = '\x80'; // invalid
            p[1] = '*';
            p[2] = '*';
            p[3] = '*';
            utf8_checker u;
            BEAST_EXPECT(! u.write(reinterpret_cast<
                std::uint8_t const*>(in), 12));
        }
    }

    void 
    AutodeskTests() 
    {
        std::vector<std::vector<std::uint8_t>> const data{
            { 's','t','a','r','t', 0xE0 },
            { 0xA6, 0x81, 'e','n','d' } };
        utf8_checker u;
        for(auto const& s : data)
        {
            std::size_t n = s.size();
            buffers_suffix<net::const_buffer> cb{net::const_buffer(s.data(), n)};
            multi_buffer b;
            while(n)
            {
                auto const amount = (std::min)(n, std::size_t(3)/*size*/);
                b.commit(net::buffer_copy(b.prepare(amount), cb));
                cb.consume(amount);
                n -= amount;
            }
            BEAST_EXPECT(u.write(b.data()));
        }
        BEAST_EXPECT(u.finish());
    }

    void 
    AutobahnTest(std::vector<std::vector<std::uint8_t>>&& data, std::vector<bool> result) 
    {
        BEAST_EXPECT(data.size() == result.size());
        utf8_checker u;
        for(std::size_t i = 0; i < data.size(); ++i)
        {
            auto const& s = data[i];

            std::size_t n = s.size();
            buffers_suffix<net::const_buffer> cb{net::const_buffer(s.data(), n)};
            multi_buffer b;
            while(n)
            {
                auto const amount = (std::min)(n, std::size_t(3)/*size*/);
                b.commit(net::buffer_copy(b.prepare(amount), cb));
                cb.consume(amount);
                n -= amount;
            }
            BEAST_EXPECT(u.write(b.data()) == result[i]);
        }
    }

    void
    run() override
    {
        testOneByteSequence();
        testTwoByteSequence();
        testThreeByteSequence();
        testFourByteSequence();
        testWithStreamBuffer();
        testBranches();
        AutodeskTests();
        // 6.4.2
        AutobahnTest(std::vector<std::vector<std::uint8_t>>{
            { 0xCE, 0xBA, 0xE1, 0xBD, 0xB9, 0xCF, 0x83, 0xCE, 0xBC, 0xCE, 0xB5, 0xF4 },
            { 0x90 }, { 0x80, 0x80, 0x65, 0x64, 0x69, 0x74, 0x65, 0x64 } },
            { true, false, false});
        // 6.4.4
        AutobahnTest(std::vector<std::vector<std::uint8_t>>{
            { 0xCE, 0xBA, 0xE1, 0xBD, 0xB9, 0xCF, 0x83, 0xCE, 0xBC, 0xCE, 0xB5, 0xF4 },
            { 0x90 } },
            { true, false });
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,utf8_checker);

} // detail
} // websocket
} // beast
} // boost
