//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_FUZZ_HPP
#define BOOST_BEAST_TEST_FUZZ_HPP

#include <boost/beast/core/static_string.hpp>
#include <boost/beast/core/string.hpp>
#include <random>

namespace boost {
namespace beast {
namespace test {

class fuzz_rand
{
    std::mt19937 rng_;

public:
    std::mt19937&
    rng()
    {
        return rng_;
    }

    template<class Unsigned>
    Unsigned
    operator()(Unsigned n)
    {
        return static_cast<Unsigned>(
            std::uniform_int_distribution<
                Unsigned>{0, n-1}(rng_));
    }
};

template<std::size_t N, class Rand, class F>
static
void
fuzz(
    static_string<N> const& input,
    std::size_t repeat,
    std::size_t depth,
    Rand& r,
    F const& f)
{
    static_string<N> mod{input};
    for(auto i = repeat; i; --i)
    {
        switch(r(4))
        {
        case 0: // insert
            if(mod.size() >= mod.max_size())
                continue;
            mod.insert(r(mod.size() + 1), 1,
                static_cast<char>(r(256)));
            break;

        case 1: // erase
            if(mod.size() == 0)
                continue;
            mod.erase(r(mod.size()), 1);
            break;

        case 2: // swap
        {
            if(mod.size() <= 1)
                continue;
            auto off = r(mod.size() - 1);
            auto const temp = mod[off];
            mod[off] = mod[off + 1];
            mod[off + 1] = temp;
            break;
        }
        case 3: // repeat
        {
            if(mod.empty())
                continue;
            auto n = (std::min)(
                std::geometric_distribution<
                    std::size_t>{}(r.rng()),
                mod.max_size() - mod.size());
            if(n == 0)
                continue;
            auto off = r(mod.size());
            mod.insert(off, n, mod[off + 1]);
            break;
        }
        }
        f(string_view{mod.data(), mod.size()});
        if(depth > 0)
            fuzz(mod, repeat, depth - 1, r, f);
    }
}

} // test
} // beast
} // boost

#endif
