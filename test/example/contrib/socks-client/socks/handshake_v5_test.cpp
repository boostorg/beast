#include "socks/handshake.hpp"
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/handler.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <ciso646>
#include <string>
#include <chrono>

namespace beast = boost::beast;
namespace net = beast::net;

namespace socks {
namespace test {

namespace {

  template<class Cond>
  void run_until_condition(net::io_context& ioc, Cond cond)
  {
    while(not ioc.stopped() and not cond())
    {
      ioc.run_one();
    }
    if (ioc.stopped())
      ioc.restart();
  }

} // namespace <private>

struct handshake_v5_test : beast::unit_test::suite 
{
   void testSocks5Protocol()
  {
    boost::beast::net::io_context ioc;

    // successful connect, credentials supplied, ip4 address, no resolve
    {
        beast::test::stream client_stream(ioc);
        beast::test::stream server_stream(ioc);
        beast::test::connect(client_stream, server_stream);

        async_handshake_v5(client_stream, "1.1.1.1", 80, "bob", "password", false, beast::test::success_handler());

        // client sends "hello"
        {
          run_until_condition(ioc, [&]{
            auto buf = server_stream.buffer();
            return buf.size() >= 4;
          });
          auto expected = std::string("\x05" // ver 5
                                      "\x02" // 2 methods offered
                                      "\x00" // method 1: no auth
                                      "\x02" // method 2: username/password
                                      , 4);
          auto actual = server_stream.str();
          BEAST_EXPECT(expected == actual);
        }

        // server responds demanding authentication
        {
          auto hello_response_require_auth = []{ return std::string("\x05\x02", 2); };
          net::write(server_stream, net::buffer(hello_response_require_auth()));
          run_until_condition(ioc, [&]{
            auto buf = server_stream.buffer();
            return buf.size() >= 2;
          });
        }

        // client responds with credentials
        {
          auto expected = std::string("\x01\x03bob\0x08password", 5);
          run_until_condition(ioc, [&]{
            auto buf = server_stream.buffer();
            return buf.size() >= expected.size();
          });
          auto actual = server_stream.str();
          BEAST_EXPECT(expected == actual);
        }

        // server responds ok
        {
          auto response_data = std::string("\x01\x00", 2);
          net::write(server_stream, net::buffer(response_data));
        }

        // client issues connect request
        {
          auto expected = std::string("\x05" // socks 5
                                      "\x01" // connect
                                      "\x00" // reserved
                                      "\x01" // type IP4
                                      "\x01\x01\x01\x01" // ip address
                                      "\x00\x50" // port
                                      "", 10);
          run_until_condition(ioc, [&] {
            auto buf = server_stream.buffer();
            return buf.size() >= expected.size();
          });
          auto actual = server_stream.str();
          BEAST_EXPECT(expected == actual);
        }

        // server responds
        {
          auto response_data = std::string( "\x05" // version
                                            "\x00" // reply (success)
                                            "\x00" // reserved
                                            "\x01"  // address type ip4
                                            "\x02\x02\x02\x02" // remote ip
                                            "\x00\x50" // remote port
                                            , 10);
          net::write(server_stream, net::buffer(response_data));
        }

        beast::test::run(ioc);
    }
  }

  void
  run() override
  {
      testSocks5Protocol();
  }
};
BEAST_DEFINE_TESTSUITE(beast, socks, handshake_v5);

} // namespace test
} // namespace socks
