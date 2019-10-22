//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_TEST_MESSAGE_FUZZ_HPP
#define BOOST_BEAST_HTTP_TEST_MESSAGE_FUZZ_HPP

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/http/detail/rfc7230.hpp>
#include <cstdint>
#include <random>
#include <string>

namespace boost {
namespace beast {
namespace http {

template<class = void>
std::string
escaped_string(string_view s)
{
    std::string out;
    out.reserve(s.size());
    for(char c : s)
    {
        if(c == '\r')
            out.append("\\r");
        else if(c == '\n')
            out.append("\\n");
        else if(c == '\t')
            out.append("\\t");
        else
            out.append(&c, 1);
    }
    return out;
}

// Produces random HTTP messages
//
template<class = void>
class message_fuzz_t
{
    std::mt19937 rng_;

    static
    std::string
    to_hex(std::size_t v)
    {
        if(! v)
            return "0";
        std::string s;
        while(v > 0)
        {
            s.insert(s.begin(),
                "0123456789abcdef"[v&0xf]);
            v >>= 4;
        }
        return s;
    }

public:
    template<class UInt = std::size_t>
    UInt
    rand(std::size_t n)
    {
        return static_cast<UInt>(
            std::uniform_int_distribution<
                std::size_t>{0, n-1}(rng_));
    }

    std::string
    method()
    {
#if 0
        // All IANA registered methods
        static char const* const list[39] = {
            "ACL", "BASELINE-CONTROL", "BIND", "CHECKIN", "CHECKOUT", "CONNECT",
            "COPY", "DELETE", "GET", "HEAD", "LABEL", "LINK", "LOCK", "MERGE",
            "MKACTIVITY", "MKCALENDAR", "MKCOL", "MKREDIRECTREF", "MKWORKSPACE",
            "MOVE", "OPTIONS", "ORDERPATCH", "PATCH", "POST", "PRI", "PROPFIND",
            "PROPPATCH", "PUT", "REBIND", "REPORT", "SEARCH", "TRACE", "UNBIND",
            "UNCHECKOUT", "UNLINK", "UNLOCK", "UPDATE", "UPDATEREDIRECTREF",
            "VERSION-CONTROL"
        };
        return list[rand(39)];
#else
        // methods parsed by nodejs-http-parser
        static char const* const list[33] = {
            "ACL", "BIND", "CHECKOUT", "CONNECT", "COPY", "DELETE", "HEAD", "GET",
            "LINK", "LOCK", "MERGE", "MKCOL", "MKCALENDAR", "MKACTIVITY", "M-SEARCH",
            "MOVE", "NOTIFY", "OPTIONS", "PATCH", "POST", "PROPFIND", "PROPPATCH",
            "PURGE", "PUT", "REBIND", "REPORT", "SEARCH", "SUBSCRIBE", "TRACE",
            "UNBIND", "UNLINK", "UNLOCK", "UNSUBSCRIBE"
        };
        return list[rand(33)];
#endif
    }

