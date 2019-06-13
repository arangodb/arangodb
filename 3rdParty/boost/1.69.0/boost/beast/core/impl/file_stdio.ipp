//
// Copyright (c) 2015-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_FILE_STDIO_IPP
#define BOOST_BEAST_CORE_IMPL_FILE_STDIO_IPP

#include <boost/core/exchange.hpp>
#include <limits>

namespace boost {
namespace beast {

inline
file_stdio::
~file_stdio()
{
    if(f_)
        fclose(f_);
}

inline
file_stdio::
file_stdio(file_stdio&& other)
    : f_(boost::exchange(other.f_, nullptr))
{
}

inline
file_stdio&
file_stdio::
operator=(file_stdio&& other)
{
    if(&other == this)
        return *this;
    if(f_)
        fclose(f_);
    f_ = other.f_;
    other.f_ = nullptr;
    return *this;
}

inline
void
file_stdio::
native_handle(FILE* f)
{
    if(f_)
        fclose(f_);
    f_ = f;
}

inline
void
file_stdio::
close(error_code& ec)
{
    if(f_)
    {
        int failed = fclose(f_);
        f_ = nullptr;
        if(failed)
        {
            ec.assign(errno, generic_category());
            return;
        }
    }
    ec.assign(0, ec.category());
}

inline
void
file_stdio::
open(char const* path, file_mode mode, error_code& ec)
{
    if(f_)
    {
        fclose(f_);
        f_ = nullptr;
    }
    char const* s;
    switch(mode)
    {
    default:
    case file_mode::read:               s = "rb"; break;
    case file_mode::scan:               s = "rb"; break;
    case file_mode::write:              s = "wb"; break;
    case file_mode::write_new:          s = "wbx"; break;
    case file_mode::write_existing:     s = "wb"; break;
    case file_mode::append:             s = "ab"; break;
    case file_mode::append_new:         s = "abx"; break;
    case file_mode::append_existing:    s = "ab"; break;
    }
#if BOOST_MSVC
    auto const ev = fopen_s(&f_, path, s);
    if(ev)
    {
        f_ = nullptr;
        ec.assign(ev, generic_category());
        return;
    }
#else
    f_ = std::fopen(path, s);
    if(! f_)
    {
        ec.assign(errno, generic_category());
        return;
    }
#endif
    ec.assign(0, ec.category());
}

inline
std::uint64_t
file_stdio::
size(error_code& ec) const
{
    if(! f_)
    {
        ec = make_error_code(errc::invalid_argument);
        return 0;
    }
    long pos = std::ftell(f_);
    if(pos == -1L)
    {
        ec.assign(errno, generic_category());
        return 0;
    }
    int result = std::fseek(f_, 0, SEEK_END);
    if(result != 0)
    {
        ec.assign(errno, generic_category());
        return 0;
    }
    long size = std::ftell(f_);
    if(size == -1L)
    {
        ec.assign(errno, generic_category());
        std::fseek(f_, pos, SEEK_SET);
        return 0;
    }
    result = std::fseek(f_, pos, SEEK_SET);
    if(result != 0)
        ec.assign(errno, generic_category());
    else
        ec.assign(0, ec.category());
    return size;
}

inline
std::uint64_t
file_stdio::
pos(error_code& ec) const
{
    if(! f_)
    {
        ec = make_error_code(errc::invalid_argument);
        return 0;
    }
    long pos = std::ftell(f_);
    if(pos == -1L)
    {
        ec.assign(errno, generic_category());
        return 0;
    }
    ec.assign(0, ec.category());
    return pos;
}

inline
void
file_stdio::
seek(std::uint64_t offset, error_code& ec)
{
    if(! f_)
    {
        ec = make_error_code(errc::invalid_argument);
        return;
    }
    if(offset > (std::numeric_limits<long>::max)())
    {
        ec = make_error_code(errc::invalid_seek);
        return;
    }
    int result = std::fseek(f_,
        static_cast<long>(offset), SEEK_SET);
    if(result != 0)
        ec.assign(errno, generic_category());
    else
        ec.assign(0, ec.category());
}

inline
std::size_t
file_stdio::
read(void* buffer, std::size_t n, error_code& ec) const
{
    if(! f_)
    {
        ec = make_error_code(errc::invalid_argument);
        return 0;
    }
    auto nread = std::fread(buffer, 1, n, f_);
    if(std::ferror(f_))
    {
        ec.assign(errno, generic_category());
        return 0;
    }
    return nread;
}

inline
std::size_t
file_stdio::
write(void const* buffer, std::size_t n, error_code& ec)
{
    if(! f_)
    {
        ec = make_error_code(errc::invalid_argument);
        return 0;
    }
    auto nwritten = std::fwrite(buffer, 1, n, f_);
    if(std::ferror(f_))
    {
        ec.assign(errno, generic_category());
        return 0;
    }
    return nwritten;
}

} // beast
} // boost

#endif
