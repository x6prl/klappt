#include <cassert>

#include "base/arr.h"

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

int main() {
	test_arr_standard_iteration();
}
