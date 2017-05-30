//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/status.hpp>

#include <beast/unit_test/suite.hpp>

namespace beast {
namespace http {

class status_test
    : public beast::unit_test::suite
{
public:
    void
    testStatus()
    {
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

BEAST_DEFINE_TESTSUITE(status,http,beast);

} // http
} // beast

