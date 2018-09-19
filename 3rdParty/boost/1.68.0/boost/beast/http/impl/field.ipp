//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_FIELD_IPP
#define BOOST_BEAST_HTTP_IMPL_FIELD_IPP

#include <boost/beast/core/string.hpp>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>
#include <boost/assert.hpp>

namespace boost {
namespace beast {
namespace http {

namespace detail {

struct field_table
{
    using array_type =
        std::array<string_view, 353>;

    struct hash
    {
        std::size_t
        operator()(string_view s) const
        {
            auto const n = s.size();
            return
                beast::detail::ascii_tolower(s[0]) *
                beast::detail::ascii_tolower(s[n/2]) ^
                beast::detail::ascii_tolower(s[n-1]);   // hist[] = 331, 10, max_load_factor = 0.15f
        }
    };

    struct iequal
    {
        // assumes inputs have equal length
        bool
        operator()(
            string_view lhs,
            string_view rhs) const
        {
            auto p1 = lhs.data();
            auto p2 = rhs.data();
            auto pend = lhs.end();
            char a, b;
            while(p1 < pend)
            {
                a = *p1++;
                b = *p2++;
                if(a != b)
                    goto slow;
            }
            return true;

            while(p1 < pend)
            {
            slow:
                if( beast::detail::ascii_tolower(a) !=
                    beast::detail::ascii_tolower(b))
                    return false;
                a = *p1++;
                b = *p2++;
            }
            return true;
        }
    };

    using map_type = std::unordered_map<
        string_view, field, hash, iequal>;