    std::string
    scheme()
    {
        static char const* const list[241] = {
            "aaa", "aaas", "about", "acap", "acct", "acr", "adiumxtra", "afp", "afs",
            "aim", "appdata", "apt", "attachment", "aw", "barion", "beshare", "bitcoin",
            "blob", "bolo", "callto", "cap", "chrome", "chrome-extension", "cid",
            "coap", "coaps", "com-eventbrite-attendee", "content", "crid", "cvs",
            "data", "dav", "dict", "dis", "dlna-playcontainer", "dlna-playsingle",
            "dns", "dntp", "dtn", "dvb", "ed2k", "example", "facetime", "fax", "feed",
            "feedready", "file", "filesystem", "finger", "fish", "ftp", "geo", "gg",
            "git", "gizmoproject", "go", "gopher", "gtalk", "h323", "ham", "hcp",
            "http", "https", "iax", "icap", "icon", "im", "imap", "info", "iotdisco",
            "ipn", "ipp", "ipps", "irc", "irc6", "ircs", "iris", "iris.beep",
            "iris.lwz", "iris.xpc", "iris.xpcs", "isostore", "itms", "jabber", "jar",
            "jms", "keyparc", "lastfm", "ldap", "ldaps", "magnet", "mailserver",
            "mailto", "maps", "market", "message", "mid", "mms",
            "modem", "ms-access", "ms-drive-to", "ms-enrollment", "ms-excel",
            "ms-getoffice", "ms-help", "ms-infopath", "ms-media-stream-id", "ms-project",
            "ms-powerpoint", "ms-publisher", "ms-search-repair",
            "ms-secondary-screen-controller", "ms-secondary-screen-setup",
            "ms-settings", "ms-settings-airplanemode", "ms-settings-bluetooth",
            "ms-settings-camera", "ms-settings-cellular", "ms-settings-cloudstorage",
            "ms-settings-emailandaccounts", "ms-settings-language", "ms-settings-location",
            "ms-settings-lock", "ms-settings-nfctransactions", "ms-settings-notifications",
            "ms-settings-power", "ms-settings-privacy", "ms-settings-proximity",
            "ms-settings-screenrotation", "ms-settings-wifi", "ms-settings-workplace",
            "ms-spd", "ms-transit-to", "ms-visio", "ms-walk-to", "ms-word", "msnim",
            "msrp", "msrps", "mtqp", "mumble", "mupdate", "mvn", "news", "nfs", "ni",
            "nih", "nntp", "notes", "oid", "opaquelocktoken", "pack", "palm", "paparazzi",
            "pkcs11", "platform", "pop", "pres", "prospero", "proxy", "psyc", "query",
            "redis", "rediss", "reload", "res", "target", "rmi", "rsync", "rtmfp",
            "rtmp", "rtsp", "rtsps", "rtspu", "secondlife", "service", "session", "sftp",
            "sgn", "shttp", "sieve", "sip", "sips", "skype", "smb", "sms", "smtp",
            "snews", "snmp", "soap.beep", "soap.beeps", "soldat", "spotify", "ssh",
            "steam", "stun", "stuns", "submit", "svn", "tag", "teamspeak", "tel",
            "teliaeid", "telnet", "tftp", "things", "thismessage", "tip", "tn3270",
            "tool", "turn", "turns", "tv", "udp", "unreal", "urn", "ut2004", "v-event",
            "vemmi", "ventrilo", "videotex", "vnc", "view-source", "wais", "webcal",
            "wpid", "ws", "wss", "wtai", "wyciwyg", "xcon", "xcon-userid", "xfire",
            "xmlrpc.beep", "xmlrpc.beeps", "xmpp", "xri", "ymsgr", "z39.50", "z39.50r",
            "z39.50s:"
        };
        return list[rand(241)];
    }

    std::string
    pchar()
    {
        if(rand(4))
            return std::string(1,
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz"
                ":@&=+$,"[rand(69)]);
        std::string s = "%";
        s += "0123456789abcdef"[rand(16)];
        s += "0123456789abcdef"[rand(16)];
        return s;
    }

    char
    uric()
    {
        return 'a';
    }

    char
    uric_no_slash()
    {
        return 'a';
    }

    std::string
    param()
    {
        std::string s;
        while(rand(2))
            s += pchar();
        return s;
    }

    std::string
    query()
    {
        std::string s;
        while(rand(2))
            s += uric();
        return s;
    }

    std::string
    userinfo()
    {
        std::string s;
        while(rand(2))
            s += "a";
        return s;
    }

    /*  authority     = server | reg_name

        reg_name      = 1*( unreserved | escaped | "$" | "," |
                        ";" | ":" | "@" | "&" | "=" | "+" )

        server        = [ [ userinfo "@" ] hostport ]
        userinfo      = *( unreserved | escaped |
                         ";" | ":" | "&" | "=" | "+" | "$" | "," )

        hostport      = host [ ":" port ]
        host          = hostname | IPv4address
        hostname      = *( domainlabel "." ) toplabel [ "." ]
        domainlabel   = alphanum | alphanum *( alphanum | "-" ) alphanum
        toplabel      = alpha | alpha *( alphanum | "-" ) alphanum
        IPv4address   = 1*digit "." 1*digit "." 1*digit "." 1*digit
        port          = *digit
    */
    std::string
    server()
    {
        std::string s;
        if(rand(2))
            s += userinfo() + "@";
        return s;
    }

    std::string
    reg_name()
    {
        std::string s;
        s = "a";
        while(rand(2))
            s += "a";
        return s;
    }

    std::string
    authority()
    {
        if(rand(2))
            return server();
        return reg_name();
    }

    std::string
    opaque_part()
    {
        std::string s;
        s += uric_no_slash();
        while(rand(2))
            s += uric();
        return s;
    }

