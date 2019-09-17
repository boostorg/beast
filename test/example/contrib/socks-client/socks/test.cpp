#include "socks/uri.hpp"
#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace uri   {
class uri_parser_test : public unit_test::suite {
public:
void ParseUrl(const std::string& url,
    const std::string& scheme = "",
    const std::string& username = "",
    const std::string& password = "",
    const std::string& host = "",
    const std::string& port = "0",
    const std::string& path = "",
    const std::string& query = "",
    const std::string& fragment = "")
{
    socks::uri out(url);

    BEAST_EXPECT(out.scheme() == scheme);
    BEAST_EXPECT(out.username() == username);
    BEAST_EXPECT(out.password() == password);
    BEAST_EXPECT(out.host() == host);
    BEAST_EXPECT(out.port() == port);
    BEAST_EXPECT(out.path() == path);
    BEAST_EXPECT(out.query_string() == query);
    BEAST_EXPECT(out.fragment() == fragment);
}

void badParseUrl(string_view url)
{
    try
    {
        socks::uri u = url;
	boost::ignore_unused(u);
        BEAST_EXPECT(false);
    }
    catch (const std::exception&)
    {
        BEAST_EXPECT(true);
    }
}

void
testParseAbsolute()
{
    ParseUrl("wss://x//", "wss", "", "", "x", "443", "//");
    // IPv4
    ParseUrl("WS://1.1.1.1", "WS", "", "", "1.1.1.1", "80");
    ParseUrl("ws://1.1.1.1", "ws", "", "", "1.1.1.1", "80");
    ParseUrl("wss://1.1.1.1", "wss", "", "", "1.1.1.1", "443");
    ParseUrl("ftp://1.1.1.1", "ftp", "", "", "1.1.1.1", "21");
    ParseUrl("http://1.1.1.1", "http", "", "", "1.1.1.1", "80");
    ParseUrl("https://1.1.1.1", "https", "", "", "1.1.1.1", "443");
    ParseUrl("gopher://1.1.1.1", "gopher", "", "", "1.1.1.1", "70");
    ParseUrl("a://1.1.1.1", "a", "", "", "1.1.1.1");
    ParseUrl("http://a@1.1.1.1", "http", "a", "", "1.1.1.1", "80");
    ParseUrl("http://a:b@1.1.1.1", "http", "a", "b", "1.1.1.1", "80");
    ParseUrl("http://1.1.1.1:80", "http", "", "", "1.1.1.1", "80");
    ParseUrl("http://1.1.1.1:80", "http", "", "", "1.1.1.1", "80");
    // Empty path
    ParseUrl("http://1.1.1.1?a=b", "http", "", "", "1.1.1.1", "80", "", "a=b");
    ParseUrl("http://1.1.1.1#a", "http", "", "", "1.1.1.1", "80", "", "", "a");
    ParseUrl("http://1.1.1.1:80?a=b", "http", "", "", "1.1.1.1", "80", "", "a=b");
    ParseUrl("http://1.1.1.1:80#a", "http", "", "", "1.1.1.1", "80", "", "", "a");
    // Non-empty Path
    ParseUrl("http://1.1.1.1:80/", "http", "", "", "1.1.1.1", "80", "/");
    ParseUrl("http://1.1.1.1:80/?", "http", "", "", "1.1.1.1", "80", "/");
    ParseUrl("http://1.1.1.1:80/a", "http", "", "", "1.1.1.1", "80", "/a");
    ParseUrl("http://1.1.1.1:80/a/", "http", "", "", "1.1.1.1", "80", "/a/");
    ParseUrl("http://1.1.1.1:80/a/b", "http", "", "", "1.1.1.1", "80", "/a/b");
    ParseUrl("http://1.1.1.1:80/a?b", "http", "", "", "1.1.1.1", "80", "/a", "b");
    ParseUrl("http://1.1.1.1:80/a?b=1", "http", "", "", "1.1.1.1", "80", "/a", "b=1");
    ParseUrl("http://1.1.1.1:80/a#", "http", "", "", "1.1.1.1", "80", "/a");
    ParseUrl("http://1.1.1.1:80/#a", "http", "", "", "1.1.1.1", "80", "/", "", "a");
    ParseUrl("http://1.1.1.1:80/a#a", "http", "", "", "1.1.1.1", "80", "/a", "", "a");
    ParseUrl("http://1.1.1.1:80/a?b=1#", "http", "", "", "1.1.1.1", "80", "/a", "b=1");
    ParseUrl("http://1.1.1.1:80/a?b=1#a", "http", "", "", "1.1.1.1", "80", "/a", "b=1", "a");
    // IPv6
    ParseUrl("http://[::1]", "http", "", "", "::1", "80");
    ParseUrl("http://[::1]/a", "http", "", "", "::1", "80", "/a");
    ParseUrl("http://[::1]?a", "http", "", "", "::1", "80", "", "a");
    ParseUrl("http://[::1]#a", "http", "", "", "::1", "80", "", "", "a");
    ParseUrl("http://[::1]:80", "http", "", "", "::1", "80");
    ParseUrl("http://[fe80:1010::1010]", "http", "", "", "fe80:1010::1010", "80");
    // Registered name
    ParseUrl("https://boost.org", "https", "", "", "boost.org", "443");
    // Path
    ParseUrl("h://1/abcdefghijklmnopqrstuvwxyz0123456789", "h", "", "", "1", "0", "/abcdefghijklmnopqrstuvwxyz0123456789");
    ParseUrl("h://1/-._~!$&\'()*+,=:@", "h", "", "", "1", "0", "/-._~!$&\'()*+,=:@");
    // Query
    ParseUrl("h://1?abcdefghijklmnopqrstuvwxyz0123456789", "h", "", "", "1", "0", "", "abcdefghijklmnopqrstuvwxyz0123456789");
    ParseUrl("h://1?-._~!$&\'()*+,=:@/?", "h", "", "", "1", "0", "", "-._~!$&\'()*+,=:@/?");
    // Fragment
    ParseUrl("h://1#abcdefghijklmnopqrstuvwxyz0123456789", "h", "", "", "1", "0", "", "", "abcdefghijklmnopqrstuvwxyz0123456789");
    ParseUrl("h://1#-._~!$&\'()*+,=:@/?", "h", "", "", "1", "0", "", "", "-._~!$&\'()*+,=:@/?");
    // Potentially malicious request smuggling
    ParseUrl("http://boost.org#@evil.com/", "http", "", "", "boost.org", "80", "", "", "@evil.com/");
    ParseUrl("http://boost.org/%0d%0aSLAVEOF%20boost.org%206379%0d%0a", "http", "", "", "boost.org", "80", socks::uri::encodeURI("/\r\nSLAVEOF boost.org 6379\r\n"));
}

void
testBadParse()
{
    badParseUrl("http://1.1.1.1 &@2.2.2.2# @3.3.3.3/");
    badParseUrl("http://127.0.0.1:25/%0D%0AHELO boost.org%0D%0AMAIL FROM: admin@boost.org:25");
    badParseUrl("http://127.0.0.1:11211:80");
    badParseUrl("http://foo@evil.com:80@boost.org/");
    badParseUrl("http://foo@127.0.0.1 @boost.org/");
    badParseUrl("http://boost.org/\xFF\x2E\xFF\x2E");
    badParseUrl("http://0\r\n SLAVEOF boost.org 6379\r\n :80");
    badParseUrl("http://foo@127.0.0.1:11211@boost.org:80");
    badParseUrl("http://foo@127.0.0.1 @boost.org:11211");
    badParseUrl("http://jd:a");
    badParseUrl("http://:12");
    badParseUrl("http://?");
    badParseUrl("http://a:c?");
    badParseUrl("http://a:c@?");
    badParseUrl("file://[:1]:12/");
    badParseUrl("file://[:1]:12");
    badParseUrl("file://[:1]:");
    badParseUrl("file://[:1]:/");
}

void
run() override
{
    testParseAbsolute();
    testBadParse();
}

};

BEAST_DEFINE_TESTSUITE(beast,socks,uri_parser);

}
}
}
