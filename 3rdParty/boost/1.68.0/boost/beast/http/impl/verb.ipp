//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_VERB_IPP
#define BOOST_BEAST_HTTP_IMPL_VERB_IPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace boost {
namespace beast {
namespace http {

namespace detail {

template<class = void>
inline
string_view
verb_to_string(verb v)
{
    switch(v)
    {
    case verb::delete_:       return "DELETE";
    case verb::get:           return "GET";
    case verb::head:          return "HEAD";
    case verb::post:          return "POST";
    case verb::put:           return "PUT";
    case verb::connect:       return "CONNECT";
    case verb::options:       return "OPTIONS";
    case verb::trace:         return "TRACE";

    case verb::copy:          return "COPY";
    case verb::lock:          return "LOCK";
    case verb::mkcol:         return "MKCOL";
    case verb::move:          return "MOVE";
    case verb::propfind:      return "PROPFIND";
    case verb::proppatch:     return "PROPPATCH";
    case verb::search:        return "SEARCH";
    case verb::unlock:        return "UNLOCK";
    case verb::bind:          return "BIND";
    case verb::rebind:        return "REBIND";
    case verb::unbind:        return "UNBIND";
    case verb::acl:           return "ACL";

    case verb::report:        return "REPORT";
    case verb::mkactivity:    return "MKACTIVITY";
    case verb::checkout:      return "CHECKOUT";
    case verb::merge:         return "MERGE";

    case verb::msearch:       return "M-SEARCH";
    case verb::notify:        return "NOTIFY";
    case verb::subscribe:     return "SUBSCRIBE";
    case verb::unsubscribe:   return "UNSUBSCRIBE";

    case verb::patch:         return "PATCH";
    case verb::purge:         return "PURGE";

    case verb::mkcalendar:    return "MKCALENDAR";

    case verb::link:          return "LINK";
    case verb::unlink:        return "UNLINK";
    
    case verb::unknown:
        return "<unknown>";
    }

    BOOST_THROW_EXCEPTION(std::invalid_argument{"unknown verb"});
}

template<class = void>
verb
string_to_verb(string_view v)
{
/*
    ACL
    BIND
    CHECKOUT
    CONNECT
    COPY
    DELETE
    GET
    HEAD
    LINK
    LOCK
    M-SEARCH
    MERGE
    MKACTIVITY
    MKCALENDAR
    MKCOL
    MOVE
    NOTIFY
    OPTIONS
    PATCH
    POST
    PROPFIND
    PROPPATCH
    PURGE
    PUT
    REBIND
    REPORT
    SEARCH
    SUBSCRIBE
    TRACE
    UNBIND
    UNLINK
    UNLOCK
    UNSUBSCRIBE
*/
    if(v.size() < 3)
        return verb::unknown;
    // s must be null terminated
    auto const eq =
        [](string_view sv, char const* s)
        {
            auto p = sv.data();
            for(;;)
            {
                if(*s != *p)
                    return false;
                ++s;
                ++p;
                if(! *s)
                    return p == sv.end();
            }
        };
    auto c = v[0];
    v.remove_prefix(1);
    switch(c)
    {
    case 'A':
        if(v == "CL")
            return verb::acl;
        break;

    case 'B':
        if(v == "IND")
            return verb::bind;
        break;

    case 'C':
        c = v[0];
        v.remove_prefix(1);
        switch(c)
        {
        case 'H':
            if(eq(v, "ECKOUT"))
                return verb::checkout;
            break;

        case 'O':
            if(eq(v, "NNECT"))
                return verb::connect;
            if(eq(v, "PY"))
                return verb::copy;
            BOOST_FALLTHROUGH;

        default:
            break;
        }
        break;

    case 'D':
        if(eq(v, "ELETE"))
            return verb::delete_;
        break;

    case 'G':
        if(eq(v, "ET"))
            return verb::get;
        break;

    case 'H':
        if(eq(v, "EAD"))
            return verb::head;
        break;

    case 'L':
        if(eq(v, "INK"))
            return verb::link;
        if(eq(v, "OCK"))
            return verb::lock;
        break;

    case 'M':
        c = v[0];
        v.remove_prefix(1);
        switch(c)
        {
        case '-':
            if(eq(v, "SEARCH"))
                return verb::msearch;
            break;

        case 'E':
            if(eq(v, "RGE"))
                return verb::merge;
            break;

        case 'K':
            if(eq(v, "ACTIVITY"))
                return verb::mkactivity;
            if(v[0] == 'C')
            {
                v.remove_prefix(1);
                if(eq(v, "ALENDAR"))
                    return verb::mkcalendar;
                if(eq(v, "OL"))
                    return verb::mkcol;
                break;
            }
            break;
        
        case 'O':
            if(eq(v, "VE"))
                return verb::move;
            BOOST_FALLTHROUGH;

        default:
            break;
        }
        break;

    case 'N':
        if(eq(v, "OTIFY"))
            return verb::notify;
        break;

    case 'O':
        if(eq(v, "PTIONS"))
            return verb::options;
        break;

    case 'P':
        c = v[0];
        v.remove_prefix(1);
        switch(c)
        {
        case 'A':
            if(eq(v, "TCH"))
                return verb::patch;
            break;

        case 'O':
            if(eq(v, "ST"))
                return verb::post;
            break;

        case 'R':
            if(eq(v, "OPFIND"))
                return verb::propfind;
            if(eq(v, "OPPATCH"))
                return verb::proppatch;
            break;

        case 'U':
            if(eq(v, "RGE"))
                return verb::purge;
            if(eq(v, "T"))
                return verb::put;
            BOOST_FALLTHROUGH;

        default:
            break;
        }
        break;

    case 'R':
        if(v[0] != 'E')
            break;
        v.remove_prefix(1);
        if(eq(v, "BIND"))
            return verb::rebind;
        if(eq(v, "PORT"))
            return verb::report;
        break;

    case 'S':
        if(eq(v, "EARCH"))
            return verb::search;
        if(eq(v, "UBSCRIBE"))
            return verb::subscribe;
        break;

    case 'T':
        if(eq(v, "RACE"))
            return verb::trace;
        break;

    case 'U':
        if(v[0] != 'N')
            break;
        v.remove_prefix(1);
        if(eq(v, "BIND"))
            return verb::unbind;
        if(eq(v, "LINK"))
            return verb::unlink;
        if(eq(v, "LOCK"))
            return verb::unlock;
        if(eq(v, "SUBSCRIBE"))
            return verb::unsubscribe;
        break;

    default:
        break;
    }

    return verb::unknown;
}

} // detail

inline
string_view
to_string(verb v)
{
    return detail::verb_to_string(v);
}

inline
verb
string_to_verb(string_view s)
{
    return detail::string_to_verb(s);
}

} // http
} // beast
} // boost

#endif
