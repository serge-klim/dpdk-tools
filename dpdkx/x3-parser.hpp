#pragma once

#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <utility>
#include <tuple>
#include <chrono>
#include <cstdint>

namespace parser {
namespace x3 = boost::spirit::x3;
    
template <typename T, typename Enabled = std::true_type>
struct type;

template <typename T>
constexpr decltype(auto) type_parser() noexcept(noexcept(type<T>{}())) { return type<T>{}(); }

template <>
struct type<bool, std::true_type>
{
   constexpr /*consteval*/ auto operator()() noexcept { return x3::bool_; }
};

template <typename T>
struct type<T, std::bool_constant<std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) == sizeof(std::int8_t)>>
{
    constexpr /*consteval*/ auto operator()() noexcept { return x3::int8; }
};

template <typename T>
struct type<T, std::bool_constant<std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == sizeof(std::uint8_t)>>
{
    constexpr /*consteval*/ auto operator()() noexcept { return x3::uint8; }
};

template <typename T>
struct type<T, std::bool_constant<std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) == sizeof(std::int16_t)>>
{
    constexpr /*consteval*/ auto operator()() noexcept { return x3::int16; }
};

template <typename T>
struct type<T, std::bool_constant<std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == sizeof(std::uint16_t)>>
{
    constexpr /*consteval*/ auto operator()() noexcept { return x3::uint16; }
};

template <typename T>
struct type<T, std::bool_constant<std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) == sizeof(std::int32_t)>>
{
    constexpr /*consteval*/ auto operator()() noexcept { return x3::int32; }
};

template <typename T>
struct type<T, std::bool_constant<std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == sizeof(std::uint32_t)>>
{
    constexpr /*consteval*/ auto operator()() noexcept { return x3::uint32; }
};

template <typename T>
struct type<T, std::bool_constant<std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) == sizeof(std::int64_t)>>
{
    constexpr /*consteval*/ auto operator()() noexcept { return x3::int64; }
};

template <typename T>
struct type<T, std::bool_constant<std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == sizeof(std::uint64_t)>>
{
    constexpr /*consteval*/ auto operator()() noexcept { return x3::uint64; }
};

//template <>
//struct type<int, std::true_type>
//{
//   constexpr /*consteval*/ auto operator()() noexcept { return x3::int_; }
//};
//
//template <>
//struct type<unsigned int, std::true_type>
//{
//   constexpr /*consteval*/ auto operator()() noexcept { return x3::uint_; }
//};

template <>
struct type<double, std::true_type>
{
   constexpr /*consteval*/ auto operator()() noexcept { return x3::double_; }
};

template <>
struct type<float, std::true_type>
{
   constexpr /*consteval*/ auto operator()() noexcept { return x3::float_; }
};

template <typename First, typename... Rest>
struct type<std::tuple<First, Rest...>, std::true_type>
{
   constexpr /*consteval*/ auto operator()() noexcept
   {
      return type_parser<First>() > (... > (',' > type_parser<Rest>()));
   }
};

//constexpr/*consteval*/ void type_parser() noexcept;

//template<>
//constexpr/*consteval*/ auto type_parser<int>() noexcept { return x3::int_; }


} // namespace parser