    /*  abs_path      = "/"  path_segments
        path_segments = segment *( "/" segment )
        segment       = *pchar *( ";" param )
        param         = *pchar
        pchar         = unreserved | escaped |
                        ":" | "@" | "&" | "=" | "+" | "$" | ","
    */
    std::string
    abs_path()
    {
        std::string s = "/";
        for(;;)
        {
            while(rand(2))
                s += pchar();
            while(rand(2))
                s += ";" + param();
            if(rand(2))
                break;
            s.append("/");
        }
        return s;
    }

    /*  net_path      = "//" authority [ abs_path ]
    */
    std::string
    net_path()
    {
        std::string s = "//";
        s += authority();
        if(rand(2))
            s += abs_path();
        return s;
    }

    /*  absoluteURI   = scheme ":" ( hier_part | opaque_part )
        scheme        = alpha *( alpha | digit | "+" | "-" | "." )
        hier_part     = ( net_path | abs_path ) [ "?" query ]
        abs_path      = "/"  path_segments
        query         = *uric
        opaque_part   = uric_no_slash *uric
    */
    std::string
    abs_uri()
    {
        std::string s;
        s = scheme() + ":";
        if(rand(2))
        {
            if(rand(2))
                s += net_path();
            else
                s += abs_path();
            if(rand(2))
                s += "?" + query();
        }
        else
        {
            s += opaque_part();
        }
        return s;
    }

    std::string
    target()
    {
        //switch(rand(4))
        switch(1)
        {
        case 0: return abs_uri();
        case 1: return abs_path();
        case 2: return authority();
        default:
        case 3: break;
        }
        return "*";
    }

    std::string
    token()
    {
        static char constexpr valid[78] =
            "!#$%&\'*+-." "0123456789" "ABCDEFGHIJ" "KLMNOPQRST"
            "UVWXYZ^_`a"  "bcdefghijk" "lmnopqrstu" "vwxyz|~";
        std::string s;
        s.append(1, valid[rand(77)]);
        while(rand(4))
            s.append(1, valid[rand(77)]);
        return s;
    }

#if 0
    std::string
    target()
    {
        static char constexpr alpha[63] =
            "0123456789" "ABCDEFGHIJ" "KLMNOPQRST"
            "UVWXYZabcd" "efghijklmn" "opqrstuvwx" "yz";
        std::string s;
        s = "/";
        while(rand(4))
            s.append(1, alpha[rand(62)]);
        return s;
    }
#endif

