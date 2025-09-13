#pragma once

#include <boost/callable_traits.hpp>
#include <tuple>
#include <type_traits>

namespace proxy { inline namespace v0 {

template <typename F>
requires std::is_function_v<F>
struct mem_fn;

namespace detail {

template <auto F>
struct mem_fn_dispatcher;
template <typename T, typename R, typename... Args, R (T::*F)(Args...)>
struct mem_fn_dispatcher<F> {
   using proxy_type = mem_fn<R(Args... args)>;
   static R dispatch(void* obj, Args... args) {
      return (static_cast<T*>(obj)->*F)(std::forward<Args>(args)...);
   }
};

template <typename R, typename T>
struct fn;

template <typename R, typename... Args>
struct fn<R, std::tuple<Args...>> {
   using type = R (*)(void*, Args...);
};

} // namespace detail

template <typename F>
requires std::is_function_v<F>
struct mem_fn {
   using return_type = boost::callable_traits::return_type_t<F>;
   using args = typename boost::callable_traits::args<F>::type;
   using dispatcher_pointer = typename detail::fn<return_type, args>::type;

 public:
   mem_fn() = default;
   constexpr mem_fn(void* obj, dispatcher_pointer dispatcher) noexcept : obj_{obj}, dispatcher_{dispatcher} {}
   template <typename... Args>
   auto operator()(Args&&... args) const {
      return (*dispatcher_)(obj_, std::forward<Args>(args)...);
   }
   constexpr bool operator==(mem_fn const& other) const noexcept {
      return obj_ == other.obj_ && dispatcher_ == other.dispatcher_;
   }
   constexpr bool operator!() const noexcept { return obj_; }

 private:
   void* obj_ = nullptr;
   dispatcher_pointer dispatcher_ = nullptr;
};

template <auto F, typename T>
requires std::is_member_function_pointer_v<decltype(F)>
auto make_proxy(T* obj) {
   using dispatcher = detail::mem_fn_dispatcher<F>;
   return typename dispatcher::proxy_type{
       obj,
       &dispatcher::dispatch};
}

}} // namespace proxy::v0