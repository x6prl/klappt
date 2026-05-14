#pragma once

#include "domain/engine.h"
#include "domain/word.h"
#include "ui/components/text_input_state.h"

struct WordEditState {
	WordId word_id{};
	WordType type{WordType::Nil};
	int8_t in_learning_list{};
	int8_t was_learned{};

	Gender gender{Gender::unknown};
	bool adjective_indeclinable{};
	Engine::Mode mode{Engine::Mode::Entire};
	bool has_learning_state{};

	MobileTextInputBuffer lemma{};
	MobileTextInputBuffer plural_suffix{};
	MobileTextInputBuffer third_person{};
	MobileTextInputBuffer praeteritum{};
	MobileTextInputBuffer auxv_and_past_participle{};
	MobileTextInputBuffer comparative{};
	MobileTextInputBuffer superlative{};
	MobileTextInputBuffer translations_raw{};
	MobileTextInputBuffer grammar{};

	bool valid{};
	StrView validation_error{};
};
