//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_FIELD_IPP
#define BEAST_HTTP_IMPL_FIELD_IPP

#include <beast/core/detail/ci_char_traits.hpp>
#include <algorithm>
#include <array>
#include <boost/assert.hpp>

namespace beast {
namespace http {

namespace detail {

class field_strings
{
    using array_type =
        std::array<string_view, 302>;

    array_type v_;

public:
    using const_iterator =
        array_type::const_iterator; 

/*
    From:
    
    https://www.iana.org/assignments/message-headers/message-headers.xhtml
*/
    field_strings()
        : v_({{
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
            "Age",
            "Allow",
            "ALPN",
            "Also-Control",
            "Alt-Svc",
            "Alt-Used",
            "Alternate-Recipient",
            "Alternates",
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
            "Cc",
            "Close",
            "Comments",
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
            "Encoding",
            "Encrypted",
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
            "MIME-Version",
            "MMHS-Acp127-Message-Identifier",
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
            "Obsoletes",
            "Opt",
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
            "SLUG",
            "SoapAction",
            "Solicitation",
            "Status-URI",
            "Strict-Transport-Security",
            "Subject",
            "Summary",
            "Supersedes",
            "Surrogate-Capability",
            "Surrogate-Control",
            "TCN",
            "TE",
            "Timeout",
            "To",
            "Topic",
            "Trailer",
            "Transfer-Encoding",
            "TTL",
            "Upgrade",
            "Urgency",
            "URI",
            "User-Agent",
            "Variant-Vary",
            "Vary",
            "VBR-Info",
            "Via",
            "Want-Digest",
            "Warning",
            "WWW-Authenticate",
            "X-Frame-Options",
            "X400-Content-Identifier",
            "X400-Content-Return",
            "X400-Content-Type",
            "X400-MTS-Identifier",
            "X400-Originator",
            "X400-Received",
            "X400-Recipients",
            "X400-Trace",
            "Xref",
        }})
    {
    }

    std::size_t
    size() const
    {
        return v_.size();
    }

    const_iterator
    begin() const
    {
        return v_.begin();
    }

    const_iterator
    end() const
    {
        return v_.end();
    }
};

inline
field_strings const&
get_field_strings()
{
    static field_strings const fs;
    return fs;
}

template<class = void>
string_view
to_string(field f)
{
    auto const& v = get_field_strings();
    BOOST_ASSERT(static_cast<unsigned>(f) < v.size());
    return v.begin()[static_cast<unsigned>(f)];
}

template<class = void>
field
string_to_field(string_view s)
{
    auto const& v = get_field_strings();
    auto const it = std::lower_bound(
        v.begin(), v.end(), s,
            beast::detail::ci_less{});
    if(it == v.end())
        return field::unknown;
    if(! beast::detail::ci_equal(s, *it))
        return field::unknown;
    BOOST_ASSERT(beast::detail::ci_equal(s,
        to_string(static_cast<field>(it - v.begin()))));
    return static_cast<field>(it - v.begin());
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
    return detail::string_to_field(s);
}

} // http
} // beast

#endif
