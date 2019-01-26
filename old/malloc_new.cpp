/*
 * Copyright (C) 2007	Scott Schneider, Christos Antonopoulos
 * 
 * This library is PM_free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301	USA
 */
#include <new>

extern "C" {

#include "pmmalloc.h"

}

/* following declarations are copied and pasted from jePM_malloc_cpp.cpp */
void *operator new(std::size_t size);
void *operator new[](std::size_t size);
void *operator new(std::size_t size, const std::nothrow_t &) noexcept;
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept;
void operator delete(void *ptr) noexcept;
void operator delete[](void *ptr) noexcept;
void operator delete(void *ptr, const std::nothrow_t &) noexcept;
void operator delete[](void *ptr, const std::nothrow_t &) noexcept;

#if __cpp_sized_deallocation >= 201309
/* C++14's sized-delete operators. */
void operator delete(void *ptr, std::size_t size) noexcept;
void operator delete[](void *ptr, std::size_t size) noexcept;
#endif

void*
operator new(std::size_t size) {
	return PM_malloc(size);
}

void*
operator new[](std::size_t size) {
	return PM_malloc(size);
}

void* 
operator new(size_t size, const std::nothrow_t &) noexcept {
	return PM_malloc(size);
}

void*
operator new[](size_t size, const std::nothrow_t &) noexcept {
	return PM_malloc(size);
}

void
operator delete(void *ptr) noexcept {
	PM_free(ptr);
}

void
operator delete[](void *ptr) noexcept {
	PM_free(ptr);
}

void
operator delete(void *ptr, const std::nothrow_t &) noexcept {
	PM_free(ptr);
}

void
operator delete[](void *ptr, const std::nothrow_t &) noexcept {
	PM_free(ptr);
}

#if __cpp_sized_deallocation >= 201309

void
operator delete(void *ptr, std::size_t size) noexcept {
	assert(0&&"not implemented!");
}

void operator delete[](void *ptr, std::size_t size) noexcept {
	assert(0&&"not implemented!");
}

#endif // __cpp_sized_deallocation