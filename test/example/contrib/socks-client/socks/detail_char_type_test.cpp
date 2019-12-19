#include "socks/detail/char_type.hpp"
#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace socks {
namespace detail {

struct detail_char_type_test : boost::beast::unit_test::suite 
{

  // test a representative sample of inputs to the to_hex function
  void testToHex()
  {
    toHexItem(0, "00");
    toHexItem(255, "ff");
    toHexItem(10, "0a");
    toHexItem(100, "64");
    toHexItem(127, "7f");
    toHexItem(128, "80");
    toHexItem(160, "a0");
  }

  void
  run() override
  {
      testToHex();
  }

private:

  void toHexItem(unsigned char in, string_view expected_out)
  {
    auto out = to_hex(in);
    BEAST_EXPECT(out == expected_out);
  }

};
BEAST_DEFINE_TESTSUITE(beast,socks,detail_char_type);

} // namespace detail
} // namespace socks
