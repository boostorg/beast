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

struct handshake_v5_test : beast::unit_test::suite 
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

  void testSocks5Protocol()
  {
    boost::beast::net::io_context ioc;
    beast::test::run(ioc);
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
