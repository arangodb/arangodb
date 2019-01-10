//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_SERVICE_BASE_HPP
#define BOOST_BEAST_CORE_DETAIL_SERVICE_BASE_HPP

#include <boost/asio/execution_context.hpp>

namespace boost {
namespace beast {
namespace detail {

template<class T>
class service_id : public boost::asio::execution_context::id
{
};

template<class T>
class service_base
    : public boost::asio::execution_context::service
{
protected:
    boost::asio::execution_context& ctx_;

public:
    static service_id<T> id;

    explicit
    service_base(boost::asio::execution_context& ctx)
        : boost::asio::execution_context::service(ctx)
        , ctx_(ctx)
    {
    }
};

template<class T>
service_id<T>
service_base<T>::id;

} // detail
} // beast
} // boost

#endif
