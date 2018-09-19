// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>

#if defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)

int main()
{
}

#else

#include <boost/optional.hpp>
#include <utility>

class basic_multi_buffer;

class const_buffers_type
{
    basic_multi_buffer const* b_;

    friend class basic_multi_buffer;

    explicit
    const_buffers_type(basic_multi_buffer const& b);

public:

    const_buffers_type() = delete;
    const_buffers_type(const_buffers_type const&) = default;
    const_buffers_type& operator=(const_buffers_type const&) = default;
};

int main()
{
    boost::optional< std::pair<const_buffers_type, int> > opt, opt2;
    opt = opt2;
}

#endif
