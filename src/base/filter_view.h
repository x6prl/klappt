#pragma once

#include "arena.h"

template <class SeqT> struct FilterView {
	using FilterFn = bool (*)(const SeqT &, Size);

	struct Iterator {
		SeqT *seq{};
		FilterFn filter{};
		Size i{};
		Size end{};

		void skip_rejected() {
			for (; i < end; ++i) {
				if (filter(*seq, i)) {
					break;
				}
			}
		}

		auto &operator*() const { return (*seq)[i]; }
		auto *operator->() const { return &(*seq)[i]; }

		Iterator &operator++() {
			++i;
			skip_rejected();
			return *this;
		}

		explicit operator bool() const { return i < end; }

		bool operator==(const Iterator &other) const {
			return seq == other.seq && i == other.i;
		}

		bool operator!=(const Iterator &other) const { return !(*this == other); }
	};

	SeqT *seq{};
	Size size{};
	FilterFn filter{};

	Iterator begin() {
		Iterator it{seq, filter, 0, size};
		it.skip_rejected();
		return it;
	}

	Iterator end() { return {seq, filter, size, size}; }
};

template <class SeqT>
FilterView<SeqT> filter_view(SeqT *seq, Size size,
                             typename FilterView<SeqT>::FilterFn filter) {
	return {seq, size, filter};
}
