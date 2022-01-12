//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_EXAMPLE_FILE_HPP
#define BOOST_JSON_EXAMPLE_FILE_HPP

#include <boost/json/detail/config.hpp>
#include <cstdio>
#include <string>

class file
{
    FILE* f_ = nullptr;
    long size_ = 0;

    void
    fail(boost::json::error_code& ec)
    {
        ec.assign( errno, boost::json::generic_category() );
    }

public:
    ~file()
    {
        if(f_)
            std::fclose(f_);
    }

    file() = default;

    file( file&& other ) noexcept
        : f_(other.f_)
    {
        other.f_ = nullptr;
    }

    file( char const* path, char const* mode )
    {
        open( path, mode );
    }

    file&
    operator=(file&& other) noexcept
    {
        close();
        f_ = other.f_;
        other.f_ = nullptr;
        return *this;
    }

    void
    close()
    {
        if(f_)
        {
            std::fclose(f_);
            f_ = nullptr;
            size_ = 0;
        }
    }

    void
    open(
        char const* path,
        char const* mode,
        boost::json::error_code& ec)
    {
        close();
        f_ = std::fopen( path, mode );
        if( ! f_ )
            return fail(ec);
        if( std::fseek( f_, 0, SEEK_END ) != 0)
            return fail(ec);
        size_ = std::ftell( f_ );
        if( size_ == -1 )
        {
            size_ = 0;
            return fail(ec);
        }
        if( std::fseek( f_, 0, SEEK_SET ) != 0)
            return fail(ec);
    }

    void
    open(
        char const* path,
        char const* mode)
    {
        boost::json::error_code ec;
        open(path, mode, ec);
        if(ec)
            throw boost::json::system_error(ec);
    }

    long
    size() const noexcept
    {
        return size_;
    }

    bool
    eof() const noexcept
    {
        return std::feof( f_ ) != 0;
    }

    std::size_t
    read( char* data, std::size_t size, boost::json::error_code& ec)
    {
        auto const nread = std::fread( data, 1, size, f_ );
        if( std::ferror(f_) )
            ec.assign( errno, boost::json::generic_category() );
        return nread;
    }

    std::size_t
    read( char* data, std::size_t size )
    {
        boost::json::error_code ec;
        auto const nread = read( data, size, ec );
        if(ec)
            throw boost::json::system_error(ec);
        return nread;
    }
};

inline
std::string
read_file( char const* path, boost::json::error_code& ec )
{
    file f;
    f.open( path, "r", ec );
    if(ec)
        return {};
    std::string s;
    s.resize( f.size() );
    s.resize( f.read( &s[0], s.size(), ec) );
    if(ec)
        return {};
    return s;
}

inline
std::string
read_file( char const* path )
{
    boost::json::error_code ec;
    auto s = read_file( path, ec);
    if(ec)
        throw boost::json::system_error(ec);
    return s;
}

#endif
