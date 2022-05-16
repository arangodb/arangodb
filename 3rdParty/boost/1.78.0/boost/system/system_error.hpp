#ifndef BOOST_SYSTEM_SYSTEM_ERROR_HPP
#define BOOST_SYSTEM_SYSTEM_ERROR_HPP

// Copyright Beman Dawes 2006
// Copyright Peter Dimov 2021
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/errc.hpp>
#include <boost/system/detail/error_code.hpp>
#include <string>
#include <stdexcept>
#include <cassert>

namespace boost
{
namespace system
{

class BOOST_SYMBOL_VISIBLE system_error: public std::runtime_error
{
private:

    error_code code_;

private:

    static std::string build_message( char const * prefix, error_code const & ec )
    {
        std::string r;

        if( prefix )
        {
            r += prefix;
            r += ": ";
        }

        r += ec.what();
        return r;
    }

    static std::string build_message( char const * prefix, int ev, error_category const & cat )
    {
        return build_message( prefix, error_code( ev, cat ) );
    }

public:

    explicit system_error( error_code const & ec )
      : std::runtime_error( build_message( 0, ec ) ), code_( ec ) {}

    system_error( error_code const & ec, std::string const & prefix )
      : std::runtime_error( build_message( prefix.c_str(), ec ) ), code_( ec ) {}

    system_error( error_code const & ec, char const * prefix )
      : std::runtime_error( build_message( prefix, ec ) ), code_( ec ) {}

    system_error( int ev, error_category const & ecat )
      : std::runtime_error( build_message( 0, ev, ecat ) ), code_( ev, ecat ) {}

    system_error( int ev, error_category const & ecat, std::string const & prefix )
      : std::runtime_error( build_message( prefix.c_str(), ev, ecat ) ), code_( ev, ecat ) {}

    system_error( int ev, error_category const & ecat, char const * prefix )
      : std::runtime_error( build_message( prefix, ev, ecat ) ), code_( ev, ecat ) {}

    error_code code() const BOOST_NOEXCEPT
    {
        return code_;
    }
};

} // namespace system
} // namespace boost

#endif // BOOST_SYSTEM_SYSTEM_ERROR_HPP
