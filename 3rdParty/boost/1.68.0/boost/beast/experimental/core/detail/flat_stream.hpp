//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_FLAT_STREAM_HPP
#define BOOST_BEAST_CORE_DETAIL_FLAT_STREAM_HPP

#include <boost/asio/buffer.hpp>
#include <cstdlib>
#include <iterator>

namespace boost {
namespace beast {
namespace detail {

class flat_stream_base
{
public:
    // Largest buffer size we will flatten.
    // 16KB is the upper limit on reasonably sized HTTP messages.
    static std::size_t constexpr coalesce_limit = 16 * 1024;

    // calculates the coalesce settings for a buffer sequence
    template<class BufferSequence>
    static
    std::pair<std::size_t, bool>
    coalesce(BufferSequence const& buffers, std::size_t limit)
    {
        std::pair<std::size_t, bool> result{0, false};
        auto first = boost::asio::buffer_sequence_begin(buffers);
        auto last = boost::asio::buffer_sequence_end(buffers);
        if(first != last)
        {
            result.first = boost::asio::buffer_size(*first);
            if(result.first < limit)
            {
                auto it = first;
                auto prev = first;
                while(++it != last)
                {
                    auto const n =
                        boost::asio::buffer_size(*it);
                    if(result.first + n > limit)
                        break;
                    result.first += n;
                    prev = it;
                }
                result.second = prev != first;
            }
        }
        return result;
    }
};

} // detail
} // beast
} // boost

#endif