    std::string
    field()
    {        static char const* const list[289] =
        {
            "A-IM",
            "Accept", "Accept-Additions", "Accept-Charset", "Accept-Datetime", "Accept-Encoding",
            "Accept-Features", "Accept-Language", "Accept-Patch", "Accept-Ranges", "Age", "Allow",
            "ALPN", "Also-Control", "Alt-Svc", "Alt-Used", "Alternate-Recipient", "Alternates",
            "Apply-To-Redirect-Ref", "Approved", "Archive", "Archived-At", "Article-Names",
            "Article-Updates", "Authentication-Info", "Authentication-Results", "Authorization",
            "Auto-Submitted", "Autoforwarded", "Autosubmitted", "Base", "Bcc", "Body", "C-Ext",
            "C-Man", "C-Opt", "C-PEP", "C-PEP-Info", "Cache-Control",
            "CalDAV-Timezones", "Cc", "Close", "Comments", /*"Connection",*/ "Content-Alternative",
            "Content-Base", "Content-Description", "Content-Disposition", "Content-Duration",
            "Content-Encoding", "Content-features", "Content-ID", "Content-Identifier",
            "Content-Language", /*"Content-Length",*/ "Content-Location", "Content-MD5",
            "Content-Range", "Content-Return", "Content-Script-Type", "Content-Style-Type",
            "Content-Transfer-Encoding", "Content-Type", "Content-Version", "Control", "Conversion",
            "Conversion-With-Loss", "Cookie", "Cookie2", "DASL", "DAV", "DL-Expansion-History", "Date",
            "Date-Received", "Default-Style", "Deferred-Delivery", "Delivery-Date", "Delta-Base",
            "Depth", "Derived-From", "Destination", "Differential-ID", "Digest",
            "Discarded-X400-IPMS-Extensions", "Discarded-X400-MTS-Extensions", "Disclose-Recipients",
            "Disposition-Notification-Options", "Disposition-Notification-To", "Distribution",
            "DKIM-Signature", "Downgraded-Bcc", "Downgraded-Cc", "Downgraded-Disposition-Notification-To",
            "Downgraded-Final-Recipient", "Downgraded-From", "Downgraded-In-Reply-To",
            "Downgraded-Mail-From", "Downgraded-Message-Id", "Downgraded-Original-Recipient",
            "Downgraded-Rcpt-To", "Downgraded-References", "Downgraded-Reply-To", "Downgraded-Resent-Bcc",
            "Downgraded-Resent-Cc", "Downgraded-Resent-From", "Downgraded-Resent-Reply-To",
            "Downgraded-Resent-Sender", "Downgraded-Resent-To", "Downgraded-Return-Path",
            "Downgraded-Sender", "Downgraded-To", "Encoding", "Encrypted", "ETag", "Expect",
            "Expires", "Expiry-Date", "Ext", "Followup-To", "Forwarded", "From",
            "Generate-Delivery-Report", "GetProfile", "Hobareg", "Host", "HTTP2-Settings", "IM", "If",
            "If-Match", "If-Modified-Since", "If-None-Match", "If-Range", "If-Schedule-Tag-Match",
            "If-Unmodified-Since", "Importance", "In-Reply-To", "Incomplete-Copy", "Injection-Date",
            "Injection-Info", "Keep-Alive", "Keywords", "Label", "Language", "Last-Modified",
            "Latest-Delivery-Time", "Lines", "Link", "List-Archive", "List-Help", "List-ID",
            "List-Owner", "List-Post", "List-Subscribe", "List-Unsubscribe", "Location", "Lock-Token",
            "Man", "Max-Forwards", "Memento-Datetime", "Message-Context", "Message-ID", "Message-Type",
            "Meter", "MIME-Version", "MMHS-Exempted-Address", "MMHS-Extended-Authorisation-Info",
            "MMHS-Subject-Indicator-Codes", "MMHS-Handling-Instructions", "MMHS-Message-Instructions",
            "MMHS-Codress-Message-Indicator", "MMHS-Originator-Reference", "MMHS-Primary-Precedence",
            "MMHS-Copy-Precedence", "MMHS-Message-Type", "MMHS-Other-Recipients-Indicator-To",
            "MMHS-Other-Recipients-Indicator-CC", "MMHS-Acp127-Message-Identifier", "MMHS-Originator-PLAD",
            "MT-Priority", "Negotiate", "Newsgroups", "NNTP-Posting-Date", "NNTP-Posting-Host",
            "Obsoletes", "Opt", "Ordering-Type", "Organization", "Origin",
            "Original-Encoded-Information-Types", "Original-From", "Original-Message-ID",
            "Original-Recipient", "Original-Sender", "Originator-Return-Address", "Original-Subject",
            "Overwrite", "P3P", "Path", "PEP", "PICS-Label", "Pep-Info", "Position", "Posting-Version",
            "Pragma", "Prefer", "Preference-Applied", "Prevent-NonDelivery-Report", "Priority",
            "ProfileObject", "Protocol", "Protocol-Info", "Protocol-Query", "Protocol-Request",
            "Proxy-Authenticate", "Proxy-Authentication-Info", "Proxy-Authorization", "Proxy-Features",
            "Proxy-Instruction", "Public", "Public-Key-Pins", "Public-Key-Pins-Report-Only", "Range",
            "Received", "Received-SPF", "Redirect-Ref", "References", "Referer", "Relay-Version",
            "Reply-By", "Reply-To", "Require-Recipient-Valid-Since", "Resent-Bcc", "Resent-Cc",
            "Resent-Date", "Resent-From", "Resent-Message-ID", "Resent-Reply-To", "Resent-Sender",
            "Resent-To", "Retry-After", "Return-Path", "Safe", "Schedule-Reply", "Schedule-Tag",
            "Sec-WebSocket-Accept", "Sec-WebSocket-Extensions", "Sec-WebSocket-Key",
            "Sec-WebSocket-Protocol", "Sec-WebSocket-Version", "Security-Scheme", "See-Also", "Sender",
            "Sensitivity", "Server", "Set-Cookie", "Set-Cookie2",
            "SetProfile", "SLUG", "SoapAction", "Solicitation", "Status-URI", "Strict-Transport-Security",
            "Subject", "Summary", "Supersedes", "Surrogate-Capability", "Surrogate-Control", "TCN",
            "TE", "Timeout", "To", "Trailer", /*"Transfer-Encoding",*/ "URI", /*"Upgrade",*/ "User-Agent",
            "Variant-Vary", "Vary", "VBR-Info", "Via", "WWW-Authenticate", "Want-Digest", "Warning",
            "X400-Content-Identifier", "X400-Content-Return", "X400-Content-Type", "X400-MTS-Identifier",
            "X400-Originator", "X400-Received", "X400-Recipients", "X400-Trace", "X-Frame-Options", "Xref"
        };
        return list[rand(289)];
    }

