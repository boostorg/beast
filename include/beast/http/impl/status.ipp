//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_STATUS_IPP
#define BEAST_HTTP_IMPL_STATUS_IPP

namespace beast {
namespace http {
namespace detail {

template<class = void>
string_view
status_to_string(int v)
{
    switch(static_cast<status>(v))
    {
    // 1xx
    case status::continue_:                             return "Continue";
    case status::switching_protocols:                   return "Switching Protocols";
    case status::processing:                            return "Processing";

    // 2xx
    case status::ok:                                    return "OK";
    case status::created:                               return "Created";
    case status::accepted:                              return "Accepted";
    case status::non_authoritative_information:         return "Non-Authoritative Information";
    case status::no_content:                            return "No Content";
    case status::reset_content:                         return "Reset Content";
    case status::partial_content:                       return "Partial Content";
    case status::multi_status:                          return "Multi-Status";
    case status::already_reported:                      return "Already Reported";
    case status::im_used:                               return "IM Used";

    // 3xx
    case status::multiple_choices:                      return "Multiple Choices";
    case status::moved_permanently:                     return "Moved Permanently";
    case status::found:                                 return "Found";
    case status::see_other:                             return "See Other";
    case status::not_modified:                          return "Not Modified";
    case status::use_proxy:                             return "Use Proxy";
    case status::temporary_redirect:                    return "Temporary Redirect";
    case status::permanent_redirect:                    return "Permanent Redirect";

    // 4xx
    case status::bad_request:                           return "Bad Request";
    case status::unauthorized:                          return "Unauthorized";
    case status::payment_required:                      return "Payment Required";
    case status::forbidden:                             return "Forbidden";
    case status::not_found:                             return "Not Found";
    case status::method_not_allowed:                    return "Method Not Allowed";
    case status::not_acceptable:                        return "Not Acceptable";
    case status::proxy_authentication_required:         return "Proxy Authentication Required";
    case status::request_timeout:                       return "Request Timeout";
    case status::conflict:                              return "Conflict";
    case status::gone:                                  return "Gone";
    case status::length_required:                       return "Length Required";
    case status::precondition_failed:                   return "Precondition Failed";
    case status::payload_too_large:                     return "Payload Too Large";
    case status::uri_too_long:                          return "URI Too Long";
    case status::unsupported_media_type:                return "Unsupported Media Type";
    case status::range_not_satisfiable:                 return "Range Not Satisfiable";
    case status::expectation_failed:                    return "Expectation Failed";
    case status::misdirected_request:                   return "Misdirected Request";
    case status::unprocessable_entity:                  return "Unprocessable Entity";
    case status::locked:                                return "Locked";
    case status::failed_dependency:                     return "Failed Dependency";
    case status::upgrade_required:                      return "Upgrade Required";
    case status::precondition_required:                 return "Precondition Required";
    case status::too_many_requests:                     return "Too Many Requests";
    case status::request_header_fields_too_large:       return "Request Header Fields Too Large";
    case status::connection_closed_without_response:    return "Connection Closed Without Response";
    case status::unavailable_for_legal_reasons:         return "Unavailable For Legal Reasons";
    case status::client_closed_request:                 return "Client Closed Request";
    // 5xx
    case status::internal_server_error:                 return "Internal Server Error";
    case status::not_implemented:                       return "Not Implemented";
    case status::bad_gateway:                           return "Bad Gateway";
    case status::service_unavailable:                   return "Service Unavailable";
    case status::gateway_timeout:                       return "Gateway Timeout";
    case status::http_version_not_supported:            return "HTTP Version Not Supported";
    case status::variant_also_negotiates:               return "Variant Also Negotiates";
    case status::insufficient_storage:                  return "Insufficient Storage";
    case status::loop_detected:                         return "Loop Detected";
    case status::not_extended:                          return "Not Extended";
    case status::network_authentication_required:       return "Network Authentication Required";
    case status::network_connect_timeout_error:         return "Network Connect Timeout Error";

    default:
        break;
    }
    return "Unknown Status";
}

} // detail

inline
string_view
obsolete_reason(status v)
{
    return detail::status_to_string(
        static_cast<int>(v));
}

inline
std::ostream&
operator<<(std::ostream& os, status v)
{
    return os << obsolete_reason(v);
}

} // http
} // beast

#endif
