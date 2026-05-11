#include <cassert>

#include "base/arr.h"
#include "base/filter_view.h"

static bool non_zero_occupied(const Arr<int, 5> &arr, Size i) {
	return arr[i] != 0;
}

static void test_arr_standard_iteration() {
	Arr<int, 4> arr{{1, 2, 3, 4}};

	auto it = arr.begin();
	assert(it != arr.end());
	assert(*it == 1);
	++it;
	assert(*it == 2);

	int sum = 0;
	for (auto *p = arr.begin(); p != arr.end(); ++p) {
		sum += *p;
	}
	assert(sum == 10);
}

static void test_filter_view_iteration() {
	Arr<int, 5> arr{};
	arr[1] = 7;
	arr[3] = 9;

	auto filtered = filter_view(&arr, arr.size(), non_zero_occupied);
	auto it = filtered.begin();
	auto end = filtered.end();
	assert(it != end);
	assert(*it == 7);
	++it;
	assert(it != end);
	assert(*it == 9);
	++it;
	assert(it == end);
}

int main() {
	test_arr_standard_iteration();
	test_filter_view_iteration();
}