    std::string
    text()
    {
        std::string s;
        while(rand(3))
        {
            for(;;)
            {
                char c = rand<char>(256);
                if(detail::is_text(c))
                {
                    s.append(1, c);
                    break;
                }
            }
        }
        return s;
    }

    std::string
    value()
    {
        std::string s;
        while(rand(3))
        {
            if(rand(5))
            {
                s.append(text());
            }
            else
            {
                // LWS
                if(! rand(4))
                    s.append("\r\n");
                s.append(1, rand(2) ? ' ' : '\t');
                while(rand(2))
                    s.append(1, rand(2) ? ' ' : '\t');
            }
        }
        return s;
    }

    template<class DynamicBuffer>
    void
    fields(DynamicBuffer& db)
    {
        auto os = ostream(db);
        while(rand(6))
            os <<
                field() <<
                (rand(4) ? ": " : ":") <<
                value() <<
                "\r\n";
    }

    template<class DynamicBuffer>
    void
    body(DynamicBuffer& db)
    {
        if(! rand(4))
        {
            ostream(db) <<
                "Content-Length: 0\r\n\r\n";
            return;
        }
        if(rand(2))
        {
            auto const len = rand(500);
            ostream(db) <<
                "Content-Length: " << len << "\r\n\r\n";
            auto mb = db.prepare(len);
            for(auto it = net::buffer_sequence_begin(mb);
                it != net::buffer_sequence_end(mb);
                ++it)
            {
                net::mutable_buffer b = *it;
                auto p = static_cast<char*>(b.data());
                auto n = b.size();
                while(n--)
                    *p++ = static_cast<char>(32 + rand(26+26+10+6));
            }
            db.commit(len);
        }
        else
        {
            auto len = rand(500);
            ostream(db) <<
                "Transfer-Encoding: chunked\r\n\r\n";
            while(len > 0)
            {
                auto n = (std::min)(1 + rand(300), len);
                len -= n;
                ostream(db) <<
                    to_hex(n) << "\r\n";
                auto mb = db.prepare(n);
                for(auto it = net::buffer_sequence_begin(mb);
                    it != net::buffer_sequence_end(mb);
                    ++it)
                {
                    net::mutable_buffer b = *it;
                    auto p = static_cast<char*>(b.data());
                    auto m = b.size();
                    while(m--)
                        *p++ = static_cast<char>(32 + rand(26+26+10+6));
                }
                db.commit(n);
                ostream(db) << "\r\n";
            }
            ostream(db) << "0\r\n\r\n";
        }
    }

    template<class DynamicBuffer>
    void
    request(DynamicBuffer& db)
    {
        ostream(db) <<
            method() << " " << target() << " HTTP/1.1\r\n";
        fields(db);
        body(db);
    }

    template<class DynamicBuffer>
    void
    response(DynamicBuffer& db)
    {
        ostream(db) <<
            "HTTP/1." <<
            (rand(2) ? "0" : "1") << " " <<
            (100 + rand(401)) << " " <<
            token() <<
            "\r\n";
        fields(db);
        body(db);
        ostream(db) << "\r\n";
    }
};

using message_fuzz = message_fuzz_t<>;

template<class Good, class Bad>
void
chunkExtensionsTest(
    Good const& good, Bad const& bad)
{
    good("");
    good(";x");
    good(";x;y");
    good(";x=y");
    good(";x;y=z");
    good(" ;x");
    good("\t;x");
    good(" \t;x");
    good("\t ;x");
    good(" ; x");
    good(" ;\tx");
    good("\t ; \tx");
    good(";x= y");
    good(" ;x= y");
    good(" ; x= y");
    good(R"(;x="\"")");
    good(R"(;x="\\")");
    good(R"(;x;y=z;z="\"";p="\\";q="1\"2\\")");

    bad(" ");
    bad(";");
    bad("=");
    bad(" ;");
    bad("; ");
    bad(" ; ");
    bad(" ; x ");
    bad(";x =");
    bad(";x = ");
    bad(";x==");
}

} // http
} // beast
} // boost

#endif
