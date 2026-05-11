#include "str_view.h"

#include <cctype>
#include <cstring>

StrView::operator bool() const { return !!size; }

char &StrView::operator[](Size i) { return const_cast<char *>(data)[i]; }

const char &StrView::operator[](Size i) const { return data[i]; }

bool StrView::operator==(const char other[]) const {
	auto other_size = static_cast<Size>(strlen(other));
	if (size != other_size) {
		return false;
	}
	if (size == 0) {
		return true;
	}
	if (!data || !other) {
		return false;
	}
	return 0 == memcmp(data, other, size);
}

bool StrView::operator!=(const char other[]) const { return !(*this == other); }

bool StrView::operator==(const StrView &other) const {
	if (size != other.size) {
		return false;
	}
	if (size == 0) {
		return true;
	}
	if (!data || !other.data) {
		return false;
	}
	return 0 == memcmp(data, other.data, size);
}

bool StrView::operator!=(const StrView &other) const {
	return !(*this == other);
}

char StrView::first() const { return data[0]; }

char StrView::last() const { return data[size - 1]; }

StrView StrView::copy(Arena &a) const {
	auto copy = a.pushN<char>(size);
	memcpy(copy, data, size);
	return {copy, size};
}

StrView StrView::concat(Arena &arena, const StrView left,
                        const StrView right) {
	auto new_size = left.size + right.size;
	auto new_mem = arena.pushN<char>(new_size);
	memcpy(new_mem, left.data, left.size);
	memcpy(new_mem + left.size, right.data, right.size);
	return {new_mem, new_size};
}

StrView StrView::concat_with(Arena &arena, const StrView left,
                             const StrView right, char delimiter) {
	auto new_size = left.size + right.size + 1;
	auto new_mem = arena.pushN<char>(new_size);
	memcpy(new_mem, left.data, left.size);
	new_mem[left.size] = delimiter;
	memcpy(new_mem + left.size + 1, right.data, right.size);
	return {new_mem, new_size};
}

StrView &StrView::mut_triml() {
	Size i{0};
	for (; i < size && std::isspace(static_cast<unsigned char>(data[i])); ++i) {
	}
	size -= i;
	data += i;

	return *this;
}

StrView &StrView::mut_trimr() {
	for (; size > 0 && std::isspace(static_cast<unsigned char>(data[size - 1]));
	     --size) {
	}
	return *this;
}

StrView &StrView::mut_trim() { return mut_triml().mut_trimr(); }

StrView StrView::triml() const {
	auto copy = *this;
	copy.mut_triml();
	return copy;
}

StrView StrView::trimr() const {
	auto copy = *this;
	copy.mut_trimr();
	return copy;
}

StrView StrView::trim() const {
	auto copy = *this;
	copy.mut_trim();
	return copy;
}

StrView StrView::mut_split_by(char delimiter) {
	for (Size i = 0; i < size; ++i) {
		if (delimiter == data[i]) {
			StrView head{data, i};
			++i;
			size -= i;
			data += i;
			return head;
		}
	}
	auto ret = *this;
	*this = {};
	return ret;
}

StrView StrView::mut_split_by(int (*handler)(int ch)) {
	for (Size i = 0; i < size; ++i) {
		if (handler(static_cast<unsigned char>(data[i]))) {
			StrView head{data, i};
			++i;
			size -= i;
			data += i;
			return head;
		}
	}
	auto ret = *this;
	*this = {};
	return ret;
}

StrView StrView::mut_split() { return mut_split_by(&std::isspace); }
// TODO: rewrite the implementation
Pair<StrView, StrView> StrView::split_by(char delimiter) const {
	auto copy = *this;
	return {copy.mut_split_by(delimiter), copy};
}
// TODO: rewrite the implementation
Pair<StrView, StrView> StrView::split_by(int (*handler)(int ch)) const {
	auto copy = *this;
	return {copy.mut_split_by(handler), copy};
}
// TODO: rewrite the implementation
Pair<StrView, StrView> StrView::split() const {
	auto copy = *this;
	return {copy.mut_split(), copy};
}

StrView StrView::slice(Size from, Size to) const {
	auto start = from < 0 ? 0 : from;
	auto end = (to < 0 || to > size) ? size : to;
	if (start > end) {
		start = end;
	}
	return {data + start, end - start};
}

bool StrView::is_contains(char ch) const {
	for (Size i{0}; i < size; ++i) {
		if (ch == data[i]) {
			return true;
		}
	}
	return false;
}

const char *StrView::find(char ch) const {
	for (Size i{0}; i < size; ++i) {
		if (ch == data[i]) {
			return data + i;
		}
	}
	return end();
}

Clay_String StrView::to_clay_string() const {
	return {false, static_cast<int32_t>(size), data};
}

const char *StrView::begin() const { return data; }

const char *StrView::end() const { return data + size; }

StrView StrView::from_chars(Arena &a, const char *data, int size) {
	auto allocated = static_cast<char *>(a.push(size + 1));
	memcpy(allocated, data, size);
	allocated[size] = '\0';
	return {allocated, size};
}

StrView StrView::from_chars(Arena &a, const char *data) {
	return from_chars(a, data, strlen(data));
}
