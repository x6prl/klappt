#include "str_view.h"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstring>

namespace {

// Stolen from https://github.com/tsoding/nob.h/blob/main/nob.h
// which is
// Stolen from Jai's Unicode module
static const int8_t bytes_for_utf8[] = {
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	  2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	  4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6,
};

StrView from_numberf_impl(Arena &a, auto val, int precision) {
	static_assert(std::is_floating_point_v<decltype(val)>,
	              "from_numberf is only for floating point types");
	constexpr Size BUF_SIZE = 32;
	auto strbuf = a.pushN<char>(BUF_SIZE);
	std::to_chars_result res;
	res = std::to_chars(strbuf, strbuf + BUF_SIZE, val,
	                    std::chars_format::fixed, precision);
	return {strbuf, static_cast<Size>(res.ptr - strbuf)};
}

StrView from_number_impl(Arena &a, auto val) {
	static_assert(std::is_arithmetic_v<decltype(val)>,
	              "from_number is only for arithmetic types");
	constexpr Size BUF_SIZE = 32;
	auto strbuf = a.pushN<char>(BUF_SIZE);
	std::to_chars_result res;
	if constexpr (std::is_integral_v<decltype(val)>) {
		res = std::to_chars(strbuf, strbuf + BUF_SIZE, val);
		return {strbuf, static_cast<Size>(res.ptr - strbuf)};
	} else {
		return from_numberf_impl(a, val, 2);
	}
}

uint32_t decode_utf8_at(const char *data, Size size, Size i, Size &char_len) {
	uint8_t b0 = static_cast<uint8_t>(data[i]);
	char_len = bytes_for_utf8[b0];

	if (char_len <= 0 || i + char_len > size) {
		char_len = 1;
		return b0;
	}

	if (char_len == 1) {
		return b0;
	} else if (char_len == 2) {
		return ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(data[i + 1]) & 0x3F);
	} else if (char_len == 3) {
		return ((b0 & 0x0F) << 12) |
		       ((static_cast<uint8_t>(data[i + 1]) & 0x3F) << 6) |
		       (static_cast<uint8_t>(data[i + 2]) & 0x3F);
	} else if (char_len == 4) {
		return ((b0 & 0x07) << 18) |
		       ((static_cast<uint8_t>(data[i + 1]) & 0x3F) << 12) |
		       ((static_cast<uint8_t>(data[i + 2]) & 0x3F) << 6) |
		       (static_cast<uint8_t>(data[i + 3]) & 0x3F);
	}
	char_len = 1;
	return b0;
}

bool is_unicode_punctuation(uint32_t cp) {
	// standard ASCII punctuation: !"#$%&'()*+,-./:;<=>?@[\]^_`{|}~
	if (cp < 0x80) {
		return std::ispunct(static_cast<unsigned char>(cp)) != 0;
	}

	// latin-1 Supplement punctuation (¡, §, «, ¶, ·, », ¿)
	if (cp == 0x00A1 || cp == 0x00A7 || cp == 0x00AB || cp == 0x00B6 ||
	    cp == 0x00B7 || cp == 0x00BB || cp == 0x00BF) {
		return true;
	}

	// general (dashes –, —, quotes “”, ‘’, „, ellipses …, bullets •, etc.)
	if (cp >= 0x2010 && cp <= 0x205E) {
		return true;
	}

	// supplemental
	if (cp >= 0x2E00 && cp <= 0x2E7F) {
		return true;
	}

	// CJK symbols and punctuation (、, 。, 「, 」, 《, 》, etc.)
	if (cp >= 0x3001 && cp <= 0x303F) {
		return true;
	}

	// small form variants
	if (cp >= 0xFE50 && cp <= 0xFE6F) {
		return true;
	}

	// fullwidth and halfwidth forms punctuation
	if ((cp >= 0xFF01 && cp <= 0xFF0F) || (cp >= 0xFF1A && cp <= 0xFF20) ||
	    (cp >= 0xFF3B && cp <= 0xFF40) || (cp >= 0xFF5B && cp <= 0xFF65)) {
		return true;
	}

	return false;
}

} // namespace

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

Size StrView::utf8_length() const {
	Size length{0};
	for (Size i{0}; i < size;) {
		auto index = (uint8_t)data[i];
		Size s = bytes_for_utf8[index];
		++length;
		i += s;
	}
	return length;
}

