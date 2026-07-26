#pragma once

#include <SDL3/SDL_log.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <sys/mman.h>

// NOTE: should be SIGNED
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

	Arena(Size arena_size = 1 << 19) : allocated_size{arena_size} {
		data = static_cast<decltype(data)>(
			  mmap(nullptr, arena_size, PROT_READ | PROT_WRITE,
		           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		printf("Created arena of size %d KiB\n", arena_size / 1024);
	}
	~Arena() {
		munmap(data, allocated_size);
		printf("Destroyed arena of size %d KiB\n", allocated_size / 1024);
	}
	Arena(Arena const &) = delete;
	void operator=(Arena const &) = delete;

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
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
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
