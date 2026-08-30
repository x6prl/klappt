#pragma once

#include <sys/mman.h>

#include <cassert>
#include <cinttypes>
#include <cstdlib>

#include <SDL3/SDL_log.h>

// NOTE: should be SIGNED
using Size = int64_t;
#define PRSize PRId64

struct Arena {
	using Offset = Size;
	unsigned char *data{};
	Offset offset{0};
	Offset size_objects{0};
	const Size allocated_size{0};

	struct TempGuard {
		Arena *a;
		const Offset pos{0};
		const Offset objects;

		TempGuard(Arena *_a)
			  : a{_a}, pos{a->offset}, objects{_a->size_objects} {}
		~TempGuard() {
			a->offset = pos;
			a->size_objects = objects;
		}
	};

	Arena(Size arena_size = 1 << 19) : allocated_size{arena_size} {
		data = static_cast<decltype(data)>(
			  mmap(nullptr, arena_size, PROT_READ | PROT_WRITE,
		           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		SDL_Log("Created arena of size %ld KiB", arena_size / 1024);
	}
	~Arena() {
		munmap(data, allocated_size);
		SDL_Log("Destroyed arena of size %ld KiB", allocated_size / 1024);
	}
	Arena(Arena const &) = delete;
	void operator=(Arena const &) = delete;

	// NOTE: ^2 alignment only!
	void *push(Size size, Size align = 32) {
		size_objects += size;
		// alignment
		{
			offset += align - 1;
			offset &= ~Offset{align - 1};
		}
		auto ret = data + offset;
		offset += size;
		if (offset > allocated_size) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Arena: cannot allocate memory"
			             " (request=%lld aligned_used=%lld capacity=%lld)\n",
			             size, offset, allocated_size);
			exit(-5);
		}
		return static_cast<void *>(ret);
	}

	template <class T> T *pushN(Size count) {
		return static_cast<T *>(
			  push(count * static_cast<Size>(sizeof(T)), alignof(T)));
	}

	void clear() {
		offset = 0;
		size_objects = 0;
	}

	void print_stats() const {
		const auto used = offset;
		const auto free = allocated_size - used;
		const auto alignment_overhead = used - size_objects;

		SDL_Log("Arena stats: "
		        "capacity=%" PRSize " KiB, "
		        "used=%" PRSize " KiB, "
		        "free=%" PRSize " KiB, "
		        "objects=%" PRSize " KiB, "
		        "alignment_overhead=%" PRSize " B",
		        allocated_size / 1024, used / 1024, free / 1024,
		        size_objects / 1024, alignment_overhead);
	}
	TempGuard guard() { return {this}; }
};
