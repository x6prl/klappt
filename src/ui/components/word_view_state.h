#pragma once

#include "base/str_view.h"
#include "domain/engine.h"
#include "domain/word.h"
#include "domain/word_id.h"
#include "domain/words.h"

struct WordViewState{
	StrView title{};
	WordId word_id{};
	WordRef word_ref{};
	bool has_state{false};
	Word word_copy{};
	Engine::State state_copy{};
};
