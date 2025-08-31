#pragma once
#include <boost/multiprecision/cpp_int.hpp>
#include <type_traits>
#include <cstdint>
#include <cassert>

//#ifdef WIN32
//#include <inttypes.h>
//
//auto mul_div(auto x, auto n, auto d) -> decltype(x * n / d) {
//   using type = decltype(x * n / d);
//   if constexpr (std::is_signed_v<type>) {
//      __int64 h{};
//      auto res  = _mul128(x, n, &h);
//      res = _div128(h, res, d, &h);
//      return static_cast<type>(res);
//   } else {
//      unsigned __int64 h{};
//      auto res = _umul128(x, n, &h);
//      res = _div128(h, res, d, &h);
//      return static_cast<type>(res);
//   }
//}
//
//#else

auto mul_div(auto x, auto n, auto d) -> decltype(x * n / d) {
   using type = decltype(x * n / d);
   using value_type = std::conditional_t<std::is_signed_v<type>, __int128_t, __uint128_t>;
   auto res = static_cast<value_type>(x) * n;
#ifdef WIN32
   auto l = static_cast<std::make_unsigned_t<type>>(res);
   auto mp = boost::multiprecision::uint128_t{static_cast<type>(res >> 64)} << 64;
   mp |= l;
   return static_cast<type>(mp / d);
#else
   return static_cast<type>(res / d);
#endif // WIN32
}
//auto mul_div(auto x, auto n, auto d) -> decltype(x * n / d) {
//   using type = decltype(x * n / d);
//#ifdef WIN32
//   using mp_type = std::conditional_t<std::is_signed_v<type>, boost::multiprecision::int128_t, boost::multiprecision::uint128_t>;
//   auto res = mp_type{x} * mp_type{n};
//   return static_cast<type>(res / d);
//#else
//   using value_type = std::conditional_t<std::is_signed_v<type>, __int128_t, __uint128_t>;
//   auto res = static_cast<value_type>(x) * n;
//   return static_cast<type>(res / d);
//#endif // WIN32
//}












