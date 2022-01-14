//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TEST_LIGHTWEIGHT_TEST_STREAM_HPP_INCLUDED_
#define BOOST_ATOMIC_TEST_LIGHTWEIGHT_TEST_STREAM_HPP_INCLUDED_

#include <iostream>
#include <boost/config.hpp>

struct test_stream_type
{
    typedef std::ios_base& (*ios_base_manip)(std::ios_base&);
    typedef std::basic_ios< char, std::char_traits< char > >& (*basic_ios_manip)(std::basic_ios< char, std::char_traits< char > >&);
    typedef std::ostream& (*stream_manip)(std::ostream&);

    template< typename T >
    test_stream_type const& operator<< (T const& value) const
    {
        std::cerr << value;
        return *this;
    }

    test_stream_type const& operator<< (ios_base_manip manip) const
    {
        std::cerr << manip;
        return *this;
    }
    test_stream_type const& operator<< (basic_ios_manip manip) const
    {
        std::cerr << manip;
        return *this;
    }
    test_stream_type const& operator<< (stream_manip manip) const
    {
        std::cerr << manip;
        return *this;
    }

    // Make sure characters are printed as numbers if tests fail
    test_stream_type const& operator<< (char value) const
    {
        std::cerr << static_cast< int >(value);
        return *this;
    }
    test_stream_type const& operator<< (signed char value) const
    {
        std::cerr << static_cast< int >(value);
        return *this;
    }
    test_stream_type const& operator<< (unsigned char value) const
    {
        std::cerr << static_cast< unsigned int >(value);
        return *this;
    }
    test_stream_type const& operator<< (short value) const
    {
        std::cerr << static_cast< int >(value);
        return *this;
    }
    test_stream_type const& operator<< (unsigned short value) const
    {
        std::cerr << static_cast< unsigned int >(value);
        return *this;
    }

#if defined(BOOST_HAS_INT128)
    // Some GCC versions don't provide output operators for __int128
    test_stream_type const& operator<< (boost::int128_type const& v) const
    {
        std::cerr << static_cast< long long >(v);
        return *this;
    }
    test_stream_type const& operator<< (boost::uint128_type const& v) const
    {
        std::cerr << static_cast< unsigned long long >(v);
        return *this;
    }
#endif // defined(BOOST_HAS_INT128)
#if defined(BOOST_HAS_FLOAT128)
    // libstdc++ does not provide output operators for __float128
    test_stream_type const& operator<< (boost::float128_type const& v) const
    {
        std::cerr << static_cast< double >(v);
        return *this;
    }
#endif // defined(BOOST_HAS_FLOAT128)
};

const test_stream_type test_stream = {};

#define BOOST_LIGHTWEIGHT_TEST_OSTREAM test_stream

#include <boost/core/lightweight_test.hpp>

#endif // BOOST_ATOMIC_TEST_LIGHTWEIGHT_TEST_STREAM_HPP_INCLUDED_
