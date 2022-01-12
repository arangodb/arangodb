//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/field.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http {

class field_test : public beast::unit_test::suite
{
public:
    void
    testField()
    {
        auto const match =
            [&](field f, string_view s)
            {
                BEAST_EXPECT(iequals(to_string(f), s));
                BEAST_EXPECT(string_to_field(s) == f);
            };

        match(field::accept, "accept");
        match(field::accept, "aCcept");
        match(field::accept, "ACCEPT");

        match(field::a_im, "A-IM");
        match(field::accept, "Accept");
        match(field::accept_additions, "Accept-Additions");
        match(field::accept_charset, "Accept-Charset");
        match(field::accept_datetime, "Accept-Datetime");
        match(field::accept_encoding, "Accept-Encoding");
        match(field::accept_features, "Accept-Features");
        match(field::accept_language, "Accept-Language");
        match(field::accept_patch, "Accept-Patch");
        match(field::accept_post, "Accept-Post");
        match(field::accept_ranges, "Accept-Ranges");
        match(field::access_control, "Access-Control");
        match(field::access_control_allow_credentials, "Access-Control-Allow-Credentials");
        match(field::access_control_allow_headers, "Access-Control-Allow-Headers");
        match(field::access_control_allow_methods, "Access-Control-Allow-Methods");
        match(field::access_control_allow_origin, "Access-Control-Allow-Origin");
        match(field::access_control_expose_headers, "Access-Control-Expose-Headers");
        match(field::access_control_max_age, "Access-Control-Max-Age");
        match(field::access_control_request_headers, "Access-Control-Request-Headers");
        match(field::access_control_request_method, "Access-Control-Request-Method");
        match(field::age, "Age");
        match(field::allow, "Allow");
        match(field::alpn, "ALPN");
        match(field::also_control, "Also-Control");
        match(field::alt_svc, "Alt-Svc");
        match(field::alt_used, "Alt-Used");
        match(field::alternate_recipient, "Alternate-Recipient");
        match(field::alternates, "Alternates");
        match(field::apparently_to, "Apparently-To");
        match(field::apply_to_redirect_ref, "Apply-To-Redirect-Ref");
        match(field::approved, "Approved");
        match(field::archive, "Archive");
        match(field::archived_at, "Archived-At");
        match(field::article_names, "Article-Names");
        match(field::article_updates, "Article-Updates");
        match(field::authentication_control, "Authentication-Control");
        match(field::authentication_info, "Authentication-Info");
        match(field::authentication_results, "Authentication-Results");
        match(field::authorization, "Authorization");
        match(field::auto_submitted, "Auto-Submitted");
        match(field::autoforwarded, "Autoforwarded");
        match(field::autosubmitted, "Autosubmitted");
        match(field::base, "Base");
        match(field::bcc, "Bcc");
        match(field::body, "Body");
        match(field::c_ext, "C-Ext");
        match(field::c_man, "C-Man");
        match(field::c_opt, "C-Opt");
        match(field::c_pep, "C-PEP");
        match(field::c_pep_info, "C-PEP-Info");
        match(field::cache_control, "Cache-Control");
        match(field::caldav_timezones, "CalDAV-Timezones");
        match(field::cancel_key, "Cancel-Key");
        match(field::cancel_lock, "Cancel-Lock");
        match(field::cc, "Cc");
        match(field::close, "Close");
        match(field::comments, "Comments");
        match(field::compliance, "Compliance");
        match(field::connection, "Connection");
        match(field::content_alternative, "Content-Alternative");
        match(field::content_base, "Content-Base");
        match(field::content_description, "Content-Description");
        match(field::content_disposition, "Content-Disposition");
        match(field::content_duration, "Content-Duration");
        match(field::content_encoding, "Content-Encoding");
        match(field::content_features, "Content-features");
        match(field::content_id, "Content-ID");
        match(field::content_identifier, "Content-Identifier");
        match(field::content_language, "Content-Language");
        match(field::content_length, "Content-Length");
        match(field::content_location, "Content-Location");
        match(field::content_md5, "Content-MD5");
        match(field::content_range, "Content-Range");
        match(field::content_return, "Content-Return");
        match(field::content_script_type, "Content-Script-Type");
        match(field::content_style_type, "Content-Style-Type");
        match(field::content_transfer_encoding, "Content-Transfer-Encoding");
        match(field::content_type, "Content-Type");
        match(field::content_version, "Content-Version");
        match(field::control, "Control");
        match(field::conversion, "Conversion");
        match(field::conversion_with_loss, "Conversion-With-Loss");
        match(field::cookie, "Cookie");
        match(field::cookie2, "Cookie2");
        match(field::cost, "Cost");
        match(field::dasl, "DASL");
        match(field::date, "Date");
        match(field::date_received, "Date-Received");
        match(field::dav, "DAV");
        match(field::default_style, "Default-Style");
        match(field::deferred_delivery, "Deferred-Delivery");
        match(field::delivery_date, "Delivery-Date");
        match(field::delta_base, "Delta-Base");
        match(field::depth, "Depth");
        match(field::derived_from, "Derived-From");
        match(field::destination, "Destination");
        match(field::differential_id, "Differential-ID");
        match(field::digest, "Digest");
        match(field::discarded_x400_ipms_extensions, "Discarded-X400-IPMS-Extensions");
        match(field::discarded_x400_mts_extensions, "Discarded-X400-MTS-Extensions");
        match(field::disclose_recipients, "Disclose-Recipients");
        match(field::disposition_notification_options, "Disposition-Notification-Options");
        match(field::disposition_notification_to, "Disposition-Notification-To");
        match(field::distribution, "Distribution");
        match(field::dkim_signature, "DKIM-Signature");
        match(field::dl_expansion_history, "DL-Expansion-History");
        match(field::downgraded_bcc, "Downgraded-Bcc");
        match(field::downgraded_cc, "Downgraded-Cc");
        match(field::downgraded_disposition_notification_to, "Downgraded-Disposition-Notification-To");
        match(field::downgraded_final_recipient, "Downgraded-Final-Recipient");
        match(field::downgraded_from, "Downgraded-From");
        match(field::downgraded_in_reply_to, "Downgraded-In-Reply-To");
        match(field::downgraded_mail_from, "Downgraded-Mail-From");
        match(field::downgraded_message_id, "Downgraded-Message-Id");
        match(field::downgraded_original_recipient, "Downgraded-Original-Recipient");
        match(field::downgraded_rcpt_to, "Downgraded-Rcpt-To");
        match(field::downgraded_references, "Downgraded-References");
        match(field::downgraded_reply_to, "Downgraded-Reply-To");
        match(field::downgraded_resent_bcc, "Downgraded-Resent-Bcc");
        match(field::downgraded_resent_cc, "Downgraded-Resent-Cc");
        match(field::downgraded_resent_from, "Downgraded-Resent-From");
        match(field::downgraded_resent_reply_to, "Downgraded-Resent-Reply-To");
        match(field::downgraded_resent_sender, "Downgraded-Resent-Sender");
        match(field::downgraded_resent_to, "Downgraded-Resent-To");
        match(field::downgraded_return_path, "Downgraded-Return-Path");
        match(field::downgraded_sender, "Downgraded-Sender");
        match(field::downgraded_to, "Downgraded-To");
        match(field::ediint_features, "EDIINT-Features");
        match(field::eesst_version, "Eesst-Version");
        match(field::encoding, "Encoding");
        match(field::encrypted, "Encrypted");
        match(field::errors_to, "Errors-To");
        match(field::etag, "ETag");
        match(field::expect, "Expect");
        match(field::expires, "Expires");
        match(field::expiry_date, "Expiry-Date");
        match(field::ext, "Ext");
        match(field::followup_to, "Followup-To");
        match(field::forwarded, "Forwarded");
        match(field::from, "From");
        match(field::generate_delivery_report, "Generate-Delivery-Report");
        match(field::getprofile, "GetProfile");
        match(field::hobareg, "Hobareg");
        match(field::host, "Host");
        match(field::http2_settings, "HTTP2-Settings");
        match(field::if_, "If");
        match(field::if_match, "If-Match");
        match(field::if_modified_since, "If-Modified-Since");
        match(field::if_none_match, "If-None-Match");
        match(field::if_range, "If-Range");
        match(field::if_schedule_tag_match, "If-Schedule-Tag-Match");
        match(field::if_unmodified_since, "If-Unmodified-Since");
        match(field::im, "IM");
        match(field::importance, "Importance");
        match(field::in_reply_to, "In-Reply-To");
        match(field::incomplete_copy, "Incomplete-Copy");
        match(field::injection_date, "Injection-Date");
        match(field::injection_info, "Injection-Info");
        match(field::jabber_id, "Jabber-ID");
        match(field::keep_alive, "Keep-Alive");
        match(field::keywords, "Keywords");
        match(field::label, "Label");
        match(field::language, "Language");
        match(field::last_modified, "Last-Modified");
        match(field::latest_delivery_time, "Latest-Delivery-Time");
        match(field::lines, "Lines");
        match(field::link, "Link");
        match(field::list_archive, "List-Archive");
        match(field::list_help, "List-Help");
        match(field::list_id, "List-ID");
        match(field::list_owner, "List-Owner");
        match(field::list_post, "List-Post");
        match(field::list_subscribe, "List-Subscribe");
        match(field::list_unsubscribe, "List-Unsubscribe");
        match(field::list_unsubscribe_post, "List-Unsubscribe-Post");
        match(field::location, "Location");
        match(field::lock_token, "Lock-Token");
        match(field::man, "Man");
        match(field::max_forwards, "Max-Forwards");
        match(field::memento_datetime, "Memento-Datetime");
        match(field::message_context, "Message-Context");
        match(field::message_id, "Message-ID");
        match(field::message_type, "Message-Type");
        match(field::meter, "Meter");
        match(field::method_check, "Method-Check");
        match(field::method_check_expires, "Method-Check-Expires");
        match(field::mime_version, "MIME-Version");
        match(field::mmhs_acp127_message_identifier, "MMHS-Acp127-Message-Identifier");
        match(field::mmhs_authorizing_users, "MMHS-Authorizing-Users");
        match(field::mmhs_codress_message_indicator, "MMHS-Codress-Message-Indicator");
        match(field::mmhs_copy_precedence, "MMHS-Copy-Precedence");
        match(field::mmhs_exempted_address, "MMHS-Exempted-Address");
        match(field::mmhs_extended_authorisation_info, "MMHS-Extended-Authorisation-Info");
        match(field::mmhs_handling_instructions, "MMHS-Handling-Instructions");
        match(field::mmhs_message_instructions, "MMHS-Message-Instructions");
        match(field::mmhs_message_type, "MMHS-Message-Type");
        match(field::mmhs_originator_plad, "MMHS-Originator-PLAD");
        match(field::mmhs_originator_reference, "MMHS-Originator-Reference");
        match(field::mmhs_other_recipients_indicator_cc, "MMHS-Other-Recipients-Indicator-CC");
        match(field::mmhs_other_recipients_indicator_to, "MMHS-Other-Recipients-Indicator-To");
        match(field::mmhs_primary_precedence, "MMHS-Primary-Precedence");
        match(field::mmhs_subject_indicator_codes, "MMHS-Subject-Indicator-Codes");
        match(field::mt_priority, "MT-Priority");
        match(field::negotiate, "Negotiate");
        match(field::newsgroups, "Newsgroups");
        match(field::nntp_posting_date, "NNTP-Posting-Date");
        match(field::nntp_posting_host, "NNTP-Posting-Host");
        match(field::non_compliance, "Non-Compliance");
        match(field::obsoletes, "Obsoletes");
        match(field::opt, "Opt");
        match(field::optional, "Optional");
        match(field::optional_www_authenticate, "Optional-WWW-Authenticate");
        match(field::ordering_type, "Ordering-Type");
        match(field::organization, "Organization");
        match(field::origin, "Origin");
        match(field::original_encoded_information_types, "Original-Encoded-Information-Types");
        match(field::original_from, "Original-From");
        match(field::original_message_id, "Original-Message-ID");
        match(field::original_recipient, "Original-Recipient");
        match(field::original_sender, "Original-Sender");
        match(field::original_subject, "Original-Subject");
        match(field::originator_return_address, "Originator-Return-Address");
        match(field::overwrite, "Overwrite");
        match(field::p3p, "P3P");
        match(field::path, "Path");
        match(field::pep, "PEP");
        match(field::pep_info, "Pep-Info");
        match(field::pics_label, "PICS-Label");
        match(field::position, "Position");
        match(field::posting_version, "Posting-Version");
        match(field::pragma, "Pragma");
        match(field::prefer, "Prefer");
        match(field::preference_applied, "Preference-Applied");
        match(field::prevent_nondelivery_report, "Prevent-NonDelivery-Report");
        match(field::priority, "Priority");
        match(field::privicon, "Privicon");
        match(field::profileobject, "ProfileObject");
        match(field::protocol, "Protocol");
        match(field::protocol_info, "Protocol-Info");
        match(field::protocol_query, "Protocol-Query");
        match(field::protocol_request, "Protocol-Request");
        match(field::proxy_authenticate, "Proxy-Authenticate");
        match(field::proxy_authentication_info, "Proxy-Authentication-Info");
        match(field::proxy_authorization, "Proxy-Authorization");
        match(field::proxy_connection, "Proxy-Connection");
        match(field::proxy_features, "Proxy-Features");
        match(field::proxy_instruction, "Proxy-Instruction");
        match(field::public_, "Public");
        match(field::public_key_pins, "Public-Key-Pins");
        match(field::public_key_pins_report_only, "Public-Key-Pins-Report-Only");
        match(field::range, "Range");
        match(field::received, "Received");
        match(field::received_spf, "Received-SPF");
        match(field::redirect_ref, "Redirect-Ref");
        match(field::references, "References");
        match(field::referer, "Referer");
        match(field::referer_root, "Referer-Root");
        match(field::relay_version, "Relay-Version");
        match(field::reply_by, "Reply-By");
        match(field::reply_to, "Reply-To");
        match(field::require_recipient_valid_since, "Require-Recipient-Valid-Since");
        match(field::resent_bcc, "Resent-Bcc");
        match(field::resent_cc, "Resent-Cc");
        match(field::resent_date, "Resent-Date");
        match(field::resent_from, "Resent-From");
        match(field::resent_message_id, "Resent-Message-ID");
        match(field::resent_reply_to, "Resent-Reply-To");
        match(field::resent_sender, "Resent-Sender");
        match(field::resent_to, "Resent-To");
        match(field::resolution_hint, "Resolution-Hint");
        match(field::resolver_location, "Resolver-Location");
        match(field::retry_after, "Retry-After");
        match(field::return_path, "Return-Path");
        match(field::safe, "Safe");
        match(field::schedule_reply, "Schedule-Reply");
        match(field::schedule_tag, "Schedule-Tag");
        match(field::sec_fetch_dest, "Sec-Fetch-Dest");
        match(field::sec_fetch_mode, "Sec-Fetch-Mode");
        match(field::sec_fetch_site, "Sec-Fetch-Site");
        match(field::sec_fetch_user, "Sec-Fetch-User");
        match(field::sec_websocket_accept, "Sec-WebSocket-Accept");
        match(field::sec_websocket_extensions, "Sec-WebSocket-Extensions");
        match(field::sec_websocket_key, "Sec-WebSocket-Key");
        match(field::sec_websocket_protocol, "Sec-WebSocket-Protocol");
        match(field::sec_websocket_version, "Sec-WebSocket-Version");
        match(field::security_scheme, "Security-Scheme");
        match(field::see_also, "See-Also");
        match(field::sender, "Sender");
        match(field::sensitivity, "Sensitivity");
        match(field::server, "Server");
        match(field::set_cookie, "Set-Cookie");
        match(field::set_cookie2, "Set-Cookie2");
        match(field::setprofile, "SetProfile");
        match(field::sio_label, "SIO-Label");
        match(field::sio_label_history, "SIO-Label-History");
        match(field::slug, "SLUG");
        match(field::soapaction, "SoapAction");
        match(field::solicitation, "Solicitation");
        match(field::status_uri, "Status-URI");
        match(field::strict_transport_security, "Strict-Transport-Security");
        match(field::subject, "Subject");
        match(field::subok, "SubOK");
        match(field::subst, "Subst");
        match(field::summary, "Summary");
        match(field::supersedes, "Supersedes");
        match(field::surrogate_capability, "Surrogate-Capability");
        match(field::surrogate_control, "Surrogate-Control");
        match(field::tcn, "TCN");
        match(field::te, "TE");
        match(field::timeout, "Timeout");
        match(field::title, "Title");
        match(field::to, "To");
        match(field::topic, "Topic");
        match(field::trailer, "Trailer");
        match(field::transfer_encoding, "Transfer-Encoding");
        match(field::ttl, "TTL");
        match(field::ua_color, "UA-Color");
        match(field::ua_media, "UA-Media");
        match(field::ua_pixels, "UA-Pixels");
        match(field::ua_resolution, "UA-Resolution");
        match(field::ua_windowpixels, "UA-Windowpixels");
        match(field::upgrade, "Upgrade");
        match(field::urgency, "Urgency");
        match(field::uri, "URI");
        match(field::user_agent, "User-Agent");
        match(field::variant_vary, "Variant-Vary");
        match(field::vary, "Vary");
        match(field::vbr_info, "VBR-Info");
        match(field::version, "Version");
        match(field::via, "Via");
        match(field::want_digest, "Want-Digest");
        match(field::warning, "Warning");
        match(field::www_authenticate, "WWW-Authenticate");
        match(field::x_archived_at, "X-Archived-At");
        match(field::x_device_accept, "X-Device-Accept");
        match(field::x_device_accept_charset, "X-Device-Accept-Charset");
        match(field::x_device_accept_encoding, "X-Device-Accept-Encoding");
        match(field::x_device_accept_language, "X-Device-Accept-Language");
        match(field::x_device_user_agent, "X-Device-User-Agent");
        match(field::x_frame_options, "X-Frame-Options");
        match(field::x_mittente, "X-Mittente");
        match(field::x_pgp_sig, "X-PGP-Sig");
        match(field::x_ricevuta, "X-Ricevuta");
        match(field::x_riferimento_message_id, "X-Riferimento-Message-ID");
        match(field::x_tiporicevuta, "X-TipoRicevuta");
        match(field::x_trasporto, "X-Trasporto");
        match(field::x_verificasicurezza, "X-VerificaSicurezza");
        match(field::x400_content_identifier, "X400-Content-Identifier");
        match(field::x400_content_return, "X400-Content-Return");
        match(field::x400_content_type, "X400-Content-Type");
        match(field::x400_mts_identifier, "X400-MTS-Identifier");
        match(field::x400_originator, "X400-Originator");
        match(field::x400_received, "X400-Received");
        match(field::x400_recipients, "X400-Recipients");
        match(field::x400_trace, "X400-Trace");
        match(field::xref, "Xref");

        auto const unknown =
            [&](string_view s)
            {
                BEAST_EXPECT(string_to_field(s) == field::unknown);
            };
        unknown("");
        unknown("x");
    }

    void run() override
    {
        testField();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,field);

} // http
} // beast
} // boost
