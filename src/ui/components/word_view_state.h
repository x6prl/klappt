#pragma once

#include "domain/engine.h"
#include "domain/word.h"
#include "domain/word_id.h"
#include "domain/words.h"

struct WordViewState{
	WordId word_id{};
	WordRef word_ref{};
	bool has_learning_state{false};
	Word word_copy{};
	Engine::State learning_state_copy{};
};
