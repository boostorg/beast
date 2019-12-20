#include "socks/handshake.hpp"
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/handler.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <ciso646>
#include <string>

namespace beast = boost::beast;
namespace net = beast::net;

namespace socks {
namespace test {

template<class Cond>
void run_until_condition(net::io_context& ioc, Cond cond)
{
  while(not ioc.stopped() and not cond())
  {
    ioc.run_one();
  }
  ioc.restart();
}

struct handshake_v4_test : beast::unit_test::suite 
{

  auto v4_connect_request() -> std::string
  {
    return std::string (
      "\x04" // VN 
      "\x01" // CD
      "\x00\x50" // DSTPORT
      "\x01\x01\x01\x01" // DSTIP
      "bob" // USERID
      "\x00" // NULL
      , 12
    );
  }

  auto v4_success_reponse() -> std::string
  {
    return std::string(
      "\x00" // VN
      "\x5a" // CD
      "\x00\x50" // DSTPORT
      "\x01\x01\x01\x01" // DSTIP
      , 8
    );
  };

  auto v4_reject_reponse() -> std::string
  {
    return std::string(
      "\x00" // VN
      "\x5b" // CD
      "\x00\x50" // DSTPORT
      "\x01\x01\x01\x01" // DSTIP
      , 8
    );
  };

  void testSocks4Protocol()
  {
    boost::beast::net::io_context ioc;

    // successful connect
    {
        beast::test::stream client_stream(ioc);
        beast::test::stream server_stream(ioc);
        beast::test::connect(client_stream, server_stream);

        async_handshake_v4(client_stream, "1.1.1.1", 80, "bob", beast::test::success_handler());

        {
          run_until_condition(ioc, [&]{
            auto buf = server_stream.buffer();
            return buf.size() >= 12;
          });
          auto expected = v4_connect_request();
          auto actual = server_stream.str();
          BEAST_EXPECT(expected == actual);
        }

        net::write(server_stream, net::buffer(v4_success_reponse()));
        beast::test::run(ioc);
    }

    // request rejected
    {
        beast::test::stream client_stream(ioc);
        beast::test::stream server_stream(ioc);
        beast::test::connect(client_stream, server_stream);

        async_handshake_v4(client_stream, "1.1.1.1", 80, "bob", beast::test::fail_handler(socks::error::socks_request_rejected_or_failed));

        {
          run_until_condition(ioc, [&]{
            auto buf = server_stream.buffer();
            return buf.size() >= 12;
          });
          auto expected = v4_connect_request();
          auto actual = server_stream.str();
          BEAST_EXPECT(expected == actual);
        }

        net::write(server_stream, net::buffer(v4_reject_reponse()));
        beast::test::run(ioc);
    }

    // io error mid conversation
    {
        beast::test::stream client_stream(ioc);
        beast::test::stream server_stream(ioc);
        beast::test::connect(client_stream, server_stream);

        async_handshake_v4(client_stream, "1.1.1.1", 80, "bob", beast::test::fail_handler(net::error::eof));

        {
          run_until_condition(ioc, [&]{
            auto buf = server_stream.buffer();
            return buf.size() >= 12;
          });
          auto expected = v4_connect_request();
          auto actual = server_stream.str();
          BEAST_EXPECT(expected == actual);
        }

        server_stream.close();
        beast::test::run(ioc);
    }
  }

  void
  run() override
  {
      testSocks4Protocol();
  }
};
BEAST_DEFINE_TESTSUITE(beast, socks, handshake_v4);

} // namespace test
} // namespace socks
