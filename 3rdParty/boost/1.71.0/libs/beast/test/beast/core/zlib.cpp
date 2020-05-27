//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <beast/detail/zlib/deflate_stream.hpp>
#include <beast/detail/zlib/inflate_stream.hpp>

#include <beast/unit_test/suite.hpp>
#include <array>
#include <cassert>
#include <memory>
#include <random>

namespace boost {
namespace beast {
namespace zlib {

class zlib_test : public beast::unit_test::suite
{
public:
    class buffer
    {
        std::size_t size_ = 0;
        std::size_t capacity_ = 0;
        std::unique_ptr<std::uint8_t[]> p_;

    public:
        buffer() = default;
        buffer(buffer&&) = default;
        buffer& operator=(buffer&&) = default;


        explicit
        buffer(std::size_t capacity)
        {
            reserve(capacity);
        }

        bool
        empty() const
        {
            return size_ == 0;
        }

        std::size_t
        size() const
        {
            return size_;
        }

        std::size_t
        capacity() const
        {
            return capacity_;
        }

        std::uint8_t const*
        data() const
        {
            return p_.get();
        }

        std::uint8_t*
        data()
        {
            return p_.get();
        }

        void
        reserve(std::size_t capacity)
        {
            if(capacity != capacity_)
            {
                p_.reset(new std::uint8_t[capacity]);
                capacity_ = capacity;
            }
        }

        void
        resize(std::size_t size)
        {
            assert(size <= capacity_);
            size_ = size;
        }
    };

    buffer
    make_source1(std::size_t size)
    {
        std::mt19937 rng;
        buffer b(size);
        auto p = b.data();
        std::size_t n = 0;
        static std::string const chars(
            "01234567890{}\"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
            "{{{{{{{{{{}}}}}}}}}}  ");
        while(n < size)
        {
            *p++ = chars[rng()%chars.size()];
            ++n;
        }
        b.resize(n);
        return b;
    }

    buffer
    make_source2(std::size_t size)
    {
        std::mt19937 rng;
        std::array<double, 2> const i{0, 65535};
        std::array<double, 2> const w{0, 1};
        std::piecewise_linear_distribution<double> d(
            i.begin(), i.end(), w.begin());
        buffer b(size);
        auto p = b.data();
        std::size_t n = 0;
        while(n < size)
        {
            if(n == 1)
            {
                *p++ = rng()%256;
                ++n;
                continue;
            }
            auto const v = static_cast<std::uint16_t>(d(rng));
            *p++ = v>>8;
            *p++ = v&0xff;
            n += 2;

        }
        b.resize(n);
        return b;
    }

    void
    checkInflate(buffer const& input, buffer const& original)
    {
        for(std::size_t i = 0; i < input.size(); ++i)
        {
            buffer output(original.size());
            inflate_stream zs;
            zs.avail_in = 0;
            zs.next_in = 0;
            zs.next_out = output.data();
            zs.avail_out = output.capacity();
            if(i > 0)
            {
                zs.next_in = (Byte*)input.data();
                zs.avail_in = i;
                auto result = zs.write(Z_FULL_FLUSH);
                expect(result == Z_OK);
            }
            zs.next_in = (Byte*)input.data() + i;
            zs.avail_in = input.size() - i;
            auto result = zs.write(Z_FULL_FLUSH);
            output.resize(output.capacity() - zs.avail_out);
            expect(result == Z_OK);
            expect(output.size() == original.size());
            expect(std::memcmp(
                output.data(), original.data(), original.size()) == 0);
        }
    }

    void testSpecial()
    {
        {
            deflate_stream zs;
        }
        {
            inflate_stream zs;
        }
    }

    void testCompress()
    {
        static std::size_t constexpr N = 2048;
        for(int source = 0; source <= 1; ++source)
        {
            buffer original;
            switch(source)
            {
            case 0:
                original = make_source1(N);
                break;
            case 1:
                original = make_source2(N);
                break;
            }
            for(int level = 0; level <= 9; ++level)
            {
                for(int strategy = 0; strategy <= 4; ++strategy)
                {
                    for(int wbits = 15; wbits <= 15; ++wbits)
                    {
                        deflate_stream zs;
                        zs.avail_in = 0;
                        zs.next_in = 0;
                        expect(deflate_stream::deflateInit2(&zs,
                            level,
                            wbits,
                            4,
                            strategy) == Z_OK);
                        buffer output(deflate_stream::deflateBound(&zs, original.size()));
                        zs.next_in = (Byte*)original.data();
                        zs.avail_in = original.size();
                        zs.next_out = output.data();
                        zs.avail_out = output.capacity();
                        auto result = zs.deflate(Z_FULL_FLUSH);
                        expect(result == Z_OK);
                        output.resize(output.capacity() - zs.avail_out);
                        checkInflate(output, original);
                    }
                }
            }
        }
    }

    void run() override
    {
        testSpecial();
        testCompress();
    }
};

BEAST_DEFINE_TESTSUITE(zlib,core,beast);

} // zlib
} // beast
} // boost