    array_type by_name_;
    std::vector<map_type> by_size_;
/*
    From:
    
    https://www.iana.org/assignments/message-headers/message-headers.xhtml
*/
    field_table()
        : by_name_({{
            "<unknown-field>",
            "A-IM",
            "Accept",
            "Accept-Additions",
            "Accept-Charset",
            "Accept-Datetime",
            "Accept-Encoding",
            "Accept-Features",
            "Accept-Language",
            "Accept-Patch",
            "Accept-Post",
            "Accept-Ranges",
            "Access-Control",
            "Access-Control-Allow-Credentials",
            "Access-Control-Allow-Headers",
            "Access-Control-Allow-Methods",
            "Access-Control-Allow-Origin",
            "Access-Control-Expose-Headers",
            "Access-Control-Max-Age",
            "Access-Control-Request-Headers",
            "Access-Control-Request-Method",
            "Age",
            "Allow",
            "ALPN",
            "Also-Control",
            "Alt-Svc",
            "Alt-Used",
            "Alternate-Recipient",
            "Alternates",
            "Apparently-To",
            "Apply-To-Redirect-Ref",
            "Approved",
            "Archive",
            "Archived-At",
            "Article-Names",
            "Article-Updates",
            "Authentication-Control",
            "Authentication-Info",
            "Authentication-Results",
            "Authorization",
            "Auto-Submitted",
            "Autoforwarded",
            "Autosubmitted",
            "Base",
            "Bcc",
            "Body",
            "C-Ext",
            "C-Man",
            "C-Opt",
            "C-PEP",
            "C-PEP-Info",
            "Cache-Control",
            "CalDAV-Timezones",
            "Cancel-Key",
            "Cancel-Lock",
            "Cc",
            "Close",
            "Comments",
            "Compliance",
            "Connection",
            "Content-Alternative",
            "Content-Base",
            "Content-Description",
            "Content-Disposition",
            "Content-Duration",
            "Content-Encoding",
            "Content-features",
            "Content-ID",
            "Content-Identifier",
            "Content-Language",
            "Content-Length",
            "Content-Location",
            "Content-MD5",
            "Content-Range",
            "Content-Return",
            "Content-Script-Type",
            "Content-Style-Type",
            "Content-Transfer-Encoding",
            "Content-Type",
            "Content-Version",
            "Control",
            "Conversion",
            "Conversion-With-Loss",
            "Cookie",
            "Cookie2",
            "Cost",
            "DASL",
            "Date",
            "Date-Received",
            "DAV",
            "Default-Style",
            "Deferred-Delivery",
            "Delivery-Date",
            "Delta-Base",
            "Depth",
            "Derived-From",
            "Destination",
            "Differential-ID",
            "Digest",
            "Discarded-X400-IPMS-Extensions",
            "Discarded-X400-MTS-Extensions",
            "Disclose-Recipients",
            "Disposition-Notification-Options",
            "Disposition-Notification-To",
            "Distribution",
            "DKIM-Signature",
            "DL-Expansion-History",
            "Downgraded-Bcc",
            "Downgraded-Cc",
            "Downgraded-Disposition-Notification-To",
            "Downgraded-Final-Recipient",
            "Downgraded-From",
            "Downgraded-In-Reply-To",
            "Downgraded-Mail-From",
            "Downgraded-Message-Id",
            "Downgraded-Original-Recipient",
            "Downgraded-Rcpt-To",
            "Downgraded-References",
            "Downgraded-Reply-To",
            "Downgraded-Resent-Bcc",
            "Downgraded-Resent-Cc",
            "Downgraded-Resent-From",
            "Downgraded-Resent-Reply-To",
            "Downgraded-Resent-Sender",
            "Downgraded-Resent-To",
            "Downgraded-Return-Path",
            "Downgraded-Sender",
            "Downgraded-To",
            "EDIINT-Features",
            "Eesst-Version",
            "Encoding",
            "Encrypted",
            "Errors-To",
            "ETag",
            "Expect",
            "Expires",
            "Expiry-Date",
            "Ext",
            "Followup-To",
            "Forwarded",
            "From",
            "Generate-Delivery-Report",
            "GetProfile",
            "Hobareg",
            "Host",
            "HTTP2-Settings",
            "If",
            "If-Match",
            "If-Modified-Since",
            "If-None-Match",
            "If-Range",
            "If-Schedule-Tag-Match",
            "If-Unmodified-Since",
            "IM",
            "Importance",
            "In-Reply-To",
            "Incomplete-Copy",
            "Injection-Date",
            "Injection-Info",
            "Jabber-ID",
            "Keep-Alive",
            "Keywords",
            "Label",
            "Language",
            "Last-Modified",
            "Latest-Delivery-Time",
            "Lines",
            "Link",
            "List-Archive",
            "List-Help",
            "List-ID",
            "List-Owner",
            "List-Post",
            "List-Subscribe",
            "List-Unsubscribe",
            "List-Unsubscribe-Post",
            "Location",
            "Lock-Token",
            "Man",
            "Max-Forwards",
            "Memento-Datetime",
            "Message-Context",
            "Message-ID",
            "Message-Type",
            "Meter",
            "Method-Check",
            "Method-Check-Expires",
            "MIME-Version",
            "MMHS-Acp127-Message-Identifier",
            "MMHS-Authorizing-Users",
            "MMHS-Codress-Message-Indicator",
            "MMHS-Copy-Precedence",
            "MMHS-Exempted-Address",
            "MMHS-Extended-Authorisation-Info",
            "MMHS-Handling-Instructions",
            "MMHS-Message-Instructions",
            "MMHS-Message-Type",
            "MMHS-Originator-PLAD",
            "MMHS-Originator-Reference",
            "MMHS-Other-Recipients-Indicator-CC",
            "MMHS-Other-Recipients-Indicator-To",
            "MMHS-Primary-Precedence",
            "MMHS-Subject-Indicator-Codes",
            "MT-Priority",
            "Negotiate",
            "Newsgroups",
            "NNTP-Posting-Date",
            "NNTP-Posting-Host",
            "Non-Compliance",
            "Obsoletes",
            "Opt",
            "Optional",
            "Optional-WWW-Authenticate",
            "Ordering-Type",
            "Organization",
            "Origin",
            "Original-Encoded-Information-Types",
            "Original-From",
            "Original-Message-ID",
            "Original-Recipient",
            "Original-Sender",
            "Original-Subject",
            "Originator-Return-Address",
            "Overwrite",
            "P3P",
            "Path",
            "PEP",
            "Pep-Info",
            "PICS-Label",
            "Position",
            "Posting-Version",
            "Pragma",
            "Prefer",
            "Preference-Applied",
            "Prevent-NonDelivery-Report",
            "Priority",
            "Privicon",
            "ProfileObject",
            "Protocol",
            "Protocol-Info",
            "Protocol-Query",
            "Protocol-Request",
            "Proxy-Authenticate",
            "Proxy-Authentication-Info",
            "Proxy-Authorization",
            "Proxy-Connection",
            "Proxy-Features",
            "Proxy-Instruction",
            "Public",
            "Public-Key-Pins",
            "Public-Key-Pins-Report-Only",
            "Range",
            "Received",
            "Received-SPF",
            "Redirect-Ref",
            "References",
            "Referer",
            "Referer-Root",
            "Relay-Version",
            "Reply-By",
            "Reply-To",
            "Require-Recipient-Valid-Since",
            "Resent-Bcc",
            "Resent-Cc",
            "Resent-Date",
            "Resent-From",
            "Resent-Message-ID",
            "Resent-Reply-To",
            "Resent-Sender",
            "Resent-To",
            "Resolution-Hint",
            "Resolver-Location",
            "Retry-After",
            "Return-Path",
            "Safe",
            "Schedule-Reply",
            "Schedule-Tag",
            "Sec-WebSocket-Accept",
            "Sec-WebSocket-Extensions",
            "Sec-WebSocket-Key",
            "Sec-WebSocket-Protocol",
            "Sec-WebSocket-Version",
            "Security-Scheme",
            "See-Also",
            "Sender",
            "Sensitivity",
            "Server",
            "Set-Cookie",
            "Set-Cookie2",
            "SetProfile",
            "SIO-Label",
            "SIO-Label-History",
            "SLUG",
            "SoapAction",
            "Solicitation",
            "Status-URI",
            "Strict-Transport-Security",
            "Subject",
            "SubOK",
            "Subst",
            "Summary",
            "Supersedes",
            "Surrogate-Capability",
            "Surrogate-Control",
            "TCN",
            "TE",
            "Timeout",
            "Title",
            "To",
            "Topic",
            "Trailer",
            "Transfer-Encoding",
            "TTL",
            "UA-Color",
            "UA-Media",
            "UA-Pixels",
            "UA-Resolution",
            "UA-Windowpixels",
            "Upgrade",
            "Urgency",
            "URI",
            "User-Agent",
            "Variant-Vary",
            "Vary",
            "VBR-Info",
            "Version",
            "Via",
            "Want-Digest",
            "Warning",
            "WWW-Authenticate",
            "X-Archived-At",
            "X-Device-Accept",
            "X-Device-Accept-Charset",
            "X-Device-Accept-Encoding",
            "X-Device-Accept-Language",
            "X-Device-User-Agent",
            "X-Frame-Options",
            "X-Mittente",
            "X-PGP-Sig",
            "X-Ricevuta",
            "X-Riferimento-Message-ID",
            "X-TipoRicevuta",
            "X-Trasporto",
            "X-VerificaSicurezza",
            "X400-Content-Identifier",
            "X400-Content-Return",
            "X400-Content-Type",
            "X400-MTS-Identifier",
            "X400-Originator",
            "X400-Received",
            "X400-Recipients",
            "X400-Trace",
            "Xref"
        }})
    {
        // find the longest field length
        std::size_t high = 0;
        for(auto const& s : by_name_)
            if(high < s.size())
                high = s.size();
        // build by_size map
        // skip field::unknown
        by_size_.resize(high + 1);
        for(auto& map : by_size_)
            map.max_load_factor(.15f);
        for(std::size_t i = 1;
            i < by_name_.size(); ++i)
        {
            auto const& s = by_name_[i];
            by_size_[s.size()].emplace(
                s, static_cast<field>(i));
        }

#if 0
        // This snippet calculates the performance
        // of the hash function and map settings
        {
            std::vector<std::size_t> hist;
            for(auto const& map : by_size_)
            {
                for(std::size_t i = 0; i < map.bucket_count(); ++i)
                {
                    auto const n = map.bucket_size(i);
                    if(n > 0)
                    {
                        if(hist.size() < n)
                            hist.resize(n);
                        ++hist[n-1];
                    }
                }
            }
        }
#endif
    }

    field
    string_to_field(string_view s) const
    {
        if(s.size() >= by_size_.size())
            return field::unknown;
        auto const& map = by_size_[s.size()];
        if(map.empty())
            return field::unknown;
        auto it = map.find(s);
        if(it == map.end())
            return field::unknown;
        return it->second;
    }

    //
    // Deprecated
    //

    using const_iterator =
    array_type::const_iterator; 

    std::size_t
    size() const
    {
        return by_name_.size();
    }

    const_iterator
    begin() const
    {
        return by_name_.begin();
    }

    const_iterator
    end() const
    {
        return by_name_.end();
    }
};

inline
field_table const&
get_field_table()
{
    static field_table const tab;
    return tab;
}

template<class = void>
string_view
to_string(field f)
{
    auto const& v = get_field_table();
    BOOST_ASSERT(static_cast<unsigned>(f) < v.size());
    return v.begin()[static_cast<unsigned>(f)];
}

} // detail

inline
string_view
to_string(field f)
{
    return detail::to_string(f);
}

inline
field
string_to_field(string_view s)
{
    return detail::get_field_table().string_to_field(s);
}

} // http
} // beast
} // boost

#endif
