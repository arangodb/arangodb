//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/status.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http {

class status_test
    : public beast::unit_test::suite
{
public:
    void
    testStatus()
    {
        auto const check = [&](status s, int i, status_class sc)
            {
                BEAST_EXPECT(int_to_status(i) == s);
                BEAST_EXPECT(to_status_class(i) == sc);
                BEAST_EXPECT(to_status_class(int_to_status(i)) == sc);
            };
        check(status::continue_                             ,100, status_class::informational);
        check(status::switching_protocols                   ,101, status_class::informational);
        check(status::processing                            ,102, status_class::informational);

        check(status::ok                                    ,200, status_class::successful);
        check(status::created                               ,201, status_class::successful);
        check(status::accepted                              ,202, status_class::successful);
        check(status::non_authoritative_information         ,203, status_class::successful);
        check(status::no_content                            ,204, status_class::successful);
        check(status::reset_content                         ,205, status_class::successful);
        check(status::partial_content                       ,206, status_class::successful);
        check(status::multi_status                          ,207, status_class::successful);
        check(status::already_reported                      ,208, status_class::successful);
        check(status::im_used                               ,226, status_class::successful);

        check(status::multiple_choices                      ,300, status_class::redirection);
        check(status::moved_permanently                     ,301, status_class::redirection);
        check(status::found                                 ,302, status_class::redirection);
        check(status::see_other                             ,303, status_class::redirection);
        check(status::not_modified                          ,304, status_class::redirection);
        check(status::use_proxy                             ,305, status_class::redirection);
        check(status::temporary_redirect                    ,307, status_class::redirection);
        check(status::permanent_redirect                    ,308, status_class::redirection);

        check(status::bad_request                           ,400, status_class::client_error);
        check(status::unauthorized                          ,401, status_class::client_error);
        check(status::payment_required                      ,402, status_class::client_error);
        check(status::forbidden                             ,403, status_class::client_error);
        check(status::not_found                             ,404, status_class::client_error);
        check(status::method_not_allowed                    ,405, status_class::client_error);
        check(status::not_acceptable                        ,406, status_class::client_error);
        check(status::proxy_authentication_required         ,407, status_class::client_error);
        check(status::request_timeout                       ,408, status_class::client_error);
        check(status::conflict                              ,409, status_class::client_error);
        check(status::gone                                  ,410, status_class::client_error);
        check(status::length_required                       ,411, status_class::client_error);
        check(status::precondition_failed                   ,412, status_class::client_error);
        check(status::payload_too_large                     ,413, status_class::client_error);
        check(status::uri_too_long                          ,414, status_class::client_error);
        check(status::unsupported_media_type                ,415, status_class::client_error);
        check(status::range_not_satisfiable                 ,416, status_class::client_error);
        check(status::expectation_failed                    ,417, status_class::client_error);
        check(status::misdirected_request                   ,421, status_class::client_error);
        check(status::unprocessable_entity                  ,422, status_class::client_error);
        check(status::locked                                ,423, status_class::client_error);
        check(status::failed_dependency                     ,424, status_class::client_error);
        check(status::upgrade_required                      ,426, status_class::client_error);
        check(status::precondition_required                 ,428, status_class::client_error);
        check(status::too_many_requests                     ,429, status_class::client_error);
        check(status::request_header_fields_too_large       ,431, status_class::client_error);
        check(status::connection_closed_without_response    ,444, status_class::client_error);
        check(status::unavailable_for_legal_reasons         ,451, status_class::client_error);
        check(status::client_closed_request                 ,499, status_class::client_error);

        check(status::internal_server_error                 ,500, status_class::server_error);
        check(status::not_implemented                       ,501, status_class::server_error);
        check(status::bad_gateway                           ,502, status_class::server_error);
        check(status::service_unavailable                   ,503, status_class::server_error);
        check(status::gateway_timeout                       ,504, status_class::server_error);
        check(status::http_version_not_supported            ,505, status_class::server_error);
        check(status::variant_also_negotiates               ,506, status_class::server_error);
        check(status::insufficient_storage                  ,507, status_class::server_error);
        check(status::loop_detected                         ,508, status_class::server_error);
        check(status::not_extended                          ,510, status_class::server_error);
        check(status::network_authentication_required       ,511, status_class::server_error);
        check(status::network_connect_timeout_error         ,599, status_class::server_error);

        BEAST_EXPECT(to_status_class(1) == status_class::unknown);
        BEAST_EXPECT(to_status_class(status::unknown) == status_class::unknown);

        auto const good =
            [&](status v)
            {
                BEAST_EXPECT(obsolete_reason(v) != "Unknown Status");
            };
        good(status::continue_);
        good(status::switching_protocols);
        good(status::processing);
        good(status::ok);
        good(status::created);
        good(status::accepted);
        good(status::non_authoritative_information);
        good(status::no_content);
        good(status::reset_content);
        good(status::partial_content);
        good(status::multi_status);
        good(status::already_reported);
        good(status::im_used);
        good(status::multiple_choices);
        good(status::moved_permanently);
        good(status::found);
        good(status::see_other);
        good(status::not_modified);
        good(status::use_proxy);
        good(status::temporary_redirect);
        good(status::permanent_redirect);
        good(status::bad_request);
        good(status::unauthorized);
        good(status::payment_required);
        good(status::forbidden);
        good(status::not_found);
        good(status::method_not_allowed);
        good(status::not_acceptable);
        good(status::proxy_authentication_required);
        good(status::request_timeout);
        good(status::conflict);
        good(status::gone);
        good(status::length_required);
        good(status::precondition_failed);
        good(status::payload_too_large);
        good(status::uri_too_long);
        good(status::unsupported_media_type);
        good(status::range_not_satisfiable);
        good(status::expectation_failed);
        good(status::misdirected_request);
        good(status::unprocessable_entity);
        good(status::locked);
        good(status::failed_dependency);
        good(status::upgrade_required);
        good(status::precondition_required);
        good(status::too_many_requests);
        good(status::request_header_fields_too_large);
        good(status::unavailable_for_legal_reasons);
        good(status::internal_server_error);
        good(status::not_implemented);
        good(status::bad_gateway);
        good(status::service_unavailable);
        good(status::gateway_timeout);
        good(status::http_version_not_supported);
        good(status::variant_also_negotiates);
        good(status::insufficient_storage);
        good(status::loop_detected);
        good(status::not_extended);
        good(status::network_authentication_required);
    }

    void
    run()
    {
        testStatus();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,status);

} // http
} // beast
} // boost

