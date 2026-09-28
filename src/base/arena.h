#pragma once

#include <sys/mman.h>

#include <cinttypes>
#include <cstdlib>

#include <SDL3/SDL_log.h>
#include <type_traits>
#include <unistd.h>

// NOTE: should be SIGNED
using Size = int64_t;
#define PRSize PRId64

struct Arena {
	using Offset = Size;
	unsigned char *data{};
	Offset offset{0};
	Offset size_objects{0};
	const Size allocated_size{0};
	const bool is_mmaped{true};

	struct [[nodiscard]] TempGuard {
		Arena *a;
		const Offset pos{0};
		const Offset objects;

		TempGuard(Arena *_a)
			  : a{_a}, pos{a->offset}, objects{_a->size_objects} {}
		TempGuard(const TempGuard &) = delete;
		TempGuard &operator=(const TempGuard &) = delete;
		~TempGuard() {
			a->offset = pos;
			a->size_objects = objects;
		}
	};

	Arena(Size arena_size = Size{1} << 19) : allocated_size{arena_size} {
		// NOTE: page-aligned (4-16KiB)
		data = static_cast<decltype(data)>(
			  mmap(nullptr, arena_size, PROT_READ | PROT_WRITE,
		           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		if (data == MAP_FAILED) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "mmap failed for arena size %" PRSize, arena_size);
			exit(-8);
		}
		SDL_Log("Created arena of size %" PRSize " KiB", arena_size / 1024);
	}
	Arena(Arena &from, Size arena_size = Size{1} << 19)
		  : data{static_cast<unsigned char *>(
				  from.push(arena_size, sysconf(_SC_PAGESIZE)))},
			allocated_size{arena_size}, is_mmaped{false} {}
	~Arena() {
		if (is_mmaped) {
			if (data && MAP_FAILED != data) {
				munmap(data, allocated_size);
				SDL_Log("Destroyed arena of size %" PRSize
				        " KiB, used: %" PRSize " KiB",
				        allocated_size / 1024, size_objects / 1024);
			}
		} else {
			SDL_Log("Destroyed sub-arena of size %" PRSize
			        " KiB, used: %" PRSize " KiB",
			        allocated_size / 1024, size_objects / 1024);
		}
	}
	Arena(Arena const &) = delete;
	void operator=(Arena const &) = delete;

	// NOTE: ^2 alignment only! and not more than mempage size
	[[nodiscard]]
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
			             " (request=%" PRSize " aligned_used=%" PRSize
			             " capacity=%" PRSize ")\n",
			             size, offset, allocated_size);
			exit(-5);
		}
		return static_cast<void *>(ret);
	}

	template <class T> T *pushN(Size count) {
		static_assert(std::is_trivially_copyable_v<T>,
		              "Arena::pushN only supports trivially copyable types!");
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

	[[nodiscard]]
	TempGuard guard() {
		return {this};
	}
};
