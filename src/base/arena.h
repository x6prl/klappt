#pragma once

#include <sys/mman.h>

#include <cinttypes>
#include <cstdlib>

#include <SDL3/SDL_log.h>
#include <type_traits>

// NOTE: should be SIGNED
using Size = int64_t;
#define PRSize PRId64

struct Arena {
	using Offset = Size;
	unsigned char *data{nullptr};
	Offset offset{0};
	Offset objects_size{0};
	Size capacity{0};
	bool is_mmaped{false};

	struct [[nodiscard]] TempGuard {
		Arena *a{nullptr};
		const Offset pos{0};
		const Offset objects{0};

		TempGuard(Arena *_a)
			  : a{_a}, pos{a->offset}, objects{_a->objects_size} {}
		TempGuard(const TempGuard &) = delete;
		TempGuard &operator=(const TempGuard &) = delete;
		~TempGuard() {
			a->offset = pos;
			a->objects_size = objects;
		}
	};

	Arena() = default;
	Arena(Arena &&other) noexcept
		  : data{other.data}, offset{other.offset},
			objects_size{other.objects_size}, capacity{other.capacity},
			is_mmaped{other.is_mmaped} {
		other.data = nullptr;
		other.offset = 0;
		other.objects_size = 0;
		other.capacity = 0;
		other.is_mmaped = false;
	}

	Arena &operator=(Arena &&other) noexcept {
		if (this != &other) {
			data = other.data;
			offset = other.offset;
			objects_size = other.objects_size;
			capacity = other.capacity;
			is_mmaped = other.is_mmaped;

			other.data = nullptr;
			other.offset = 0;
			other.objects_size = 0;
			other.capacity = 0;
			other.is_mmaped = false;
		}
		return *this;
	}
	explicit Arena(Size arena_size) {
		// NOTE: page-aligned (4-16KiB)
		data = static_cast<decltype(data)>(
			  mmap(nullptr, arena_size, PROT_READ | PROT_WRITE,
		           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		if (data == MAP_FAILED) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "mmap failed for arena size %" PRSize, arena_size);
			exit(-8); // TODO: just return false
		}
		capacity = arena_size;
		is_mmaped = true;
		SDL_Log("Created arena of size %" PRSize " KiB", arena_size / 1024);
	}
	explicit Arena(Arena &from, Size arena_size = Size{1} << 19)
		  : data{static_cast<unsigned char *>(from.push(arena_size, 64))},
			capacity{arena_size} {}
	~Arena() {
		if (is_mmaped) {
			if (data && MAP_FAILED != data) {
				munmap(data, capacity);
				SDL_Log("Destroyed arena of size %" PRSize
				        " KiB, used: %" PRSize " KiB",
				        capacity / 1024, objects_size / 1024);
			}
		} else if (data) {
			SDL_Log("Destroyed sub-arena of size %" PRSize
			        " KiB, used: %" PRSize " KiB",
			        capacity / 1024, objects_size / 1024);
		}
	}
	Arena(Arena const &) = delete;
	void operator=(Arena const &) = delete;

	// NOTE: ^2 alignment only! and not more than mempage size
	[[nodiscard]]
	void *push(Size size, Size align = 32) {
		objects_size += size;
		// alignment
		{
			offset += align - 1;
			offset &= ~Offset{align - 1};
		}
		auto ret = data + offset;
		offset += size;
		if (offset > capacity) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Arena: cannot allocate memory"
			             " (request=%" PRSize " aligned_used=%" PRSize
			             " capacity=%" PRSize ")\n",
			             size, offset, capacity);
			exit(-5);
		}
		return static_cast<void *>(ret);
	}

	template <class T>
	[[nodiscard]]
	T *pushN(Size count) {
		static_assert(std::is_trivially_copyable_v<T>,
		              "Arena::pushN only supports trivially copyable types!");
		return static_cast<T *>(
			  push(count * static_cast<Size>(sizeof(T)), alignof(T)));
	}

	void clear() {
		offset = 0;
		objects_size = 0;
	}

	void print_stats() const {
		const auto used = offset;
		const auto free = capacity - used;
		const auto alignment_overhead = used - objects_size;

		SDL_Log("Arena stats: "
		        "capacity=%" PRSize " KiB, "
		        "used=%" PRSize " KiB, "
		        "free=%" PRSize " KiB, "
		        "objects=%" PRSize " KiB, "
		        "alignment_overhead=%" PRSize " B",
		        capacity / 1024, used / 1024, free / 1024, objects_size / 1024,
		        alignment_overhead);
	}

	[[nodiscard]]
	TempGuard guard() {
		return {this};
	}
};
