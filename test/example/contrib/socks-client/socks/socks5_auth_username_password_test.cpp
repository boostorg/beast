#include "socks/socks5_username_password_authentication.hpp"
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/handler.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <ciso646>
#include <string>

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
    ioc.restart();
  }

} // namespace <private>

struct socks5_auth_username_password_test : beast::unit_test::suite 
{
  void testSuccess()
  {
    boost::beast::net::io_context ioc;

    // successful connect
    {
      beast::test::stream client_stream(ioc);
      beast::test::stream server_stream(ioc);
      beast::test::connect(client_stream, server_stream);

      async_socks5_auth_username_password(client_stream, "bob", "password", beast::test::success_handler());

      {
        run_until_condition(ioc, [&]{
          auto buf = server_stream.buffer();
          return buf.size() >= 13;
        });
        auto expected = std::string(
          "\x01"
          "\x03" "bob"
          "\x08" "password"
          , 14
          );
        auto actual = server_stream.str();
        BEAST_EXPECT(expected == actual);
      }

      {
        auto response = std::string("\x01\x01", 2);
        net::write(server_stream, net::buffer(response));
      }
      beast::test::run(ioc);
    }
  }

  void testFailedToAuth()
  {
    boost::beast::net::io_context ioc;

    // auth failure
    {
      beast::test::stream client_stream(ioc);
      beast::test::stream server_stream(ioc);
      beast::test::connect(client_stream, server_stream);

      async_socks5_auth_username_password(
        client_stream, 
        "bob", "password", 
        beast::test::fail_handler(socks::error::socks_authentication_error)
      );

      {
        run_until_condition(ioc, [&]{
          auto buf = server_stream.buffer();
          return buf.size() >= 13;
        });
        auto expected = std::string(
          "\x01"
          "\x03" "bob"
          "\x08" "password"
          , 14
          );
        auto actual = server_stream.str();
        BEAST_EXPECT(expected == actual);
      }

      {
        auto response = std::string("\x01\x00", 2);
        net::write(server_stream, net::buffer(response));
      }
      beast::test::run(ioc);
    }
  }

  void testCommsError()
  {
    boost::beast::net::io_context ioc;

    // comms error
    {
      beast::test::stream client_stream(ioc);
      beast::test::stream server_stream(ioc);
      beast::test::connect(client_stream, server_stream);

      async_socks5_auth_username_password(
        client_stream, 
        "bob", "password", 
        beast::test::fail_handler(socks::net::error::eof)
      );

      {
        run_until_condition(ioc, [&]{
          auto buf = server_stream.buffer();
          return buf.size() > 0;
        });
      }

      {
        server_stream.close();
      }
      beast::test::run(ioc);
    }
  }

  void testProtocolError()
  {
    boost::beast::net::io_context ioc;

    // protocol error
    {
      beast::test::stream client_stream(ioc);
      beast::test::stream server_stream(ioc);
      beast::test::connect(client_stream, server_stream);

      async_socks5_auth_username_password(
        client_stream, 
        "bob", "password", 
        beast::test::fail_handler(make_error_code(boost::beast::errc::protocol_error))
      );

      {
        run_until_condition(ioc, [&]{
          auto buf = server_stream.buffer();
          return buf.size() >= 13;
        });
        auto expected = std::string(
          "\x01"
          "\x03" "bob"
          "\x08" "password"
          , 14
          );
        auto actual = server_stream.str();
        BEAST_EXPECT(expected == actual);
      }

      {
        // wrong auto protocol version
        auto response = std::string("\x5a\x01", 2);
        net::write(server_stream, net::buffer(response));
      }
      beast::test::run(ioc);
    }
  }

  void testInvalidArguments()
  {
    auto long_username = std::string(256, 'a');
    auto long_password = std::string(256, 'x');

    boost::beast::net::io_context ioc;

    // long username
    {
      beast::test::stream client_stream(ioc);
      beast::test::stream server_stream(ioc);
      beast::test::connect(client_stream, server_stream);

      async_socks5_auth_username_password(
        client_stream, 
        long_username, "password", 
        beast::test::fail_handler(make_error_code(socks::net::error::invalid_argument))
      );

      beast::test::run(ioc);
    }

    // long password
    {
      beast::test::stream client_stream(ioc);
      beast::test::stream server_stream(ioc);
      beast::test::connect(client_stream, server_stream);

      async_socks5_auth_username_password(
        client_stream, 
        "bob", long_password, 
        beast::test::fail_handler(make_error_code(socks::net::error::invalid_argument))
      );

      beast::test::run(ioc);
    }

    // long username and password
    {
      beast::test::stream client_stream(ioc);
      beast::test::stream server_stream(ioc);
      beast::test::connect(client_stream, server_stream);

      async_socks5_auth_username_password(
        client_stream, 
        long_username, long_password, 
        beast::test::fail_handler(make_error_code(socks::net::error::invalid_argument))
      );

      beast::test::run(ioc);
    }

  }

  void
  run() override
  {
      testSuccess();
      testFailedToAuth();
      testCommsError();
      testProtocolError();
      testInvalidArguments();
  }
};
BEAST_DEFINE_TESTSUITE(beast, socks, socks5_auth_username_password);

} // namespace test
} // namespace socks
