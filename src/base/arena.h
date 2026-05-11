#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

#include <sys/mman.h>

using Size = int;

// static void memzero(void *data, Size size) {
// 	memset(data, 0, size); // 64MB
// }

struct Arena {
	using Offset = ptrdiff_t;
	uint8_t *data{};
	Offset offset{0};
	Offset size_objects{0};
	const Size allocated_size{0};

	struct TempGuard {
		Arena *a;
		const Offset pos{0};

		TempGuard(Arena *_a) : a{_a}, pos{a->offset} {}
		~TempGuard() { a->offset = pos; }
	};

#ifndef __EMSCRIPTEN__
	Arena(Size arena_size = 1 << 30 /* 1GB */) : allocated_size{arena_size} {
		data = static_cast<decltype(data)>(
			  mmap(nullptr, arena_size, PROT_READ | PROT_WRITE,
		           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		// memset(data, 0, std::min(16 << 20, arena_size));
	}
#else
	Arena(Size arena_size = 16 << 20 /* 16MB */)
		  : allocated_size{arena_size} {
		data = static_cast<decltype(data)>(
			  mmap(nullptr, arena_size, PROT_READ | PROT_WRITE,
		           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		// memset(data, 0, std::min(16 << 20, arena_size));
	}
#endif
	Arena *ptr() { return this; }

	void *push(Size size, Size allign = 32) {
		size_objects += size;
		// allignment
		{
			offset += allign - 1;
			offset &= ~Offset{allign - 1};
		}
		auto ret = data + offset;
		offset += size;
		if (offset > allocated_size) {
			std::fprintf(stderr,
			             "Arena: cannot allocate memory"
			             " (request=%d aligned_used=%td capacity=%d)\n",
			             size, offset, allocated_size);
			exit(-5);
		}
		return static_cast<void *>(ret);
	}

	template <class T> T *pushN(int count) {
		return static_cast<T *>(push(count * sizeof(T)));
	}

	void clear() { offset = 0; }

	TempGuard guard() { return {this}; }
};
