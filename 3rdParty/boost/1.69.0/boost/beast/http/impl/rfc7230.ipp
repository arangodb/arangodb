//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_RFC7230_IPP
#define BOOST_BEAST_HTTP_IMPL_RFC7230_IPP

#include <boost/beast/http/detail/rfc7230.hpp>
#include <iterator>

namespace boost {
namespace beast {
namespace http {

class param_list::const_iterator
{
    using iter_type = string_view::const_iterator;

    std::string s_;
    detail::param_iter pi_;

public:
    using value_type = param_list::value_type;
    using pointer = value_type const*;
    using reference = value_type const&;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    const_iterator() = default;

    bool
    operator==(const_iterator const& other) const
    {
        return
            other.pi_.it == pi_.it &&
            other.pi_.last == pi_.last &&
            other.pi_.first == pi_.first;
    }

    bool
    operator!=(const_iterator const& other) const
    {
        return !(*this == other);
    }

    reference
    operator*() const
    {
        return pi_.v;
    }

    pointer
    operator->() const
    {
        return &*(*this);
    }

    const_iterator&
    operator++()
    {
        increment();
        return *this;
    }

    const_iterator
    operator++(int)
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

private:
    friend class param_list;

    const_iterator(iter_type first, iter_type last)
    {
        pi_.it = first;
        pi_.first = first;
        pi_.last = last;
        increment();
    }

    template<class = void>
    static
    std::string
    unquote(string_view sr);

    template<class = void>
    void
    increment();
};

inline
auto
param_list::
begin() const ->
    const_iterator
{
    return const_iterator{s_.begin(), s_.end()};
}

inline
auto
param_list::
end() const ->
    const_iterator
{
    return const_iterator{s_.end(), s_.end()};
}

inline
auto
param_list::
cbegin() const ->
    const_iterator
{
    return const_iterator{s_.begin(), s_.end()};
}

inline
auto
param_list::
cend() const ->
    const_iterator
{
    return const_iterator{s_.end(), s_.end()};
}

template<class>
std::string
param_list::const_iterator::
unquote(string_view sr)
{
    std::string s;
    s.reserve(sr.size());
    auto it = sr.begin() + 1;
    auto end = sr.end() - 1;
    while(it != end)
    {
        if(*it == '\\')
            ++it;
        s.push_back(*it);
        ++it;
    }
    return s;
}

template<class>
void
param_list::const_iterator::
increment()
{
    s_.clear();
    pi_.increment();
    if(pi_.empty())
    {
        pi_.it = pi_.last;
        pi_.first = pi_.last;
    }
    else if(! pi_.v.second.empty() &&
        pi_.v.second.front() == '"')
    {
        s_ = unquote(pi_.v.second);
        pi_.v.second = string_view{
            s_.data(), s_.size()};
    }
}

//------------------------------------------------------------------------------

class ext_list::const_iterator
{
    ext_list::value_type v_;
    iter_type it_;
    iter_type first_;
    iter_type last_;

public:
    using value_type = ext_list::value_type;
    using pointer = value_type const*;
    using reference = value_type const&;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    const_iterator() = default;

    bool
    operator==(const_iterator const& other) const
    {
        return
            other.it_ == it_ &&
            other.first_ == first_ &&
            other.last_ == last_;
    }

    bool
    operator!=(const_iterator const& other) const
    {
        return !(*this == other);
    }

    reference
    operator*() const
    {
        return v_;
    }

    pointer
    operator->() const
    {
        return &*(*this);
    }

    const_iterator&
    operator++()
    {
        increment();
        return *this;
    }

    const_iterator
    operator++(int)
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

private:
    friend class ext_list;

    const_iterator(iter_type begin, iter_type end)
    {
        it_ = begin;
        first_ = begin;
        last_ = end;
        increment();
    }

