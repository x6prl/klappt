#include <cstdint>

#include "domain/word_id.h"

enum class WordAction : uint8_t {
	Viewed = 1,
	AddedToLearn = 2,
	Completed = 3,
};

struct WordEvent {
	WordId word_id;
	WordAction action;
	uint64_t timestamp;
};
