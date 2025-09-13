#pragma once
#include "rte_malloc.h"
#include <memory>

namespace dpdkx {

template <typename T> using rte_memory = std::unique_ptr<T, decltype(&rte_free)>;

template <typename T>
rte_memory<T> allocate_memory(std::size_t size, char const* name = nullptr) {
   auto ptr = rte_malloc(name, size * sizeof(T), 0/*allignof(T)*/);
	if (!ptr)
		throw std::bad_alloc();
   return {static_cast<T*>(ptr),&rte_free};
}

} // namespace dpdkx