    template<class = void>
    void
    increment();
};

inline
auto
ext_list::
begin() const ->
    const_iterator
{
    return const_iterator{s_.begin(), s_.end()};
}

inline
auto
ext_list::
end() const ->
    const_iterator
{
    return const_iterator{s_.end(), s_.end()};
}

inline
auto
ext_list::
cbegin() const ->
    const_iterator
{
    return const_iterator{s_.begin(), s_.end()};
}

inline
auto
ext_list::
cend() const ->
    const_iterator
{
    return const_iterator{s_.end(), s_.end()};
}

template<class T>
auto
ext_list::
find(T const& s) ->
    const_iterator
{
    return std::find_if(begin(), end(),
        [&s](value_type const& v)
        {
            return iequals(s, v.first);
        });
}

template<class T>
bool
ext_list::
exists(T const& s)
{
    return find(s) != end();
}

template<class>
void
ext_list::const_iterator::
increment()
{
    /*
        ext-list    = *( "," OWS ) ext *( OWS "," [ OWS ext ] )
        ext         = token param-list
        param-list  = *( OWS ";" OWS param )
        param       = token OWS "=" OWS ( token / quoted-string )

        chunked;a=b;i=j,gzip;windowBits=12
        x,y
        ,,,,,chameleon
    */
    auto const err =
        [&]
        {
            it_ = last_;
            first_ = last_;
        };
    auto need_comma = it_ != first_;
    v_.first = {};
    first_ = it_;
    for(;;)
    {
        detail::skip_ows(it_, last_);
        if(it_ == last_)
            return err();
        auto const c = *it_;
        if(detail::is_token_char(c))
        {
            if(need_comma)
                return err();
            auto const p0 = it_;
            for(;;)
            {
                ++it_;
                if(it_ == last_)
                    break;
                if(! detail::is_token_char(*it_))
                    break;
            }
            v_.first = string_view{&*p0,
                static_cast<std::size_t>(it_ - p0)};
            detail::param_iter pi;
            pi.it = it_;
            pi.first = it_;
            pi.last = last_;
            for(;;)
            {
                pi.increment();
                if(pi.empty())
                    break;
            }
            v_.second = param_list{string_view{&*it_,
                static_cast<std::size_t>(pi.it - it_)}};
            it_ = pi.it;
            return;
        }
        if(c != ',')
            return err();
        need_comma = false;
        ++it_;
    }
}

//------------------------------------------------------------------------------

class token_list::const_iterator
{
    token_list::value_type v_;
    iter_type it_;
    iter_type first_;
    iter_type last_;

public:
    using value_type = token_list::value_type;
    using pointer = value_type const*;
    using reference = value_type const&;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    const_iterator() = default;

    bool
    operator==(const_iterator const& other) const
    {
        return
            other.it_ == it_ &&
            other.first_ == first_ &&
            other.last_ == last_;
    }

    bool
    operator!=(const_iterator const& other) const
    {
        return !(*this == other);
    }

    reference
    operator*() const
    {
        return v_;
    }

    pointer
    operator->() const
    {
        return &*(*this);
    }

    const_iterator&
    operator++()
    {
        increment();
        return *this;
    }

    const_iterator
    operator++(int)
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

private:
    friend class token_list;

    const_iterator(iter_type begin, iter_type end)
    {
        it_ = begin;
        first_ = begin;
        last_ = end;
        increment();
    }

    template<class = void>
    void
    increment();
};

inline
auto
token_list::
begin() const ->
    const_iterator
{
    return const_iterator{s_.begin(), s_.end()};
}

inline
auto
token_list::
end() const ->
    const_iterator
{
    return const_iterator{s_.end(), s_.end()};
}

inline
auto
token_list::
cbegin() const ->
    const_iterator
{
    return const_iterator{s_.begin(), s_.end()};
}

inline
auto
token_list::
cend() const ->
    const_iterator
{
    return const_iterator{s_.end(), s_.end()};
}

template<class>
void
token_list::const_iterator::
increment()
{
    /*
        token-list  = *( "," OWS ) token *( OWS "," [ OWS ext ] )
    */
    auto const err =
        [&]
        {
            it_ = last_;
            first_ = last_;
        };
    auto need_comma = it_ != first_;
    v_ = {};
    first_ = it_;
    for(;;)
    {
        detail::skip_ows(it_, last_);
        if(it_ == last_)
            return err();
        auto const c = *it_;
        if(detail::is_token_char(c))
        {
            if(need_comma)
                return err();
            auto const p0 = it_;
            for(;;)
            {
                ++it_;
                if(it_ == last_)
                    break;
                if(! detail::is_token_char(*it_))
                    break;
            }
            v_ = string_view{&*p0,
                static_cast<std::size_t>(it_ - p0)};
            return;
        }
        if(c != ',')
            return err();
        need_comma = false;
        ++it_;
    }
}

template<class T>
bool
token_list::
exists(T const& s)
{
    return std::find_if(begin(), end(),
        [&s](value_type const& v)
        {
            return iequals(s, v);
        }
    ) != end();
}

template<class Policy>
bool
validate_list(detail::basic_parsed_list<
    Policy> const& list)
{
    auto const last = list.end();
    auto it = list.begin();
    if(it.error())
        return false;
    while(it != last)
    {
        ++it;
        if(it.error())
            return false;
        if(it == last)
            break;
    }
    return true;
}

} // http
} // beast
} // boost

#endif

