//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_FIELDS_IPP
#define BOOST_BEAST_HTTP_IMPL_FIELDS_IPP

#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/static_string.hpp>
#include <boost/beast/core/detail/buffers_ref.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/rfc7230.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/chunk_encode.hpp>
#include <boost/core/exchange.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <string>

#if defined(BOOST_LIBSTDCXX_VERSION) && BOOST_LIBSTDCXX_VERSION < 60000
    // Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56437
#ifndef BOOST_BEAST_HTTP_NO_FIELDS_BASIC_STRING_ALLOCATOR
#define BOOST_BEAST_HTTP_NO_FIELDS_BASIC_STRING_ALLOCATOR
#endif
#endif

namespace boost {
namespace beast {
namespace http {

template<class Allocator>
class basic_fields<Allocator>::writer
{
public:
    using iter_type = typename list_t::const_iterator;

    struct field_iterator
    {
        iter_type it_;

        using value_type = boost::asio::const_buffer;
        using pointer = value_type const*;
        using reference = value_type const;
        using difference_type = std::ptrdiff_t;
        using iterator_category =
            std::bidirectional_iterator_tag;

        field_iterator() = default;
        field_iterator(field_iterator&& other) = default;
        field_iterator(field_iterator const& other) = default;
        field_iterator& operator=(field_iterator&& other) = default;
        field_iterator& operator=(field_iterator const& other) = default;

        explicit
        field_iterator(iter_type it)
            : it_(it)
        {
        }

        bool
        operator==(field_iterator const& other) const
        {
            return it_ == other.it_;
        }

        bool
        operator!=(field_iterator const& other) const
        {
            return !(*this == other);
        }

        reference
        operator*() const
        {
            return it_->buffer();
        }

        field_iterator&
        operator++()
        {
            ++it_;
            return *this;
        }

        field_iterator
        operator++(int)
        {
            auto temp = *this;
            ++(*this);
            return temp;
        }

        field_iterator&
        operator--()
        {
            --it_;
            return *this;
        }

        field_iterator
        operator--(int)
        {
            auto temp = *this;
            --(*this);
            return temp;
        }
    };

    class field_range
    {
        field_iterator first_;
        field_iterator last_;

    public:
        using const_iterator =
            field_iterator;

        using value_type =
            typename const_iterator::value_type;

        field_range(iter_type first, iter_type last)
            : first_(first)
            , last_(last)
        {
        }

        const_iterator
        begin() const
        {
            return first_;
        }

        const_iterator
        end() const
        {
            return last_;
        }
    };

    using view_type = buffers_cat_view<
        boost::asio::const_buffer,
        boost::asio::const_buffer,
        boost::asio::const_buffer,
        field_range,
        chunk_crlf>;

    basic_fields const& f_;
    boost::optional<view_type> view_;
    char buf_[13];

public:
    using const_buffers_type =
        beast::detail::buffers_ref<view_type>;

    writer(basic_fields const& f,
        unsigned version, verb v);

    writer(basic_fields const& f,
        unsigned version, unsigned code);

    writer(basic_fields const& f);

