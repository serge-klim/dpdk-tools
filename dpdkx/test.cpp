#define BOOST_TEST_MODULE dpdkx_tests
#include "dpdkx/error.hpp"
#include "dpdkx/log.hpp"
#include "dpdkx/utils.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <chrono>


BOOST_AUTO_TEST_SUITE(dpdkx_test_suite)

BOOST_AUTO_TEST_CASE(error_code_test)
{
	auto ok = dpdkx::make_error_code(0);
	BOOST_CHECK_EQUAL(!ok,true);
	BOOST_CHECK_NE(!ok, false);
	auto error_code = dpdkx::make_error_code(101);
	BOOST_CHECK_EQUAL(!error_code,false);
	BOOST_CHECK_NE(!error_code, true);
	BOOST_CHECK(boost::algorithm::starts_with(error_code.message(), "dpdk error : "));
	BOOST_CHECK(boost::algorithm::ends_with(error_code.message(), "[101]"));
	BOOST_CHECK_NE(error_code, ok);
	BOOST_CHECK_EQUAL(error_code, dpdkx::make_error_code(101));
}

BOOST_AUTO_TEST_CASE(log_test)
{
	auto logger = rtexx::rte_logger{ "test" };
	logger.info("test");

	logger.info("extra args", "invalid args", "...");

	logger.info("missing  args {} {}");
}

BOOST_AUTO_TEST_CASE(retry_test)
{
	constexpr static auto default_timeout = std::chrono::milliseconds{ 100 /* 100ms */ };
	auto [timeout, retry] = dpdkx::retry(std::chrono::seconds{ 3 });
	BOOST_CHECK(timeout == default_timeout);
	BOOST_CHECK_EQUAL(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{ 3 }).count() / timeout.count(), retry);
	BOOST_CHECK_EQUAL(30, retry);
}

BOOST_AUTO_TEST_SUITE_END()
