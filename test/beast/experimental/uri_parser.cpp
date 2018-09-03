//
// Copyright (c) 2018 oneiric (oneiric at i2pmail dot org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/experimental/core/uri/parser.hpp>
#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace uri   {

class uri_parser_test : public unit_test::suite {
public:
  void doParseAbsolute(string_view url,
      string_view scheme = "",
      string_view username = "",
      string_view password = "",
      string_view host = "",
      string_view port = "",
      string_view path = "",
      string_view query = "",
      string_view fragment = "")
  {
    buffer out;
    error_code ec;
    parse_absolute_form(url, out, ec);

    BEAST_EXPECT(!ec);

    BEAST_EXPECT(out.scheme() == scheme);
    BEAST_EXPECT(out.username() == username);
    BEAST_EXPECT(out.password() == password);
    BEAST_EXPECT(out.host() == host);
    BEAST_EXPECT(out.port() == port);
    BEAST_EXPECT(out.path() == path);
    BEAST_EXPECT(out.query() == query);
    BEAST_EXPECT(out.fragment() == fragment);
  }

  void badParse(string_view url)
  {
    buffer out;
    error_code ec;
    parse_absolute_form(url, out, ec);

    BEAST_EXPECT(ec);
  }

  void
  testParseAbsolute()
  {
    // IPv4
    doParseAbsolute("WS://1.1.1.1",              "ws", "", "", "1.1.1.1");
    doParseAbsolute("ws://1.1.1.1",              "ws", "", "", "1.1.1.1");
    doParseAbsolute("wss://1.1.1.1",             "wss", "", "", "1.1.1.1");
    doParseAbsolute("ftp://1.1.1.1",             "ftp", "", "", "1.1.1.1");
    doParseAbsolute("http://1.1.1.1",            "http", "", "", "1.1.1.1");
    doParseAbsolute("https://1.1.1.1",           "https", "", "", "1.1.1.1");
    doParseAbsolute("gopher://1.1.1.1",          "gopher", "", "", "1.1.1.1");
    doParseAbsolute("a://1.1.1.1",               "a", "", "", "1.1.1.1");
    doParseAbsolute("http://a@1.1.1.1",          "http", "a", "", "1.1.1.1");
    doParseAbsolute("http://a:b@1.1.1.1",        "http", "a", "b", "1.1.1.1");
    doParseAbsolute("http://1.1.1.1:80",         "http", "", "", "1.1.1.1", "80");
    doParseAbsolute("http://1.1.1.1:80",         "http", "", "", "1.1.1.1", "80");
    // Empty path
    doParseAbsolute("http://1.1.1.1?a=b",        "http", "", "", "1.1.1.1", "", "", "a=b");
    doParseAbsolute("http://1.1.1.1#a",          "http", "", "", "1.1.1.1", "", "", "", "a");
    doParseAbsolute("http://1.1.1.1:80?a=b",     "http", "", "", "1.1.1.1", "80", "", "a=b");
    doParseAbsolute("http://1.1.1.1:80#a",       "http", "", "", "1.1.1.1", "80", "", "", "a");
    // Non-empty Path
    doParseAbsolute("http://1.1.1.1:80/",        "http", "", "", "1.1.1.1", "80", "/");
    doParseAbsolute("http://1.1.1.1:80/?",       "http", "", "", "1.1.1.1", "80", "/");
    doParseAbsolute("http://1.1.1.1:80/a",       "http", "", "", "1.1.1.1", "80", "/a");
    doParseAbsolute("http://1.1.1.1:80/a/",      "http", "", "", "1.1.1.1", "80", "/a/");
    doParseAbsolute("http://1.1.1.1:80/a/b",     "http", "", "", "1.1.1.1", "80", "/a/b");
    doParseAbsolute("http://1.1.1.1:80/a?b",     "http", "", "", "1.1.1.1", "80", "/a", "b");
    doParseAbsolute("http://1.1.1.1:80/a?b=1",   "http", "", "", "1.1.1.1", "80", "/a", "b=1");
    doParseAbsolute("http://1.1.1.1:80/a#",      "http", "", "", "1.1.1.1", "80", "/a");
    doParseAbsolute("http://1.1.1.1:80/#a",      "http", "", "", "1.1.1.1", "80", "/", "", "a");
    doParseAbsolute("http://1.1.1.1:80/a#a",     "http", "", "", "1.1.1.1", "80", "/a", "", "a");
    doParseAbsolute("http://1.1.1.1:80/a?b=1#",  "http", "", "", "1.1.1.1", "80", "/a", "b=1");
    doParseAbsolute("http://1.1.1.1:80/a?b=1#a", "http", "", "", "1.1.1.1", "80", "/a", "b=1", "a");
    // IPv6
    doParseAbsolute("http://[::1]",              "http", "", "", "::1");
    doParseAbsolute("http://[::1]/a",            "http", "", "", "::1", "", "/a");
    doParseAbsolute("http://[::1]?a",            "http", "", "", "::1", "", "", "a");
    doParseAbsolute("http://[::1]#a",            "http", "", "", "::1", "", "", "", "a");
    doParseAbsolute("http://[::1]:80",           "http", "", "", "::1", "80");
    doParseAbsolute("http://[fe80:1010::1010]",  "http", "", "", "fe80:1010::1010");
    // Registered name
    doParseAbsolute("https://boost.org",          "https", "", "", "boost.org");
    // Path
    doParseAbsolute("h://1/abcdefghijklmnopqrstuvwxyz0123456789", "h", "", "", "1", "", "/abcdefghijklmnopqrstuvwxyz0123456789");
    doParseAbsolute("h://1/-._~!$&\'()*+,=:@",                    "h", "", "", "1", "", "/-._~!$&\'()*+,=:@");
    // Query
    doParseAbsolute("h://1?abcdefghijklmnopqrstuvwxyz0123456789", "h", "", "", "1", "", "", "abcdefghijklmnopqrstuvwxyz0123456789");
    doParseAbsolute("h://1?-._~!$&\'()*+,=:@/?",                  "h", "", "", "1", "", "", "-._~!$&\'()*+,=:@/?");
    // Fragment
    doParseAbsolute("h://1#abcdefghijklmnopqrstuvwxyz0123456789", "h", "", "", "1", "", "", "", "abcdefghijklmnopqrstuvwxyz0123456789");
    doParseAbsolute("h://1#-._~!$&\'()*+,=:@/?",                  "h", "", "", "1", "", "", "", "-._~!$&\'()*+,=:@/?");
    // Potentially malicious request smuggling
    doParseAbsolute("http://boost.org#@evil.com/",                "http", "", "", "boost.org", "", "", "", "@evil.com/");
    doParseAbsolute("http://boost.org/%0D%0ASLAVEOF%20boost.org%206379%0D%0A", "http", "", "", "boost.org", "", "/\r\nSLAVEOF boost.org 6379\r\n");
  }

	void
  testBadParse()
  {
    // Attack test-cases courtesy of Orange Tsai: A New Era Of SSRF Exploiting URL Parser In Trending Programming Languages
    badParse("http://1.1.1.1//");
    badParse("http://1.1.1.1 &@2.2.2.2# @3.3.3.3/");
    badParse("http://127.0.0.1:25/%0D%0AHELO boost.org%0D%0AMAIL FROM: admin@boost.org:25");
    badParse("https://127.0.0.1 %0D%0AHELO boost.org%0D%0AMAIL FROM: admin@boost.org:25");
    badParse("http://127.0.0.1:11211:80");
    badParse("http://foo@evil.com:80@boost.org/");
    badParse("http://foo@127.0.0.1 @boost.org/");
    badParse("http://boost.org/\xFF\x2E\xFF\x2E");
    badParse("http://0\r\n SLAVEOF boost.org 6379\r\n :80");
    badParse("http://foo@127.0.0.1:11211@boost.org:80");
    badParse("http://foo@127.0.0.1 @boost.org:11211");
  }

  void
  run() override
  {
    testParseAbsolute();
    testBadParse();
  }
};

BEAST_DEFINE_TESTSUITE(beast,core,uri_parser);

}  // uri
}  // beast
}  // boost