    const_buffers_type
    get() const
    {
        return const_buffers_type(*view_);
    }
};

template<class Allocator>
basic_fields<Allocator>::writer::
writer(basic_fields const& f)
    : f_(f)
{
    view_.emplace(
        boost::asio::const_buffer{nullptr, 0},
        boost::asio::const_buffer{nullptr, 0},
        boost::asio::const_buffer{nullptr, 0},
        field_range(f_.list_.begin(), f_.list_.end()),
        chunk_crlf());
}

template<class Allocator>
basic_fields<Allocator>::writer::
writer(basic_fields const& f,
        unsigned version, verb v)
    : f_(f)
{
/*
    request
        "<method>"
        " <target>"
        " HTTP/X.Y\r\n" (11 chars)
*/
    string_view sv;
    if(v == verb::unknown)
        sv = f_.get_method_impl();
    else
        sv = to_string(v);

    // target_or_reason_ has a leading SP

    buf_[0] = ' ';
    buf_[1] = 'H';
    buf_[2] = 'T';
    buf_[3] = 'T';
    buf_[4] = 'P';
    buf_[5] = '/';
    buf_[6] = '0' + static_cast<char>(version / 10);
    buf_[7] = '.';
    buf_[8] = '0' + static_cast<char>(version % 10);
    buf_[9] = '\r';
    buf_[10]= '\n';

    view_.emplace(
        boost::asio::const_buffer{sv.data(), sv.size()},
        boost::asio::const_buffer{
            f_.target_or_reason_.data(),
            f_.target_or_reason_.size()},
        boost::asio::const_buffer{buf_, 11},
        field_range(f_.list_.begin(), f_.list_.end()),
        chunk_crlf());
}

template<class Allocator>
basic_fields<Allocator>::writer::
writer(basic_fields const& f,
        unsigned version, unsigned code)
    : f_(f)
{
/*
    response
        "HTTP/X.Y ### " (13 chars)
        "<reason>"
        "\r\n"
*/
    buf_[0] = 'H';
    buf_[1] = 'T';
    buf_[2] = 'T';
    buf_[3] = 'P';
    buf_[4] = '/';
    buf_[5] = '0' + static_cast<char>(version / 10);
    buf_[6] = '.';
    buf_[7] = '0' + static_cast<char>(version % 10);
    buf_[8] = ' ';
    buf_[9] = '0' + static_cast<char>(code / 100);
    buf_[10]= '0' + static_cast<char>((code / 10) % 10);
    buf_[11]= '0' + static_cast<char>(code % 10);
    buf_[12]= ' ';

    string_view sv;
    if(! f_.target_or_reason_.empty())
        sv = f_.target_or_reason_;
    else
        sv = obsolete_reason(static_cast<status>(code));

    view_.emplace(
        boost::asio::const_buffer{buf_, 13},
        boost::asio::const_buffer{sv.data(), sv.size()},
        boost::asio::const_buffer{"\r\n", 2},
        field_range(f_.list_.begin(), f_.list_.end()),
        chunk_crlf{});
}

//------------------------------------------------------------------------------

template<class Allocator>
basic_fields<Allocator>::
value_type::
value_type(
    field name,
    string_view sname,
    string_view value)
    : off_(static_cast<off_t>(sname.size() + 2))
    , len_(static_cast<off_t>(value.size()))
    , f_(name)
{
    //BOOST_ASSERT(name == field::unknown ||
    //    iequals(sname, to_string(name)));
    char* p = reinterpret_cast<char*>(this + 1);
    p[off_-2] = ':';
    p[off_-1] = ' ';
    p[off_ + len_] = '\r';
    p[off_ + len_ + 1] = '\n';
    sname.copy(p, sname.size());
    value.copy(p + off_, value.size());
}

template<class Allocator>
inline
field
basic_fields<Allocator>::
value_type::
name() const
{
    return f_;
}

template<class Allocator>
inline
string_view const
basic_fields<Allocator>::
value_type::
name_string() const
{
    return {reinterpret_cast<
        char const*>(this + 1),
            static_cast<std::size_t>(off_ - 2)};
}

template<class Allocator>
inline
string_view const
basic_fields<Allocator>::
value_type::
value() const
{
    return {reinterpret_cast<
        char const*>(this + 1) + off_,
            static_cast<std::size_t>(len_)};
}

template<class Allocator>
inline
boost::asio::const_buffer
basic_fields<Allocator>::
value_type::
buffer() const
{
    return boost::asio::const_buffer{
        reinterpret_cast<char const*>(this + 1),
        static_cast<std::size_t>(off_) + len_ + 2};
}

//------------------------------------------------------------------------------

template<class Allocator>
basic_fields<Allocator>::
~basic_fields()
{
    delete_list();
    realloc_string(method_, {});
    realloc_string(
        target_or_reason_, {});
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(Allocator const& alloc) noexcept
    : beast::detail::empty_base_optimization<Allocator>(alloc)
{
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields&& other) noexcept
    : beast::detail::empty_base_optimization<Allocator>(
        std::move(other.member()))
    , set_(std::move(other.set_))
    , list_(std::move(other.list_))
    , method_(boost::exchange(other.method_, {}))
    , target_or_reason_(boost::exchange(other.target_or_reason_, {}))
{
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields&& other, Allocator const& alloc)
    : beast::detail::empty_base_optimization<Allocator>(alloc)
{
    if(this->member() != other.member())
    {
        copy_all(other);
        other.clear_all();
    }
    else
    {
        set_ = std::move(other.set_);
        list_ = std::move(other.list_);
        method_ = other.method_;
        target_or_reason_ = other.target_or_reason_;
    }
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields const& other)
    : beast::detail::empty_base_optimization<Allocator>(alloc_traits::
        select_on_container_copy_construction(other.member()))
{
    copy_all(other);
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields const& other,
        Allocator const& alloc)
    : beast::detail::empty_base_optimization<Allocator>(alloc)
{
    copy_all(other);
}

template<class Allocator>
template<class OtherAlloc>
basic_fields<Allocator>::
basic_fields(basic_fields<OtherAlloc> const& other)
{
    copy_all(other);
}

template<class Allocator>
template<class OtherAlloc>
basic_fields<Allocator>::
basic_fields(basic_fields<OtherAlloc> const& other,
        Allocator const& alloc)
    : beast::detail::empty_base_optimization<Allocator>(alloc)
{
    copy_all(other);
}

template<class Allocator>
auto
basic_fields<Allocator>::
operator=(basic_fields&& other) noexcept(
    alloc_traits::propagate_on_container_move_assignment::value)
      -> basic_fields&
{
    static_assert(is_nothrow_move_assignable<Allocator>::value,
        "Allocator must be noexcept assignable.");
    if(this == &other)
        return *this;
    move_assign(other, std::integral_constant<bool,
        alloc_traits:: propagate_on_container_move_assignment::value>{});
    return *this;
}

template<class Allocator>
auto
basic_fields<Allocator>::
operator=(basic_fields const& other) ->
    basic_fields&
{
    copy_assign(other, std::integral_constant<bool,
        alloc_traits::propagate_on_container_copy_assignment::value>{});
    return *this;
}

template<class Allocator>
template<class OtherAlloc>
auto
basic_fields<Allocator>::
operator=(basic_fields<OtherAlloc> const& other) ->
    basic_fields&
{
    clear_all();
    copy_all(other);
    return *this;
}

//------------------------------------------------------------------------------
//
// Element access
//
//------------------------------------------------------------------------------

template<class Allocator>
string_view const
basic_fields<Allocator>::
at(field name) const
{
    BOOST_ASSERT(name != field::unknown);
    auto const it = find(name);
    if(it == end())
        BOOST_THROW_EXCEPTION(std::out_of_range{
            "field not found"});
    return it->value();
}

template<class Allocator>
string_view const
basic_fields<Allocator>::
at(string_view name) const
{
    auto const it = find(name);
    if(it == end())
        BOOST_THROW_EXCEPTION(std::out_of_range{
            "field not found"});
    return it->value();
}

template<class Allocator>
string_view const
basic_fields<Allocator>::
operator[](field name) const
{
    BOOST_ASSERT(name != field::unknown);
    auto const it = find(name);
    if(it == end())
        return {};
    return it->value();
}

template<class Allocator>
string_view const
basic_fields<Allocator>::
operator[](string_view name) const
{
    auto const it = find(name);
    if(it == end())
        return {};
    return it->value();
}

//------------------------------------------------------------------------------
//
// Modifiers
//
//------------------------------------------------------------------------------

template<class Allocator>
void
basic_fields<Allocator>::
clear()
{
    delete_list();
    set_.clear();
    list_.clear();
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
insert(field name, string_param const& value)
{
    BOOST_ASSERT(name != field::unknown);
    insert(name, to_string(name), value);
}

template<class Allocator>
void
basic_fields<Allocator>::
insert(string_view sname, string_param const& value)
{
    auto const name =
        string_to_field(sname);
    insert(name, sname, value);
}

template<class Allocator>
void
basic_fields<Allocator>::
insert(field name,
    string_view sname, string_param const& value)
{
    auto& e = new_element(name, sname,
        static_cast<string_view>(value));
    auto const before =
        set_.upper_bound(sname, key_compare{});
    if(before == set_.begin())
    {
        BOOST_ASSERT(count(sname) == 0);
        set_.insert_before(before, e);
        list_.push_back(e);
        return;
    }
    auto const last = std::prev(before);
    // VFALCO is it worth comparing `field name` first?
    if(! iequals(sname, last->name_string()))
    {
        BOOST_ASSERT(count(sname) == 0);
        set_.insert_before(before, e);
        list_.push_back(e);
        return;
    }
    // keep duplicate fields together in the list
    set_.insert_before(before, e);
    list_.insert(++list_.iterator_to(*last), e);
}

template<class Allocator>
void
basic_fields<Allocator>::
set(field name, string_param const& value)
{
    BOOST_ASSERT(name != field::unknown);
    set_element(new_element(name, to_string(name),
        static_cast<string_view>(value)));
}

template<class Allocator>
void
basic_fields<Allocator>::
set(string_view sname, string_param const& value)
{
    set_element(new_element(
        string_to_field(sname), sname,
            static_cast<string_view>(value)));
}

template<class Allocator>
auto
basic_fields<Allocator>::
erase(const_iterator pos) ->
    const_iterator
{
    auto next = pos;
    auto& e = *next++;
    set_.erase(e);
    list_.erase(pos);
    delete_element(const_cast<value_type&>(e));
    return next;
}

template<class Allocator>
std::size_t
basic_fields<Allocator>::
erase(field name)
{
    BOOST_ASSERT(name != field::unknown);
    return erase(to_string(name));
}

template<class Allocator>
std::size_t
basic_fields<Allocator>::
erase(string_view name)
{
    std::size_t n =0;
    set_.erase_and_dispose(name, key_compare{},
        [&](value_type* e)
        {
            ++n;
            list_.erase(list_.iterator_to(*e));
            delete_element(*e);
        });
    return n;
}

template<class Allocator>
void
basic_fields<Allocator>::
swap(basic_fields<Allocator>& other)
{
    swap(other, std::integral_constant<bool,
        alloc_traits::propagate_on_container_swap::value>{});
}

template<class Allocator>
void
swap(
    basic_fields<Allocator>& lhs,
    basic_fields<Allocator>& rhs)
{
    lhs.swap(rhs);
}

//------------------------------------------------------------------------------
//
// Lookup
//
//------------------------------------------------------------------------------

template<class Allocator>
inline
std::size_t
basic_fields<Allocator>::
count(field name) const
{
    BOOST_ASSERT(name != field::unknown);
    return count(to_string(name));
}

template<class Allocator>
std::size_t
basic_fields<Allocator>::
count(string_view name) const
{
    return set_.count(name, key_compare{});
}

template<class Allocator>
inline
auto
basic_fields<Allocator>::
find(field name) const ->
    const_iterator
{
    BOOST_ASSERT(name != field::unknown);
    return find(to_string(name));
}

template<class Allocator>
auto
basic_fields<Allocator>::
find(string_view name) const ->
    const_iterator
{
    auto const it = set_.find(
        name, key_compare{});
    if(it == set_.end())
        return list_.end();
    return list_.iterator_to(*it);
}

template<class Allocator>
inline
auto
basic_fields<Allocator>::
equal_range(field name) const ->
    std::pair<const_iterator, const_iterator>
{
    BOOST_ASSERT(name != field::unknown);
    return equal_range(to_string(name));
}

template<class Allocator>
auto
basic_fields<Allocator>::
equal_range(string_view name) const ->
    std::pair<const_iterator, const_iterator>
{
    auto result =
        set_.equal_range(name, key_compare{});
    if(result.first == result.second)
        return {list_.end(), list_.end()};
    return {
        list_.iterator_to(*result.first),
        ++list_.iterator_to(*(--result.second))};
}

//------------------------------------------------------------------------------

namespace detail {

// Filter a token list
//
template<class String, class Pred>
void
filter_token_list(
    String& s,
    string_view value,
    Pred&& pred)
{
    token_list te{value};
    auto it = te.begin();
    auto last = te.end();
    if(it == last)
        return;
    while(pred(*it))
        if(++it == last)
            return;
    s.append(it->data(), it->size());
    while(++it != last)
    {
        if(! pred(*it))
        {
            s.append(", ");
            s.append(it->data(), it->size());
        }
    }
}

// Filter the last item in a token list
template<class String, class Pred>
void
filter_token_list_last(
    String& s,
    string_view value,
    Pred&& pred)
{
    token_list te{value};
    if(te.begin() != te.end())
    {
        auto it = te.begin();
        auto next = std::next(it);
        if(next == te.end())
        {
            if(! pred(*it))
                s.append(it->data(), it->size());
            return;
        }
        s.append(it->data(), it->size());
        for(;;)
        {
            it = next;
            next = std::next(it);
            if(next == te.end())
            {
                if(! pred(*it))
                {
                    s.append(", ");
                    s.append(it->data(), it->size());
                }
                return;
            }
            s.append(", ");
            s.append(it->data(), it->size());
        }
    }
}

template<class String>
void
keep_alive_impl(
    String& s, string_view value,
    unsigned version, bool keep_alive)
{
    if(version < 11)
    {
        if(keep_alive)
        {
            // remove close
            filter_token_list(s, value,
                [](string_view s)
                {
                    return iequals(s, "close");
                });
            // add keep-alive
            if(s.empty())
                s.append("keep-alive");
            else if(! token_list{value}.exists("keep-alive"))
                s.append(", keep-alive");
        }
        else
        {
            // remove close and keep-alive
            filter_token_list(s, value,
                [](string_view s)
                {
                    return
                        iequals(s, "close") ||
                        iequals(s, "keep-alive");
                });
        }
    }
    else
    {
        if(keep_alive)
        {
            // remove close and keep-alive
            filter_token_list(s, value,
                [](string_view s)
                {
                    return
                        iequals(s, "close") ||
                        iequals(s, "keep-alive");
                });
        }
        else
        {
            // remove keep-alive
            filter_token_list(s, value,
                [](string_view s)
                {
                    return iequals(s, "keep-alive");
                });
            // add close
            if(s.empty())
                s.append("close");
            else if(! token_list{value}.exists("close"))
                s.append(", close");
        }
    }
}

} // detail

//------------------------------------------------------------------------------

// Fields

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
get_method_impl() const
{
    return method_;
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
get_target_impl() const
{
    if(target_or_reason_.empty())
        return target_or_reason_;
    return {
        target_or_reason_.data() + 1,
        target_or_reason_.size() - 1};
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
get_reason_impl() const
{
    return target_or_reason_;
}

template<class Allocator>
bool
basic_fields<Allocator>::
get_chunked_impl() const
{
    auto const te = token_list{
        (*this)[field::transfer_encoding]};
    for(auto it = te.begin(); it != te.end();)
    {
        auto const next = std::next(it);
        if(next == te.end())
            return iequals(*it, "chunked");
        it = next;
    }
    return false;
}

template<class Allocator>
bool
basic_fields<Allocator>::
get_keep_alive_impl(unsigned version) const
{
    auto const it = find(field::connection);
    if(version < 11)
    {
        if(it == end())
            return false;
        return token_list{
            it->value()}.exists("keep-alive");
    }
    if(it == end())
        return true;
    return ! token_list{
        it->value()}.exists("close");
}

template<class Allocator>
bool
basic_fields<Allocator>::
has_content_length_impl() const
{
    return count(field::content_length) > 0;
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
set_method_impl(string_view s)
{
    realloc_string(method_, s);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
set_target_impl(string_view s)
{
    realloc_target(
        target_or_reason_, s);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
set_reason_impl(string_view s)
{
    realloc_string(
        target_or_reason_, s);
}

template<class Allocator>
void
basic_fields<Allocator>::
set_chunked_impl(bool value)
{
    auto it = find(field::transfer_encoding);
    if(value)
    {
        // append "chunked"
        if(it == end())
        {
            set(field::transfer_encoding, "chunked");
            return;
        }
        auto const te = token_list{it->value()};
        for(auto itt = te.begin();;)
        {
            auto const next = std::next(itt);
            if(next == te.end())
            {
                if(iequals(*itt, "chunked"))
                    return; // already set
                break;
            }
            itt = next;
        }
        static_string<max_static_buffer> buf;
        if(it->value().size() <= buf.size() + 9)
        {
            buf.append(it->value().data(), it->value().size());
            buf.append(", chunked", 9);
            set(field::transfer_encoding, buf);
        }
        else
        {
        #ifdef BOOST_BEAST_HTTP_NO_FIELDS_BASIC_STRING_ALLOCATOR
            // Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56437
            std::string s;
        #else
            using A =
                typename beast::detail::allocator_traits<
                    Allocator>::template rebind_alloc<char>;
            std::basic_string<
                char,
                std::char_traits<char>,
                A> s{A{this->member()}};
        #endif
            s.reserve(it->value().size() + 9);
            s.append(it->value().data(), it->value().size());
            s.append(", chunked", 9);
            set(field::transfer_encoding, s);
        }
        return;
    }
    // filter "chunked"
    if(it == end())
        return;
    try
    {
        static_string<max_static_buffer> buf;
        detail::filter_token_list_last(buf, it->value(),
            [](string_view s)
            {
                return iequals(s, "chunked");
            });
        if(! buf.empty())
            set(field::transfer_encoding, buf);
        else
            erase(field::transfer_encoding);
    }
    catch(std::length_error const&)
    {
    #ifdef BOOST_BEAST_HTTP_NO_FIELDS_BASIC_STRING_ALLOCATOR
        // Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56437
        std::string s;
    #else
        using A =
            typename beast::detail::allocator_traits<
                Allocator>::template rebind_alloc<char>;
        std::basic_string<
            char,
            std::char_traits<char>,
            A> s{A{this->member()}};
    #endif
        s.reserve(it->value().size());
        detail::filter_token_list_last(s, it->value(),
            [](string_view s)
            {
                return iequals(s, "chunked");
            });
        if(! s.empty())
            set(field::transfer_encoding, s);
        else
            erase(field::transfer_encoding);
    }
}

template<class Allocator>
void
basic_fields<Allocator>::
set_content_length_impl(
    boost::optional<std::uint64_t> const& value)
{
    if(! value)
        erase(field::content_length);
    else
        set(field::content_length, *value);
}

template<class Allocator>
void
basic_fields<Allocator>::
set_keep_alive_impl(
    unsigned version, bool keep_alive)
{
    // VFALCO What about Proxy-Connection ?
    auto const value = (*this)[field::connection];
    try
    {
        static_string<max_static_buffer> buf;
        detail::keep_alive_impl(
            buf, value, version, keep_alive);
        if(buf.empty())
            erase(field::connection);
        else
            set(field::connection, buf);
    }
    catch(std::length_error const&)
    {
    #ifdef BOOST_BEAST_HTTP_NO_FIELDS_BASIC_STRING_ALLOCATOR
        // Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56437
        std::string s;
    #else
        using A =
            typename beast::detail::allocator_traits<
                Allocator>::template rebind_alloc<char>;
        std::basic_string<
            char,
            std::char_traits<char>,
            A> s{A{this->member()}};
    #endif
        s.reserve(value.size());
        detail::keep_alive_impl(
            s, value, version, keep_alive);
        if(s.empty())
            erase(field::connection);
        else
            set(field::connection, s);
    }
}

//------------------------------------------------------------------------------

template<class Allocator>
auto
basic_fields<Allocator>::
new_element(field name,
    string_view sname, string_view value) ->
        value_type&
{
    if(sname.size() + 2 >
            (std::numeric_limits<off_t>::max)())
        BOOST_THROW_EXCEPTION(std::length_error{
            "field name too large"});
    if(value.size() + 2 >
            (std::numeric_limits<off_t>::max)())
        BOOST_THROW_EXCEPTION(std::length_error{
            "field value too large"});
    value = detail::trim(value);
    std::uint16_t const off =
        static_cast<off_t>(sname.size() + 2);
    std::uint16_t const len =
        static_cast<off_t>(value.size());
    auto a = rebind_type{this->member()};
    auto const p = alloc_traits::allocate(a,
        (sizeof(value_type) + off + len + 2 + sizeof(align_type) - 1) /
            sizeof(align_type));
    // VFALCO allocator can't call the constructor because its private
    //alloc_traits::construct(a, p, name, sname, value);
    new(p) value_type{name, sname, value};
    return *reinterpret_cast<value_type*>(p);
}

template<class Allocator>
void
basic_fields<Allocator>::
delete_element(value_type& e)
{
    auto a = rebind_type{this->member()};
    auto const n =
        (sizeof(value_type) + e.off_ + e.len_ + 2 + sizeof(align_type) - 1) /
            sizeof(align_type);
    //alloc_traits::destroy(a, &e);
    e.~value_type();
    alloc_traits::deallocate(a,
        reinterpret_cast<align_type*>(&e), n);
}

template<class Allocator>
void
basic_fields<Allocator>::
set_element(value_type& e)
{
    auto it = set_.lower_bound(
        e.name_string(), key_compare{});
    if(it == set_.end() || ! iequals(
        e.name_string(), it->name_string()))
    {
        set_.insert_before(it, e);
        list_.push_back(e);
        return;
    }
    for(;;)
    {
        auto next = it;
        ++next;
        set_.erase(it);
        list_.erase(list_.iterator_to(*it));
        delete_element(*it);
        it = next;
        if(it == set_.end() ||
            ! iequals(e.name_string(), it->name_string()))
            break;
    }
    set_.insert_before(it, e);
    list_.push_back(e);
}

template<class Allocator>
void
basic_fields<Allocator>::
realloc_string(string_view& dest, string_view s)
{
    if(dest.empty() && s.empty())
        return;
    auto a = typename beast::detail::allocator_traits<
        Allocator>::template rebind_alloc<
            char>(this->member());
    char* p = nullptr;
    if(! s.empty())
    {
        p = a.allocate(s.size());
        s.copy(p, s.size());
    }
    if(! dest.empty())
        a.deallocate(const_cast<char*>(
            dest.data()), dest.size());
    if(p)
        dest = {p, s.size()};
    else
        dest = {};
}

template<class Allocator>
void
basic_fields<Allocator>::
realloc_target(
    string_view& dest, string_view s)
{
    // The target string are stored with an
    // extra space at the beginning to help
    // the writer class.
    if(dest.empty() && s.empty())
        return;
    auto a = typename beast::detail::allocator_traits<
        Allocator>::template rebind_alloc<
            char>(this->member());
    char* p = nullptr;
    if(! s.empty())
    {
        p = a.allocate(1 + s.size());
        p[0] = ' ';
        s.copy(p + 1, s.size());
    }
    if(! dest.empty())
        a.deallocate(const_cast<char*>(
            dest.data()), dest.size());
    if(p)
        dest = {p, 1 + s.size()};
    else
        dest = {};
}

template<class Allocator>
template<class OtherAlloc>
void
basic_fields<Allocator>::
copy_all(basic_fields<OtherAlloc> const& other)
{
    for(auto const& e : other.list_)
        insert(e.name(), e.name_string(), e.value());
    realloc_string(method_, other.method_);
    realloc_string(target_or_reason_,
        other.target_or_reason_);
}

template<class Allocator>
void
basic_fields<Allocator>::
clear_all()
{
    clear();
    realloc_string(method_, {});
    realloc_string(target_or_reason_, {});
}

template<class Allocator>
void
basic_fields<Allocator>::
delete_list()
{
    for(auto it = list_.begin(); it != list_.end();)
        delete_element(*it++);
}

//------------------------------------------------------------------------------

template<class Allocator>
inline
void
basic_fields<Allocator>::
move_assign(basic_fields& other, std::true_type)
{
    clear_all();
    set_ = std::move(other.set_);
    list_ = std::move(other.list_);
    method_ = other.method_;
    target_or_reason_ = other.target_or_reason_;
    other.method_ = {};
    other.target_or_reason_ = {};
    this->member() = other.member();
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
move_assign(basic_fields& other, std::false_type)
{
    clear_all();
    if(this->member() != other.member())
    {
        copy_all(other);
        other.clear_all();
    }
    else
    {
        set_ = std::move(other.set_);
        list_ = std::move(other.list_);
        method_ = other.method_;
        target_or_reason_ = other.target_or_reason_;
        other.method_ = {};
        other.target_or_reason_ = {};
    }
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
copy_assign(basic_fields const& other, std::true_type)
{
    clear_all();
    this->member() = other.member();
    copy_all(other);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
copy_assign(basic_fields const& other, std::false_type)
{
    clear_all();
    copy_all(other);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
swap(basic_fields& other, std::true_type)
{
    using std::swap;
    swap(this->member(), other.member());
    swap(set_, other.set_);
    swap(list_, other.list_);
    swap(method_, other.method_);
    swap(target_or_reason_, other.target_or_reason_);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
swap(basic_fields& other, std::false_type)
{
    BOOST_ASSERT(this->member() == other.member());
    using std::swap;
    swap(set_, other.set_);
    swap(list_, other.list_);
    swap(method_, other.method_);
    swap(target_or_reason_, other.target_or_reason_);
}

} // http
} // beast
} // boost

#endif
