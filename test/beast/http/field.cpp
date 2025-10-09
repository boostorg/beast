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

        match(field::accept, "Accept");
        match(field::accept_ch, "Accept-CH");
        match(field::accept_charset, "Accept-Charset");
        match(field::accept_encoding, "Accept-Encoding");
        match(field::accept_language, "Accept-Language");
        match(field::accept_patch, "Accept-Patch");
        match(field::accept_post, "Accept-Post");
        match(field::accept_ranges, "Accept-Ranges");
        match(field::accept_signature, "Accept-Signature");
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
        match(field::alt_svc, "Alt-Svc");
        match(field::alt_used, "Alt-Used");
        match(field::authorization, "Authorization");
        match(field::cache_control, "Cache-Control");
        match(field::clear_site_data, "Clear-Site-Data");
        match(field::connection, "Connection");
        match(field::content_digest, "Content-Digest");
        match(field::content_disposition, "Content-Disposition");
        match(field::content_dpr, "Content-DPR");
        match(field::content_encoding, "Content-Encoding");
        match(field::content_language, "Content-Language");
        match(field::content_length, "Content-Length");
        match(field::content_location, "Content-Location");
        match(field::content_range, "Content-Range");
        match(field::content_security_policy, "Content-Security-Policy");
        match(field::content_security_policy_report_only, "Content-Security-Policy-Report-Only");
        match(field::content_type, "Content-Type");
        match(field::cookie, "Cookie");
        match(field::cross_origin_embedder_policy, "Cross-Origin-Embedder-Policy");
        match(field::cross_origin_opener_policy, "Cross-Origin-Opener-Policy");
        match(field::cross_origin_resource_policy, "Cross-Origin-Resource-Policy");
        match(field::date, "Date");
        match(field::deprecation, "Deprecation");
        match(field::device_memory, "Device-Memory");
        match(field::digest, "Digest");
        match(field::dnt, "DNT");
        match(field::dpr, "DPR");
        match(field::etag, "ETag");
        match(field::expect, "Expect");
        match(field::expect_ct, "Expect-CT");
        match(field::expires, "Expires");
        match(field::forwarded, "Forwarded");
        match(field::from, "From");
        match(field::host, "Host");
        match(field::if_match, "If-Match");
        match(field::if_modified_since, "If-Modified-Since");
        match(field::if_none_match, "If-None-Match");
        match(field::if_range, "If-Range");
        match(field::if_unmodified_since, "If-Unmodified-Since");
        match(field::keep_alive, "Keep-Alive");
        match(field::last_modified, "Last-Modified");
        match(field::link, "Link");
        match(field::location, "Location");
        match(field::max_forwards, "Max-Forwards");
        match(field::origin, "Origin");
        match(field::origin_agent_cluster, "Origin-Agent-Cluster");
        match(field::pragma, "Pragma");
        match(field::prefer, "Prefer");
        match(field::preference_applied, "Preference-Applied");
        match(field::priority, "Priority");
        match(field::proxy_authenticate, "Proxy-Authenticate");
        match(field::proxy_authorization, "Proxy-Authorization");
        match(field::proxy_connection, "Proxy-Connection");
        match(field::range, "Range");
        match(field::referer, "Referer");
        match(field::referrer_policy, "Referrer-Policy");
        match(field::refresh, "Refresh");
        match(field::report_to, "Report-To");
        match(field::reporting_endpoints, "Reporting-Endpoints");
        match(field::repr_digest, "Repr-Digest");
        match(field::retry_after, "Retry-After");
        match(field::sec_ch_ua_full_version, "Sec-CH-UA-Full-Version");
        match(field::sec_fetch_dest, "Sec-Fetch-Dest");
        match(field::sec_fetch_mode, "Sec-Fetch-Mode");
        match(field::sec_fetch_site, "Sec-Fetch-Site");
        match(field::sec_fetch_user, "Sec-Fetch-User");
        match(field::sec_purpose, "Sec-Purpose");
        match(field::sec_websocket_accept, "Sec-WebSocket-Accept");
        match(field::sec_websocket_extensions, "Sec-WebSocket-Extensions");
        match(field::sec_websocket_key, "Sec-WebSocket-Key");
        match(field::sec_websocket_protocol, "Sec-WebSocket-Protocol");
        match(field::sec_websocket_version, "Sec-WebSocket-Version");
        match(field::server, "Server");
        match(field::server_timing, "Server-Timing");
        match(field::service_worker, "Service-Worker");
        match(field::service_worker_allowed, "Service-Worker-Allowed");
        match(field::service_worker_navigation_preload, "Service-Worker-Navigation-Preload");
        match(field::set_cookie, "Set-Cookie");
        match(field::set_login, "Set-Login");
        match(field::signature, "Signature");
        match(field::signature_input, "Signature-Input");
        match(field::sourcemap, "SourceMap");
        match(field::strict_transport_security, "Strict-Transport-Security");
        match(field::te, "TE");
        match(field::timing_allow_origin, "Timing-Allow-Origin");
        match(field::tk, "Tk");
        match(field::trailer, "Trailer");
        match(field::transfer_encoding, "Transfer-Encoding");
        match(field::upgrade, "Upgrade");
        match(field::upgrade_insecure_requests, "Upgrade-Insecure-Requests");
        match(field::user_agent, "User-Agent");
        match(field::vary, "Vary");
        match(field::via, "Via");
        match(field::viewport_width, "Viewport-Width");
        match(field::want_content_digest, "Want-Content-Digest");
        match(field::want_repr_digest, "Want-Repr-Digest");
        match(field::warning, "Warning");
        match(field::width, "Width");
        match(field::www_authenticate, "WWW-Authenticate");
        match(field::x_content_type_options, "X-Content-Type-Options");
        match(field::x_dns_prefetch_control, "X-DNS-Prefetch-Control");
        match(field::x_forwarded_for, "X-Forwarded-For");
        match(field::x_forwarded_host, "X-Forwarded-Host");
        match(field::x_forwarded_proto, "X-Forwarded-Proto");
        match(field::x_frame_options, "X-Frame-Options");
        match(field::x_permitted_cross_domain_policies, "X-Permitted-Cross-Domain-Policies");
        match(field::x_powered_by, "X-Powered-By");
        match(field::x_robots_tag, "X-Robots-Tag");
        match(field::x_xss_protection, "X-XSS-Protection");

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
