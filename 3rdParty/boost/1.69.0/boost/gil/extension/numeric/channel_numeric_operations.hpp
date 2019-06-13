//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_NUMERIC_CHANNEL_NUMERIC_OPERATIONS_HPP
#define BOOST_GIL_EXTENSION_NUMERIC_CHANNEL_NUMERIC_OPERATIONS_HPP

#include <functional>

namespace boost { namespace gil {

// Structures for channel-wise numeric operations
// Currently defined structures:
//    channel_plus_t (+), channel_minus_t (-),
//    channel_multiplies_t (*), channel_divides_t (/),
//    channel_plus_scalar_t (+s), channel_minus_scalar_t (-s),
//    channel_multiplies_scalar_t (*s), channel_divides_scalar_t (/s),
//    channel_halves_t (/=2), channel_zeros_t (=0), channel_assigns_t (=)

/// \ingroup ChannelNumericOperations
/// structure for adding one channel to another
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel1,typename Channel2,typename ChannelR>
struct channel_plus_t : public std::binary_function<Channel1,Channel2,ChannelR> {
    ChannelR operator()(typename channel_traits<Channel1>::const_reference ch1,
                        typename channel_traits<Channel2>::const_reference ch2) const {
        return ChannelR(ch1)+ChannelR(ch2);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for subtracting one channel from another
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel1,typename Channel2,typename ChannelR>
struct channel_minus_t : public std::binary_function<Channel1,Channel2,ChannelR> {
    ChannelR operator()(typename channel_traits<Channel1>::const_reference ch1,
                        typename channel_traits<Channel2>::const_reference ch2) const {
        return ChannelR(ch1)-ChannelR(ch2);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for multiplying one channel to another
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel1,typename Channel2,typename ChannelR>
struct channel_multiplies_t : public std::binary_function<Channel1,Channel2,ChannelR> {
    ChannelR operator()(typename channel_traits<Channel1>::const_reference ch1,
                        typename channel_traits<Channel2>::const_reference ch2) const {
        return ChannelR(ch1)*ChannelR(ch2);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for dividing channels
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel1,typename Channel2,typename ChannelR>
struct channel_divides_t : public std::binary_function<Channel1,Channel2,ChannelR> {
    ChannelR operator()(typename channel_traits<Channel1>::const_reference ch1,
                        typename channel_traits<Channel2>::const_reference ch2) const {
        return ChannelR(ch1)/ChannelR(ch2);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for adding a scalar to a channel
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel,typename Scalar,typename ChannelR>
struct channel_plus_scalar_t : public std::binary_function<Channel,Scalar,ChannelR> {
    ChannelR operator()(typename channel_traits<Channel>::const_reference ch,
                        const Scalar& s) const {
        return ChannelR(ch)+ChannelR(s);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for subtracting a scalar from a channel
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel,typename Scalar,typename ChannelR>
struct channel_minus_scalar_t : public std::binary_function<Channel,Scalar,ChannelR> {
    ChannelR operator()(typename channel_traits<Channel>::const_reference ch,
                        const Scalar& s) const {
        return ChannelR(ch-s);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for multiplying a scalar to one channel
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel,typename Scalar,typename ChannelR>
struct channel_multiplies_scalar_t : public std::binary_function<Channel,Scalar,ChannelR> {
    ChannelR operator()(typename channel_traits<Channel>::const_reference ch,
                        const Scalar& s) const {
        return ChannelR(ch)*ChannelR(s);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for dividing a channel by a scalar
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel,typename Scalar,typename ChannelR>
struct channel_divides_scalar_t : public std::binary_function<Channel,Scalar,ChannelR> {
    ChannelR operator()(typename channel_traits<Channel>::const_reference ch,
                        const Scalar& s) const {
        return ChannelR(ch)/ChannelR(s);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for halving a channel
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel>
struct channel_halves_t : public std::unary_function<Channel,Channel> {
    typename channel_traits<Channel>::reference
    operator()(typename channel_traits<Channel>::reference ch) const {
        return ch/=2.0;
    }
};

/// \ingroup ChannelNumericOperations
/// structure for setting a channel to zero
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel>
struct channel_zeros_t : public std::unary_function<Channel,Channel> {
    typename channel_traits<Channel>::reference
    operator()(typename channel_traits<Channel>::reference ch) const {
        return ch=Channel(0);
    }
};

/// \ingroup ChannelNumericOperations
/// structure for assigning one channel to another
/// this is a generic implementation; user should specialize it for better performance
template <typename Channel1,typename Channel2>
struct channel_assigns_t : public std::binary_function<Channel1,Channel2,Channel2> {
    typename channel_traits<Channel2>::reference
    operator()(typename channel_traits<Channel1>::const_reference ch1,
               typename channel_traits<Channel2>::reference ch2) const {
        return ch2=Channel2(ch1);
    }
};

}}  // namespace boost::gil

#endif