StrView StrView::utf8_to_lowercase_german(Arena &a) const {
	if (!size) {
		return {};
	}
	auto copy = a.pushN<char>(size);
	StrView ret{copy, size};

	for (Size i{0}; i < size;) {
		uint8_t ch0 = static_cast<uint8_t>(data[i]);
		if (ch0 < 0x80) {
			// NOTE: we do not support Turkish 'I' (0x49) -> 'ı' (0xC4 0xB1)
			copy[i] = tolower(ch0);
			i += 1;
			continue;
		}

		// NOTE: then it should contain only an umlaut
		uint8_t ch1 = static_cast<unsigned char>(data[i + 1]);

		auto is_german_upper_second = [](uint8_t b) {
			return b == 0x84 || b == 0x96 || b == 0x9C;
		};
		constexpr uint8_t DE_MARK = 0xC3;

		copy[i] = ch0;
		copy[i + 1] = ch1;

		if (ch0 == DE_MARK && is_german_upper_second(ch1)) {
			copy[i + 1] += 0x20;
		}

		i += 2;
		continue;
	}
	return ret;
}

StrView StrView::utf8_to_lowercase(Arena &a) const {
	if (!size) {
		return {};
	}
	auto copy = a.pushN<char>(size);
	StrView ret{copy, size};

	for (Size i{0}; i < size;) {
		uint8_t ch0 = static_cast<uint8_t>(data[i]);
		if (ch0 < 0x80) {
			// NOTE: we do not support Turkish 'I' (0x49) -> 'ı' (0xC4 0xB1)
			copy[i] = tolower(ch0);
			i += 1;
			continue;
		}
		int char_len = bytes_for_utf8[ch0];
		if (char_len == 2) {
			uint8_t ch1 = static_cast<unsigned char>(
				  data[i + 1]); // hope it is a correct utf8...

			auto is_german_upper_second = [](uint8_t b) {
				return b == 0x84 || b == 0x96 || b == 0x9C;
			};

			// de+tr
			constexpr uint8_t DE_AND_TR_MARK = 0xC3;
			constexpr uint8_t TR_MARK0 = 0xC4;
			constexpr uint8_t TR_MARK1 = 0xC5;
			constexpr uint8_t RU_MARK = 0xD0;
			switch (ch0) {
			case DE_AND_TR_MARK: {
				if (is_german_upper_second(ch1)) {
					copy[i] = static_cast<char>(DE_AND_TR_MARK);
					copy[i + 1] = static_cast<char>(ch1 + 0x20);
				} else {
					copy[i] = ch0;
					copy[i + 1] = ch1;
				}
			} break;
			case TR_MARK0: {
				copy[i] = static_cast<char>(TR_MARK0);
				switch (ch1) {
				case 0x9E: // Ğ -> ğ
					copy[i + 1] = 0x9F;
					break;
				case 0xB0: // İ -> i
				           // NOTE: string become 1 byte shorter than allocated
					copy[i] = 'i';
					ret.size -= 1;
					copy -= 1;
					break;
				default:
					copy[i + 1] = ch1;
				}
			} break;
			case TR_MARK1: {
				copy[i] = static_cast<char>(TR_MARK1);
				if (ch1 == 0x9E) { // Ş -> ş
					copy[i + 1] = 0x9F;
				} else {
					copy[i + 1] = ch1;
				}
			} break;
			case RU_MARK: {
				if (ch1 >= 0x90 && ch1 <= 0x9F) {
					copy[i] = RU_MARK;
					copy[i + 1] = ch1 + 0x20;
				} else if (ch1 >= 0xA0 && ch1 <= 0xAF) {
					copy[i] = 0xD1;
					copy[i + 1] = ch1 - 0x20;
				} else if (ch1 == 0x81) {
					copy[i] = 0xD1;
					copy[i + 1] = 0x91;
				} else {
					copy[i] = static_cast<char>(RU_MARK);
					copy[i + 1] = ch1;
				}
			} break;
			default:
				copy[i] = ch0;
				copy[i + 1] = ch1;
			} // switch

			i += 2;
			continue;
		}
		for (int j = 0; j < char_len && i + j < size; ++j)
			copy[i + j] = data[i + j];
		i += char_len;
	}
	return ret;
}

StrView StrView::utf8_remove_punctuation(Arena &a) const {
	if (!size) {
		return {};
	}

	char *out = a.pushN<char>(size);
	Size out_size = 0;

	for (Size i = 0; i < size;) {
		Size char_len = 0;
		uint32_t cp = decode_utf8_at(data, size, i, char_len);

		if (!is_unicode_punctuation(cp)) {
			for (Size j = 0; j < char_len; ++j) {
				out[out_size++] = data[i + j];
			}
		}

		i += char_len;
	}

	return {out, out_size};
}

