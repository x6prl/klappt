#include "shuffle.h"

namespace {
static inline uint64_t splitmix64(uint64_t *state) {
	uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
	return z ^ (z >> 31);
}

// unbiased random integer in [0, bound)
static inline size_t rand_bounded(uint64_t *state, Size bound) {
	const uint32_t ubound = static_cast<uint32_t>(bound);
	uint32_t x;
	uint64_t m;
	uint32_t l;

	do {
		x = static_cast<uint32_t>(splitmix64(state));
		m = static_cast<uint64_t>(x) * ubound;
		l = static_cast<uint32_t>(m);
	} while (l < ubound && l < (-ubound % ubound));

	return m >> 32;
}
} // namespace

void shuffle_ints(int *a, Size n, uint64_t *rng_state) {
	if (n <= 1)
		return;

	for (Size i = n - 1; i > 0; --i) {
		Size j = rand_bounded(rng_state, i + 1);
		int tmp = a[i];
		a[i] = a[j];
		a[j] = tmp;
	}
}

Size shuffle_str_views(StrView *a, Size n, uint64_t *rng_state) {
	if (n <= 1)
		return 0;

	Size new_index_of_first_element{0};
	for (Size i = n - 1; i > 0; --i) {
		Size j = rand_bounded(rng_state, i + 1);
		StrView tmp = a[i];
		a[i] = a[j];
		a[j] = tmp;
		if (j == new_index_of_first_element) {
			new_index_of_first_element = i;
		}
	}
	return new_index_of_first_element;
}

void shuffle_str_views_and_indices(StrView *a, Size*b, Size n, uint64_t *rng_state){
	if (n <= 1)
		return;

	for (Size i = n - 1; i > 0; --i) {
		Size j = rand_bounded(rng_state, i + 1);
		StrView tmpa = a[i];
		a[i] = a[j];
		a[j] = tmpa;
		int tmpb = b[i];
		b[i] = b[j];
		b[j] = tmpb;
	}
} 

Size random_num(Size from, Size to, uint64_t* rng_state){
	auto bound = to - from;
	return from + rand_bounded(rng_state, bound);
}
