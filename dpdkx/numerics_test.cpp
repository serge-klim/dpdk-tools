#include "dpdkx/detail/numerics.hpp"
#include <boost/test/unit_test.hpp>
#include <limits>
#include <cstdint>

BOOST_AUTO_TEST_SUITE(numerics_test_suite)

BOOST_AUTO_TEST_CASE(dev_mul_test) {
   auto x = mul_div((std::numeric_limits<std::uint64_t>::max)(), 111, (std::numeric_limits<std::uint64_t>::max)());
   BOOST_CHECK_EQUAL(x, 111);
   auto x1 = mul_div((std::numeric_limits<std::uint64_t>::max)() / 2, 111, (std::numeric_limits<std::uint64_t>::max)()/2);
   BOOST_CHECK_EQUAL(x1, 111);
   auto x2 = mul_div((std::numeric_limits<std::uint64_t>::max)() / 4, 111 * 2, (std::numeric_limits<std::uint64_t>::max)() / 2);
   BOOST_CHECK_EQUAL(x2, 110);
   auto x3 = mul_div(1, 111 , 1);
   BOOST_CHECK_EQUAL(x3, 111);
   auto x4 = mul_div(2, 111/2, 1);
   BOOST_CHECK_EQUAL(x4, 110);
}

BOOST_AUTO_TEST_SUITE_END()