StrView StrView::copy(Arena &a) const {
	if (!size) {
		return {};
	}
	auto copy = a.pushN<char>(size);
	memcpy(copy, data, size);
	return {copy, size};
}

bool StrView::is_contains_substr(StrView substr) const {
	if (!substr) {
		return true;
	}
	if (substr.size > size) {
		return false;
	}
	if (substr.size == 1) {
		return is_contains(substr.first());
	}
	const char first = substr.data[0];

	Size end = size - substr.size;

	for (Size i = 0; i <= end; ++i) {
		if (data[i] == first &&
		    memcmp(data + i, substr.data, substr.size) == 0) {
			return true;
		}
	}

	return false;
}

bool StrView::is_contains_punctuation() const {
	for (Size i = 0; i < size; ++i) {
		if (std::ispunct(static_cast<unsigned char>(data[i]))) {
			return true;
		}
	}
	return false;
}

bool StrView::is_contains_punctuation_unicode() const {
	for (Size i = 0; i < size;) {
		Size char_len = 0;
		uint32_t cp = decode_utf8_at(data, size, i, char_len);
		if (is_unicode_punctuation(cp)) {
			return true;
		}
		i += char_len;
	}
	return false;
}

bool StrView::is_starts_with(char ch) const {
	return size > 0 && first() == ch;
}

bool StrView::is_starts_with(StrView pref) const {
	if (!pref) {
		return size == 0;
	}
	if (size >= pref.size) {
		for (Size i{0}; i < pref.size; ++i) {
			if (pref[i] != data[i]) {
				return false;
			}
		}
		return true;
	}
	return false;
}

StrView StrView::concat(Arena &arena, const StrView left, const StrView right) {
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

StrView &StrView::mut_triml() { return mut_triml_by(&std::isspace); }
StrView &StrView::mut_trimr() { return mut_trimr_by(&std::isspace); }
StrView &StrView::mut_trim() { return mut_trim_by(&std::isspace); }

StrView &StrView::mut_triml_by(int (*handler)(int ch)) {
	Size i{0};
	for (; i < size && handler(static_cast<unsigned char>(data[i])); ++i) {
	}
	size -= i;
	data += i;

	return *this;
}
StrView &StrView::mut_trimr_by(int (*handler)(int ch)) {
	for (; size > 0 && handler(static_cast<unsigned char>(data[size - 1]));
	     --size) {
	}
	return *this;
}
StrView &StrView::mut_trim_by(int (*handler)(int ch)) {
	return mut_triml_by(handler).mut_trimr_by(handler);
}

StrView &StrView::mut_chopl() {
	data += 1;
	return *this;
}
StrView &StrView::mut_chopr() {
	size -= 1;
	return *this;
}

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

[[nodiscard]]
DynArr<StrView> StrView::split_all_by(Arena &a, char delimiter) const {
	DynArr<StrView> ret{};

	auto [head, tail] = split_by(delimiter);
	ret.push(a, head);

	for (; tail;) {
		auto p = tail.split_by(delimiter);
		ret.push(a, p.first);
		tail = p.second;
	}

	return ret;
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

const char *StrView::to_cstr(Arena &a) const {
	auto buff = a.pushN<char>(size + 1);
	memcpy(buff, data, size);
	buff[size] = '\0';
	return buff;
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

StrView StrView::from_number(Arena &a, uint64_t val) {
	return from_number_impl(a, val);
}
StrView StrView::from_number(Arena &a, uint32_t val) {
	return from_number_impl(a, val);
}
StrView StrView::from_number(Arena &a, uint16_t val) {
	return from_number_impl(a, val);
}
StrView StrView::from_number(Arena &a, int64_t val) {
	return from_number_impl(a, val);
}
StrView StrView::from_number(Arena &a, int32_t val) {
	return from_number_impl(a, val);
}
StrView StrView::from_number(Arena &a, int16_t val) {
	return from_number_impl(a, val);
}
StrView StrView::from_number(Arena &a, float val) {
	return from_number_impl(a, val);
}
StrView StrView::from_number(Arena &a, double val) {
	return from_number_impl(a, val);
}
StrView StrView::from_number(Arena &a, float val, int precision) {

	return from_numberf_impl(a, val, precision);
}
StrView StrView::from_number(Arena &a, double val, int precision) {

	return from_numberf_impl(a, val, precision);
}

StrView StrView::from_number_hex(Arena &a, uint64_t val) {
	static constexpr char HEX[] = "0123456789abcdef";

	auto data = a.pushN<char>(16);
	for (int i = 15; i >= 0; --i) {
		data[i] = HEX[val & 0x0f];
		val >>= 4;
	}
	return {data, 16};
}
