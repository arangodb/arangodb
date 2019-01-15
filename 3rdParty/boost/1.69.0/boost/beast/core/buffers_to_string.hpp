//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_BUFFERS_TO_STRING_HPP
#define BOOST_BEAST_BUFFERS_TO_STRING_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <string>

namespace boost {
namespace beast {

/** Return a string representing the contents of a buffer sequence.

    This function returns a string representing an entire buffer
    sequence. Nulls and unprintable characters in the buffer
    sequence are inserted to the resulting string as-is. No
    character conversions are performed.

    @param buffers The buffer sequence to convert

    @par Example

    This function writes a buffer sequence converted to a string
    to `std::cout`.

    @code
    template<class ConstBufferSequence>
    void print(ConstBufferSequence const& buffers)
    {
        std::cout << buffers_to_string(buffers) << std::endl;
    }
    @endcode
*/
template<class ConstBufferSequence>
inline
std::string
buffers_to_string(ConstBufferSequence const& buffers)
{
    std::string result;
    result.reserve(boost::asio::buffer_size(buffers));
    for(boost::asio::const_buffer buffer :
            detail::buffers_range(buffers))
        result.append(static_cast<
            char const*>(buffer.data()), buffer.size());
    return result;
}

} // beast
} // boost

#endif
