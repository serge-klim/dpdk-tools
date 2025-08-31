#pragma once
#include <atomic>
#include <memory>

namespace workaround {


template <typename T>
using atomic_shared_ptr =
#ifdef __cpp_lib_atomic_shared_ptr
    std::atomic<std::shared_ptr<T>>;
#else
    std::shared_ptr<T>; //libc++ still does not suport std::atomic<std::shared_ptr<T>> 
#endif

template <typename T>
inline std::shared_ptr<T> load(std::atomic<std::shared_ptr<T>> const& ptr) noexcept {
   return ptr.load();
}

template <typename T>
inline std::shared_ptr<T> load(std::shared_ptr<T> const& ptr) {
   return std::atomic_load(&ptr);
}

template <typename T>
inline std::shared_ptr<T> load(std::atomic<std::shared_ptr<T>> const& ptr, std::memory_order mo)  noexcept {
   return ptr.load(mo);
}

template<typename T>
inline std::shared_ptr<T> load(std::shared_ptr<T> const& ptr, std::memory_order mo) {
   return std::atomic_load_explicit(&ptr, mo);
}

template <typename T>
void store(std::atomic<std::shared_ptr<T>>& ptr, std::shared_ptr<T> value) noexcept {
   ptr.store(std::move(value));
}
template <typename T>
void store(std::shared_ptr<T>& ptr, std::shared_ptr<T> value) {
   std::atomic_store(&ptr, std::move(value));
}

template <typename T>
bool compare_exchange_strong(std::atomic<std::shared_ptr<T>>& ptr, std::shared_ptr<T>& expected, std::shared_ptr<T> desired) noexcept {
  return ptr.compare_exchange_strong(expected, std::move(desired));
}
template <typename T>
bool compare_exchange_strong(std::shared_ptr<T>& ptr, std::shared_ptr<T>& expected, std::shared_ptr<T> desired) {
  return std::atomic_compare_exchange_strong(&ptr, &expected, std::move(desired));
}

} // namespace workaround
